#!/usr/bin/env python3
"""Loopback-only, fixed-target, read-only ProtonLink bridge. Standard library only."""
import argparse
import http.client
import ipaddress
import json
import mimetypes
import re
import socket
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]
ROUTES = {"/bridge/status": "/status", "/bridge/config/device": "/config/device", "/bridge/openapi.json": "/openapi.json", **{"/bridge/config/"+k: "/config/"+k for k in ("pack","wand","smoke")}}
MAX_BODY = 1024 * 1024

def resolve_target(host, port):
    if not re.fullmatch(r"[A-Za-z0-9_.-]{1,253}", host):
        raise ValueError("Use a hostname or IPv4 address only")
    records = socket.getaddrinfo(host, port, socket.AF_INET, socket.SOCK_STREAM)
    addresses = sorted({r[4][0] for r in records})
    if not addresses or any(not (ipaddress.ip_address(a).is_private or ipaddress.ip_address(a).is_link_local) or ipaddress.ip_address(a).is_unspecified for a in addresses):
        raise ValueError("Target must resolve to a local/private IPv4 address")
    return addresses[0]

def handler_for(target, port, allowed_hosts):
    class Handler(BaseHTTPRequestHandler):
        def reply(self, code, data, content_type="application/json"):
            self.send_response(code)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Cross-Origin-Resource-Policy", "same-origin")
            self.end_headers()
            self.wfile.write(data)

        def error_json(self, code, message):
            self.reply(code, json.dumps({"error": message}).encode())

        def do_GET(self):
            if self.headers.get("Host") not in allowed_hosts or self.headers.get("Sec-Fetch-Site") == "cross-site" or self.headers.get("Origin") not in (None, *("http://" + h for h in allowed_hosts)):
                return self.error_json(403, "Local origin required")
            url = urlsplit(self.path)
            if url.query or url.fragment or url.netloc or url.scheme:
                return self.error_json(400, "Queries and absolute URLs not accepted")
            if url.path in ("/bridge/settings/pack", "/bridge/settings/wand", "/bridge/settings/smoke"):
                conn = http.client.HTTPConnection(target, port, timeout=3)
                try:
                    conn.request("GET", url.path[len("/bridge"):], headers={"Connection":"close"})
                    res = conn.getresponse()
                    return self.reply(res.status, json.dumps({"requested":res.status == 200}).encode())
                except (OSError, http.client.HTTPException):
                    return self.error_json(502, "Preference request failed")
                finally:
                    conn.close()
            if url.path in ROUTES:
                conn = http.client.HTTPConnection(target, port, timeout=3)
                try:
                    conn.request("GET", ROUTES[url.path], headers={"Accept": "application/json", "Connection": "close"})
                    res = conn.getresponse()
                    if res.status != 200:
                        return self.error_json(res.status if res.status in (404, 503) else 502, "Upstream HTTP " + str(res.status))
                    raw = res.read(MAX_BODY + 1)
                    if len(raw) > MAX_BODY:
                        return self.error_json(502, "Upstream response too large")
                    value = json.loads(raw)
                    if not isinstance(value, dict):
                        return self.error_json(502, "Expected JSON object")
                    # Avoid exposing configuration secrets; only documented fields needed by the UI.
                    if url.path == "/bridge/config/device":
                        value = {k: value[k] for k in ("songList", "buildDate", "audioVersion", "streamModes") if k in value}
                    self.reply(200, json.dumps(value).encode())
                except (OSError, ValueError, http.client.HTTPException):
                    self.error_json(502, "Upstream unavailable or invalid JSON")
                finally:
                    conn.close()
                return
            if url.path == "/bridge/info":
                return self.reply(200, b'{"mode":"read-only","transport":"http-polling"}')
            if url.path in ("/", "/simulator/"):
                file = ROOT / "simulator/index.html"
            elif re.fullmatch(r"/simulator/[A-Za-z0-9_.-]+\.js", url.path):
                file = ROOT / url.path.lstrip("/")
            elif url.path == "/assets/branding/ProtonLink.png":
                file = ROOT / url.path.lstrip("/")
            else:
                return self.error_json(404, "Not found")
            if not file.is_file():
                return self.error_json(404, "Not found")
            self.reply(200, file.read_bytes(), mimetypes.guess_type(file.name)[0] or "application/octet-stream")

        def denied(self):
            self.error_json(405, "Read-only bridge: GET only")

        do_POST = do_PUT = do_DELETE = do_PATCH = do_OPTIONS = do_HEAD = denied

    return Handler

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, help="Attenuator hostname or local IPv4 address")
    parser.add_argument("--port", type=int, default=8080, help="Loopback UI port")
    parser.add_argument("--upstream-port", type=int, default=80)
    args = parser.parse_args()
    try:
        target = resolve_target(args.host, args.upstream_port)
    except (OSError, ValueError) as exc:
        parser.error("Cannot resolve equipment: " + str(exc))
    allowed = {f"127.0.0.1:{args.port}", f"localhost:{args.port}"}
    server = ThreadingHTTPServer(("127.0.0.1", args.port), handler_for(target, args.upstream_port, allowed))
    print(f"Read-only ProtonLink: http://127.0.0.1:{args.port}/simulator/", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()

if __name__ == "__main__":
    main()
