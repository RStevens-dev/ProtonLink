# Attenuator integration reference

Source: [Webrouting.h](https://github.com/gpstar81/GPStar-proton-pack/blob/main/source/Attenuator/include/Webrouting.h), [index.js](https://github.com/gpstar81/GPStar-proton-pack/blob/main/source/Attenuator/assets/index.js), [mock status](https://github.com/gpstar81/GPStar-proton-pack/blob/main/scripts/mock/status.json). Routes are candidate capabilities, firmware dependent.

## Read-only
- GET /status: equipment status JSON
- GET /config/device: songList, streamModes, buildDate, audioVersion, audioCorrupt, audioOutdated
- GET /config/pack, /config/wand, /config/smoke: configuration
- GET /openapi.json: available on newer firmware
- GET /wifi/status, /wifi/settings, /wifi/networks: optional diagnostics; redact sensitive fields
- WebSocket /ws: state updates, potentially plain text; handle malformed frames, heartbeat, reconnect and polling fallback.

## Mutating endpoints (PUT unless noted)

| Purpose | Path |
|---|---|
| Pack power | /pack/on, /pack/off |
| Music toggle | /music/startstop |
| Pause toggle | /music/pauseresume |
| Skip | /music/next, /music/prev |
| Select raw track ID | /music/select?track=N |
| Repeat | /music/loop/one, /music/loop/all |
| Shuffle | /music/shuffle/on, /music/shuffle/off |
| Master | /volume/master/set/N, /volume/master/up, /volume/master/down |
| Effects | /volume/effects/up, /volume/effects/down |
| Music volume | /volume/music/up, /volume/music/down |
| Mute | /volume/mute, /volume/unmute |
| Theme | /pack/theme/1984, /1989, /2021, /2024 (same prefix) |
| Stream | /pack/stream/proton, /stasis, /slime, /meson, /spectral, /holiday/halloween, /holiday/christmas, /spectral/custom (same prefix) |
| Smoke | /pack/smoke/on, /pack/smoke/off |
| Vibration | /pack/vibration/on, /pack/vibration/off |
| Rotation | /pack/cyclotron/clockwise, /pack/cyclotron/counterclockwise |
| Vent | /pack/vent |
| Warning attenuation | /pack/attenuate (advanced only) |
| Cable alarm | /pack/cable/on, /pack/cable/off (advanced only) |
| Lockout | /pack/lockout, /pack/restart (advanced only) |
| Persistent save | /eeprom/all, /eeprom/pack, /eeprom/wand |
| Configuration | POST /config/device/save, /config/pack/save, /config/wand/save, /config/smoke/save |

Do not use destructive reset or network modification routes in the first controller. No direct remote power-level adjustment for the baseline wand.

## Status fields
pack, wand, wandPower, mode, modeID, theme, themeID, switch, power, safety, firing, ramping, crossedStreams, cable, cyclotron, cyclotronLid, temperature, smoke, vibration, direction, lockout, canChangeStream, wandMode, vgMode, musicPlaying, musicPaused, musicLooping, musicShuffled, musicCurrent, musicStart, musicEnd, volMaster, volEffects, volMusic, volMuted. Optional hardware fields include battVoltage, packTempC, packTempF, wandAmps. Do not assume temperature/voltage/current availability. Preserve unknown values and use raw track IDs.

## Transport requirements
Use bounded request timeouts; never retry non-idempotent toggles blindly. A successful HTTP response does not prove equipment state changed. Refresh status, handle rejection, and keep controls disabled until synchronized after reconnect. Serialize commands, reject stale state, validate arguments and enforce lock in the command layer. OpenAPI discovery is read-only. The mock server upstream acknowledges action requests but does not implement all state transitions; ProtonLink's local simulator models a subset explicitly.
