# ProtonLink

**Wearable field command for Ghostbusters-inspired prop electronics.**

ProtonLink is an independent wrist-mounted controller designed for the Waveshare ESP32-S3 2.06-inch AMOLED touch development board and compatible GPStar proton pack electronics. Its first target is the original-generation pack and Neutrona wand connected through an existing Wi-Fi-enabled GPStar Attenuator. It is not an official GPStar product and does not require GPStar II motion hardware.

> Development status: early prototype. The browser simulator is a local model, not working watch firmware. No physical hardware tests have been performed. Do not flash this repository onto a watch.

## Vision

A wearable field instrument, not a miniature desktop dashboard: glanceable visual telemetry while locked, deliberate access to controls, large touch targets for convention use, independent music control, and deeper equipment configuration when needed. The pack remains independently operable through its original controls.

## Live simulator and test bench

[Open the ProtonLink test bench](https://rstevens-dev.github.io/ProtonLink/) (available after GitHub Pages is enabled and the deployment succeeds).

The browser bench pairs a virtual watch with a virtual pack and wand in one responsive layout. Both read the same state model. Watch commands update the pack; physical pack controls update the watch even while Field Lock is active. Offline mode hides watch telemetry and blocks commands while the virtual pack remains operable. Scenario injection covers standby, active, firing, warning, critical, venting and disconnected conditions. The event log and simulated /status JSON expose changes for debugging. No network commands or audio playback occur.

The function-test panel runs the same isolated assertions as Node CI. Tests never mutate the interactive equipment. The page automatically runs them at startup and can rerun on demand.

### GitHub Pages setup

1. Open repository Settings → Pages.
2. Under Build and deployment, select GitHub Actions as the source.
3. In Actions, run **Test and deploy simulator** if the initial push did not deploy.
4. Subsequent pushes to main run tests and deploy only when tests pass. Pull requests run tests without publishing. The Pages artifact contains only the static simulator, not credentials or real transport code.

## Quick start (offline)

Clone this repository and serve it locally (ES modules require an HTTP origin):

```sh
git clone https://github.com/RStevens-dev/ProtonLink.git
cd ProtonLink
python -m http.server 8080
```

Open http://localhost:8080/simulator/. No pack, watch, internet connection, credentials or software installation beyond a browser (and optional Python server) is required. Hold the face for two seconds, release, then swipe left within five seconds. Select a command-center tile. The simulator updates local state only; it never sends commands to equipment. Use the external scenario selector to inject telemetry. Music is represented by state, not audio playback.

## Hardware target

| Component | Target |
|---|---|
| Watch | Waveshare ESP32-S3 Touch AMOLED 2.06 |
| MCU | ESP32-S3R8 |
| Display | 410 × 502 touch AMOLED |
| Connectivity | 2.4 GHz Wi-Fi |
| Pack | Original-generation GPStar, exact revision pending |
| Wand | Non-motion generation |
| Gateway | Existing GPStar Attenuator |
| UI framework (planned) | LVGL, version matched to board examples |
| Firmware (planned) | C++ using ESP-IDF or Arduino/PlatformIO after driver validation |
| Storage (planned) | NVS for connection profiles and preferences |
| Voice (optional) | ESP-SR fixed vocabulary, off by default |

[Waveshare board guide](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.06). Board drivers, power-management IC, audio pins, sleep behavior and memory configuration must be verified against the exact purchased board before committing to a toolchain.

## Connection architecture

```text
ProtonLink watch
     |  local Wi-Fi / HTTP / WebSocket
GPStar Attenuator
     |  existing equipment communication
Original pack + wand
```

The watch stores a user-selected SSID and gateway separately. Resolve the gateway hostname via mDNS where supported, with a configurable IP fallback. Never hardcode passwords, personal hostnames or assume all installations use one address. No cloud service is needed for the core controller. The watch must not expose a public control server or require internet access. HTTP is local and may be unencrypted; use a trusted private network.

At connection: fetch status, discover supported endpoints via OpenAPI when available, load configuration metadata, subscribe to /ws if supported, and reconcile state after reconnect. Older releases may not expose OpenAPI or identical fields. Do not probe mutating routes to discover capabilities.

## Application organization

| Screen | Role | Status |
|---|---|---|
| Field Lock | Read-only clock, gauge, link, music, alarms | Simulated |
| Command Center | Large shortcut tiles | Simulated |
| Pack | Power, theme, stream, reported power | Simulated |
| Music | Play/stop, pause/resume, skip, repeat, shuffle | Simulated |
| Library | Track selection; configured names planned | Simulated |
| Mixer | Master, effects, music, mute | Simulated |
| Streams | Stream selector; capability filtering planned | Simulated |
| Themes | 1984, 1989, 2021, 2024 | Simulated |
| Effects | Smoke, vibration, direction, vent | Simulated; guarded |
| Diagnostics | Pack/wand, cable, cyclotron, firing, alarm state | Simulated |
| Presets | Coordinated changes with explicit review | Planned |
| Workshop | Lighting, pack, wand and Attenuator configuration | Planned |
| Settings | Network, favorites, face, lock, voice | Planned |

Everyday screens are reachable via direct shortcuts; horizontal swipe navigation is planned beyond the unlock gesture. Secondary pages should have a back action; the current prototype uses Home, Music and Pack shortcuts instead. Reserve header/content/footer regions and use vertical scrolling for lists; no controls may overlap navigation. Target large touch areas and test long labels at native resolution.

### Field Lock

Boot and wake locked; lock immediately on connection loss. No mutating commands while locked. Long press anywhere on the face for 2 seconds, cancel on movement or early release, then require a distinct left swipe within 5 seconds. Do not unlock on one continuous drag. Re-lock after 30 seconds of inactivity (configurable in future). Locking must not stop music or power down the pack. The lock is an accidental-touch guard, not a physical emergency stop or a security boundary.

Display real reported states, not fictional sensor readings. Decorative waveforms, cyclotron animation and gauge styling do not represent measured radiation or energy. Distinguish offline/unknown from standby. Use shape/text as well as color for important warnings.

### Music and audio

Music plays from the existing equipment, not the watch. Track IDs are supplied by the gateway, potentially starting at 500 rather than 1. Compute display ordinal from musicStart; never send the ordinal as a raw API ID. Use configured songList names when available and fall back to numbered tracks.

The start/stop and pause/resume endpoints are toggles. Read fresh status before toggling, skip the command if the desired state is already reached, serialize operations, verify the result, and never blindly retry after a timeout. Master volume supports absolute values; music/effects use incremental commands. Present step controls and reconcile reported values instead of pretending that arbitrary slider positions are supported.

### Pack and effects

Provide explicit confirmation for power changes, manual vent and enabling smoke. Gate commands using current reported pack/wand/firing/ramping/cyclotron and stream eligibility state, following the upstream UI and firmware handlers. Never automatically clear warnings, activate smoke or force an effect on reconnect. Keep alarm simulation and recovery controls separate from everyday actions. Missing telemetry means unknown, not safe. Firmware remains authoritative and physical controls remain available.

### Workshop and presets (planned)

Read supported configuration only. Candidate controls: cyclotron lid and inner lighting hue/saturation/brightness, power-cell hue/brightness/direction, LED counts, startup and idle sounds, wand bargraph and Attenuator buzzer/vibration. Respect pack-off and wand-off requirements. Read/merge/validate full configurations before sending a save request; do not assume partial writes are safe. Preview changes, confirm persistent EEPROM writes separately, and do not expose factory reset in everyday operation. Presets are watch-managed collections of commands, not native GPStar presets; verify each step and report partial failure.

## API compatibility

See [API reference](docs/API.md) for upstream routes and fields. These are source-verified candidates, not a claim that the owner's installed firmware supports all of them. GPStar I should be the baseline. No motion tracking, gyroscope calibration or remote wand-level control is required. GPStar II-specific fields in shared mock data are optional.

## Repository layout

```text
ProtonLink/
  README.md
  docs/API.md
  docs/ARCHITECTURE.md
  docs/ROADMAP.md
  docs/BRANDING.md
  simulator/index.html
  simulator/model.js
  simulator/model.test.js
  simulator/cases.js
  .github/workflows/pages.yml
  assets/branding/README.md
  .gitignore
```

Native firmware directories are intentionally not represented as implemented code yet. The simulator is a separate transport-free model to prevent accidental hardware control. A real adapter will implement a common command interface later.

## Development and tests

Run `node --test simulator/model.test.js` with a recent Node.js version. The shared function suite covers state transitions, guards, physical control synchronization, track boundaries, reconnect and idempotent desired playback state. Run npm test locally or the test panel in the browser. GitHub Actions gates Pages deployment on passing tests. Browser gesture and rendering tests remain manual. No claim of hardware or firmware test coverage.

Manual acceptance checklist: locked taps do not mutate equipment; early release or moved hold cancels; expired unlock window relocks; distinct swipe is required; navigation never overlaps; lists scroll; music continues on lock; offline disables controls; unsupported modes are hidden; warnings remain visible; reconnect reads authoritative status; track IDs and names remain correct.

## Roadmap

See [roadmap](docs/ROADMAP.md). The first release is a simulator/design baseline. Physical validation is deferred until the watch is available and the owner chooses to power the pack.

## Branding and attribution

ProtonLink has its own industrial badge identity: red/orange caged beacon, yellow conduit, steel enclosure, white/red lettering and yellow/black hazard stripes. Final artwork is supplied by the project owner; the original PNG and SVG still need to be uploaded to assets/branding/. See [branding notes](docs/BRANDING.md) for the asset workflow and material sources.

- [Original GPStar source](https://github.com/gpstar81/GPStar-proton-pack) (archived repository used for research)
- [GPStar current source](https://github.com/GPStarTechnologies/GPStar-Core-Source)
- [Attenuator routes](https://github.com/gpstar81/GPStar-proton-pack/blob/main/source/Attenuator/include/Webrouting.h)
- [Upstream web API](https://github.com/gpstar81/GPStar-proton-pack/blob/main/docs/WEB_API_REFERENCE.md)
- [Upstream offline mock documentation](https://github.com/gpstar81/GPStar-proton-pack/blob/main/scripts/mock/README.md)
- [Waveshare documentation](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.06)

GPStar and Ghostbusters names identify compatibility and inspiration only. No affiliation or endorsement is implied. Upstream GPStar code is GPL-3.0; this initial simulator is independently written and does not vendor its firmware. Project licensing is pending owner selection; no blanket license is implied for third-party artwork or marks. Consult upstream licenses before copying code or assets.

## Contributing

Open issues for layout, compatibility, feature proposals and reproducible bugs. Include simulator scenario, expected/actual behavior, firmware version if known and sanitized status fields. Do not publish credentials, private network details or device configuration secrets. Hardware changes require separate validation. Keep simulation and real transport explicitly separated.
