# Connecting to equipment

Start the Python 3 bridge on a computer connected to the Attenuator’s network.

```sh
python3 scripts/live_bridge.py --host YOUR_ATTENUATOR_HOSTNAME
```

Open http://127.0.0.1:8080/simulator/ and connect in the Equipment connection panel. The bridge resolves the supplied hostname or local IPv4 address at startup. The browser subscribes directly to that address at /ws. Each complete status frame updates the watch and equipment diagram. The bridge reads /status every two seconds, including when the browser or firmware cannot open the socket. Socket retries back off from half a second to fifteen seconds. Disconnecting returns to simulation and cancels retries.

Loss of authoritative HTTP telemetry locks controls. Recovery reads preferences again. Synchronized LED counts, custom spectral colors and brightness drive the diagnostic illustration. LED animation is illustrative. The gateway reports equipment state and configuration, not individual LED pixels.

For hardware testing, the optional --enable-commands flag enables the existing allowlisted PUT routes. Live commands require a fresh status read, an unlocked watch, and the relevant equipment conditions. An acknowledged command triggers another status read for verification. Reconnection does not replay commands. Legacy firmware can omit state fields required to validate particular operations.

[Upstream Attenuator implementation](https://github.com/gpstar81/GPStar-proton-pack/blob/main/source/Attenuator/include/Webhandler.h)
