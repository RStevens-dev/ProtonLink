import importlib.util
import json
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.request import Request, urlopen
from urllib.error import HTTPError
from pathlib import Path

spec = importlib.util.spec_from_file_location("bridge", Path(__file__).resolve().parents[1] / "scripts/live_bridge.py")
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)

class BridgeTest(unittest.TestCase):
    def setUp(self):
        self.calls = []
        calls = self.calls
        class Mock(BaseHTTPRequestHandler):
            def do_GET(self):
                calls.append(self.path)
                raw = json.dumps({"pack":"Idle","songList":"Theme","password":"secret"}).encode()
                self.send_response(200)
                self.end_headers()
                self.wfile.write(raw)
            def do_PUT(self):
                calls.append("PUT "+self.path)
                self.send_response(200)
                self.end_headers()
                self.wfile.write(b'{"status":"success"}')
        self.up = ThreadingHTTPServer(("127.0.0.1",0), Mock)
        self.server = ThreadingHTTPServer(("127.0.0.1",0), bridge.handler_for("127.0.0.1",self.up.server_port,set()))
        # Recreate the handler with the selected loopback origin.
        self.server.RequestHandlerClass = bridge.handler_for("127.0.0.1",self.up.server_port,{f"127.0.0.1:{self.server.server_port}"})
        for s in (self.up,self.server):
            threading.Thread(target=s.serve_forever,daemon=True).start()
        self.url=f"http://127.0.0.1:{self.server.server_port}"
    def tearDown(self):
        for s in (self.server,self.up):
            s.shutdown()
            s.server_close()
    def test_reads_and_redaction(self):
        self.assertEqual(json.load(urlopen(self.url+"/bridge/status"))["pack"],"Idle")
        self.assertEqual(json.load(urlopen(self.url+"/bridge/config/device")),{"songList":"Theme"})
        self.assertEqual(self.calls,["/status","/config/device"])
    def test_writes_and_other_routes_never_forwarded(self):
        for path,method,code in [("/bridge/status","PUT",405),("/bridge/pack/on","GET",404),("/bridge/status?x=1","GET",400)]:
            with self.assertRaises(HTTPError) as ctx:
                urlopen(Request(self.url+path,method=method))
            self.assertEqual(ctx.exception.code,code)
        self.assertEqual(self.calls,[])
    def test_cross_origin_denied(self):
        with self.assertRaises(HTTPError) as ctx:
            urlopen(Request(self.url+"/bridge/status",headers={"Origin":"https://other.example"}))
        self.assertEqual(ctx.exception.code,403)
        self.assertEqual(self.calls,[])

    def test_control_allowlist(self):
        self.server.RequestHandlerClass = bridge.handler_for("127.0.0.1",self.up.server_port,{f"127.0.0.1:{self.server.server_port}"},True)
        headers={"Origin":self.url,"X-ProtonLink-Command":"1"}
        for path in ("/pack/on","/music/select?track=514","/volume/master/set/10"):
            self.assertEqual(json.load(urlopen(Request(self.url+"/bridge"+path,headers=headers,method="PUT")))["status"],"success")
        for path in ("/pack/unknown","/music/select?track=499","/volume/master/set/101"):
            with self.assertRaises(HTTPError):
                urlopen(Request(self.url+"/bridge"+path,headers=headers,method="PUT"))
        self.assertEqual(self.calls,["PUT /pack/on","PUT /music/select?track=514","PUT /volume/master/set/10"])
    def test_preference_probe(self):
        self.assertTrue(json.load(urlopen(self.url+"/bridge/settings/pack"))["requested"])
        self.assertEqual(json.load(urlopen(self.url+"/bridge/config/pack"))["pack"],"Idle")
        self.assertEqual(self.calls,["/settings/pack","/config/pack"])
