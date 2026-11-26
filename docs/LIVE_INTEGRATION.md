# Live read-only integration

Run from a local checkout of feature/batch-4-live-readonly:

```sh
python scripts/live_bridge.py --host YOUR_ATTENUATOR_HOSTNAME
```

Open http://127.0.0.1:8080/simulator/ on that computer. Connect to the same network as your Attenuator. In Equipment connection choose Connect read-only via local bridge. Hostname is a command-line argument, not committed or saved in public source. If mDNS is unavailable use the pack's local IPv4 address. Python 3 standard library only.

This first increment uses GET /status polling every two seconds and optional GET /config/device and /openapi.json. WebSocket is deferred until actual firmware samples are verified. No command forwarding, no generic proxy, no cross-origin access, loopback bind, fixed private IPv4 destination and GET allowlist. Simulator input is disabled during live mode; raw status is displayed separately. Strict required field mapping may reject older firmware; capture the raw response locally for review rather than substituting invented values. Real telemetry is not yet validated on hardware. Track names are session-local in this increment; no actual audio or writes. Return to simulation restores prior simulated state. Local bridge cannot be used directly from the HTTPS Pages site.

Upstream reference: https://github.com/gpstar81/GPStar-proton-pack/blob/main/docs/WEB_API_REFERENCE.md
