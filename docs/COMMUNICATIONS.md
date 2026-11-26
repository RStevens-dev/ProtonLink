# Communications console

The console is a browser-local protocol model, not a packet sniffer, and never connects to a real device. All timestamps reflect browser event generation; no real network latency is measured. The Attenuator is the API gateway, not a direct pack network connection.

## Source evidence
- [Attenuator routes](https://github.com/gpstar81/GPStar-proton-pack/blob/main/source/Attenuator/include/Webrouting.h)
- [Attenuator handlers](https://github.com/gpstar81/GPStar-proton-pack/blob/main/source/Attenuator/include/Webhandler.h)
- [Shared response helper](https://github.com/gpstar81/GPStar-proton-pack/blob/main/source/SharedLib/WebRouter/include/WebRouter.h)
- [Web API documentation](https://github.com/gpstar81/GPStar-proton-pack/blob/main/docs/WEB_API_REFERENCE.md)

The real action handlers return JSON {"status":"success"} on acknowledgement, not the mock server's {"success":true}. GET /status and /ws carry equipment status; the latter pushes status when state changes. This model emits a subset of upstream fields; optional sensors and unsupported legacy fields are omitted. The WebSocket handshake is illustrative. No actual socket is opened. Stream naming and exact firmware variants need physical verification.

## Example
1. Watch → Attenuator GET /status
2. Attenuator → Watch 200 OK with current state
3. Watch → Attenuator PUT /music/startstop (only if needed)
4. Attenuator → Watch 200 OK with {"status":"success"}
5. Attenuator → Watch /ws text frame with changed musicPlaying
6. Watch → Attenuator GET /status to reconcile.

A local lock or equipment-policy rejection is not an HTTP error: no request is emitted. Real gateway errors (400, 409 etc.) are documented upstream; the current model does not implement server-side error injection or timing delays. Offline polling produces a simulated timeout, and physical changes made while offline are not delivered until reconnect. Reconnection performs a status read and simulated WebSocket handshake. Polling is optional and off by default. The 1000-event rolling buffer is exportable as JSON.

The shared function suite verifies route selection, raw track IDs, acknowledgement shape, lock guards, no duplicate playback toggles, offline behavior and synchronization. It does not establish hardware compatibility.
