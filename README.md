# ProtonLink

**Wearable field command for Ghostbusters-inspired prop electronics.**

ProtonLink is an independent wrist-mounted controller designed for the Waveshare ESP32-S3 2.06-inch AMOLED touch development board and compatible GPStar proton pack electronics. Its first target is the original-generation pack and Neutrona wand connected through an existing Wi-Fi-enabled GPStar Attenuator. It is not an official GPStar product and does not require GPStar II motion hardware.

> Development status: early prototype. The browser bench runs offline or connects to real equipment through the optional local bridge. Native watch firmware is still planned.

## GPStar version and compatibility

The reference rig is **GPStar I (Gen 1)** with a non-motion Neutrona wand and a Wi-Fi Attenuator. Its captured status reports **Super Hero** mode. The exact pack board revision and installed firmware release numbers for the pack, wand and Attenuator have yet to be recorded. An Attenuator build date identifies that gateway build only. The reported movie year identifies the selected era. See the [captured Gen 1 status](tests/fixtures/legacy-gen1-status.json).

| Function | Gen 1 behavior in ProtonLink |
|---|---|
| Field Lock and telemetry | Unlock opens Industrial Control. The reference capture includes pack and wand power, firing, cable, lid, temperature, track and three volume levels. The bridge polls status and the browser can receive WebSocket updates. |
| Pack power and Manual Vent | Hold START, STOP or VENT on the main panel. Live commands require command-enabled bridge mode, fresh telemetry and eligible pack state. Vent requires the pack to be on. These controls are implemented. Physical vent confirmation is still pending. |
| Three audio faders | System Output adjusts master volume, Discharge Effects adjusts effects **audio volume**, and Auxiliary Audio adjusts music volume. Live adjustments use step commands and reconcile the reported level. |
| Music and track library | Start/stop, pause/resume, previous/next and track selection are implemented. Use the gateway’s reported track range. The reference capture reports IDs 500–514. Command behavior still needs verification on each firmware build. |
| Movie era and stream | Display the reported values in Pack Info. The owner reports that none of the four year changes or stream selection work on this rig. Industrial Control offers neither selector. |
| Smoke and vibration | Dedicated toggles are unavailable on the reference rig. Manual Vent remains a separate command. |
| Configuration and native watch | The bridge can read available pack, wand and smoke preferences. Lighting previews and local presets do not write firmware configuration. Persistent equipment settings, native watch firmware and GPStar II motion features are outside current live support. |

### Bringing up a later revision

1. Record the pack and wand board revisions, their firmware releases and the Attenuator build separately. Connect in read-only mode and capture sanitized status, device metadata, available preferences and OpenAPI paths, if supplied.
2. Compare field names, types, units and missing values against the legacy capture. Update the status decoder and preference discovery for actual differences. Keep missing values unknown. [Status decoder](simulator/live.js) · [Preference discovery](simulator/discovery.js)
3. Check each proposed control against the firmware’s command route, method, state guards and response. Current live era selection requires confirmed ESP32 pack preferences and an advertised year-specific PUT route. Stream selection checks individual routes. Smoke and vibration each require both on and off routes. For a changed API, update the bridge allowlist, command mapping and UI capability checks together. [Bridge](scripts/live_bridge.py) · [Live commands](simulator/commands.js)
4. Add a fixture for that revision alongside the Gen 1 fixture. Test unlock, visible controls, telemetry loss and reconnect on both profiles. With the new hardware connected, verify one eligible command at a time against returned status or an observed physical result before marking it hardware-confirmed. [Live integration](docs/LIVE_INTEGRATION.md)

## Vision

A wearable field instrument, not a miniature desktop dashboard: glanceable visual telemetry while locked, deliberate access to controls, large touch targets for convention use, independent music control, and deeper equipment configuration when needed. The pack remains independently operable through its original controls.

## Live simulator and test bench

[Open the ProtonLink test bench](https://rstevens-dev.github.io/ProtonLink/) (available after GitHub Pages is enabled and the deployment succeeds).

The browser bench uses three fixed viewport columns: a native-resolution 410 × 502 watch preview, a virtual pack and wand, and a tabbed development console (Live output is the default; Tests, Telemetry and Events are alternate views). No document scrolling is needed on desktop; logs and long watch lists scroll within their panels. On narrow displays, use the Watch / Pack / Live output tabs to switch sections. The watch UI is scaled as a single fixed-resolution instrument, rather than reflowing its controls with browser width. Both read the same state model. Watch commands update the pack; physical pack controls update the watch even while Field Lock is active. Offline mode hides watch telemetry and blocks commands while the virtual pack remains operable. Scenario injection covers standby, active, firing, warning, critical, venting and disconnected conditions. The full-width shared communications console shows timestamped simulated HTTP requests, status-code responses, JSON payloads and /ws text frames, with filters, pause/resume, clear, export and optional polling. Physical controls generate push events rather than artificial watch requests. The event log and simulated /status JSON expose changes for debugging. Offline simulation sends no equipment commands and plays no audio.

The function-test panel runs the same isolated assertions as Node CI. Tests never mutate the interactive equipment. The page automatically runs them at startup and can rerun on demand.

### GitHub Pages setup

1. Open repository Settings → Pages.
2. Under Build and deployment, select **Deploy from a branch**.
3. Select **main** and **/ (root)**, then Save.
4. GitHub's native Pages workflow publishes main directly; the root redirects to simulator/. The separate Simulator regression tests workflow runs on pushes and pull requests. Branch publishing is not gated by that test job, so check tests before merging. All publishable repository files are public; never commit credentials.

## Gen 1 equipment fidelity

See [equipment audit](docs/EQUIPMENT.md) for the wiring, physical LED defaults, independent lid/ribbon/wand state, gateway synchronization, stream guards and symbolic serial messages. The simulator is an approximation of the documented Gen 1 system, not a hardware emulator.

## Animated watch instrument UI

The native-resolution watch now includes state-driven reactor rotation and power-cell indication, four distinct lock-face designs, animated alarm borders, a music transport with a decorative reel and playback visualization, vertical segmented mixer faders, graphical stream previews, schematic diagnostics and animated screen transitions. All effects are local visualizations; no invented sensor readings or track progress are presented. Reduced motion respects the browser setting and the watch preference. Existing Field Lock and API command policies remain in place.

## Expanded development laboratory

The bench includes four selectable lock faces, warning screens, native hold-to-confirm controls, swipe navigation, configurable favorites, era accents, ambient dimming, local lighting previews, resizable columns, a simulated Attenuator, API explorer, transport fault injection, watch/pack state comparison and session recording/replay. See [laboratory scope and limitations](docs/LAB.md). Persistent equipment configuration writes are disabled. The lab defaults to offline simulation.

## Quick start (offline)

Clone this repository and serve it locally (ES modules require an HTTP origin):

```sh
git clone https://github.com/RStevens-dev/ProtonLink.git
cd ProtonLink
python -m http.server 8080
```

Open http://localhost:8080/simulator/. No pack, watch, internet connection, credentials or software installation beyond a browser (and optional Python server) is required. Hold the face for two seconds, release, then swipe left within five seconds. On the default Gen 1 profile, unlocking opens Industrial Control. The simulator updates local state only; it never sends commands to equipment. Use the external scenario selector to inject telemetry. Music is represented by state, not audio playback.

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

Industrial Control is the Gen 1 landing screen. Tap the status row for subsystem menus or the track name for the library. Tap the four lights for the PKE monitor, or hold them to engage Field Lock. The bench also provides a mixer, diagnostics, field setup and a lighting workshop preview. The mock profiles expose additional controls for development.

### Field Lock

Boot and wake locked; lock immediately on connection loss. No mutating commands while locked. Long press anywhere on the face for 2 seconds, cancel on movement or early release, then require a distinct left swipe within 5 seconds. Do not unlock on one continuous drag. Re-lock after 30 seconds of inactivity (configurable in future). Locking must not stop music or power down the pack. The lock is an accidental-touch guard, not a physical emergency stop or a security boundary.

Display real reported states, not fictional sensor readings. Decorative waveforms, cyclotron animation and gauge styling do not represent measured radiation or energy. Distinguish offline/unknown from standby. Use shape/text as well as color for important warnings.

### Music and audio

The mixer supports tapping, dragging and keyboard control on all three faders, plus existing +/- buttons. Targets update immediately and settle for 600 ms after the last adjustment (and after release). The simulator models an absolute master command. The live bridge steps master volume toward the target; music and effects send only the required 5% increments, reconciling between steps. A failed/uncertain delivery cancels that batch rather than blindly retrying. Field Lock, disconnect and reset cancel queued edits. The archived upstream pack Configuration.h defines 5% increments for all three channels; actual installed firmware remains to be verified.

Music plays from the existing equipment, not the watch. Track IDs are supplied by the gateway, potentially starting at 500 rather than 1. Compute display ordinal from musicStart; never send the ordinal as a raw API ID. Use configured songList names when available and fall back to numbered tracks.

The start/stop and pause/resume endpoints are toggles. Read fresh status before toggling, skip the command if the desired state is already reached, serialize operations, verify the result, and never blindly retry after a timeout. Live volume controls reconcile incremental commands against reported levels. Present step controls and reconcile reported values instead of pretending that arbitrary slider positions are supported.

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
  simulator/protocol.js
  .github/workflows/pages.yml
  assets/branding/README.md
  .gitignore
```

Native firmware directories are intentionally not represented as implemented code yet. The offline model and the optional live bridge use separate transports.

## Development and tests

Run `node --test simulator/model.test.js` with a recent Node.js version. The shared function suite covers state transitions, guards, physical control synchronization, track boundaries, reconnect and idempotent desired playback state. Run npm test locally or the test panel in the browser. GitHub Actions runs regression tests independently of native branch publishing. Browser gesture and rendering tests remain manual. No claim of hardware or firmware test coverage.

Manual acceptance checklist: locked taps do not mutate equipment; early release or moved hold cancels; expired unlock window relocks; distinct swipe is required; navigation never overlaps; lists scroll; music continues on lock; offline disables controls; unsupported modes are hidden; warnings remain visible; reconnect reads authoritative status; track IDs and names remain correct.

## Roadmap

See [roadmap](docs/ROADMAP.md). The first release is a simulator/design baseline. Native watch validation and per-revision command testing remain open.

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


## Approved animated Field Lock faces

Eleven selectable faces, with **PKE Scanner as the factory default**. See [Field Lock face documentation](docs/FIELD_FACES.md). Electronic Marquee includes a saved full-spectrum color wheel and saturation/brightness controls.


## Track library manager

Use Development Tools → Track library or Music → Library → Manage names on phone / computer. Automatic simulated GPStar `songList` synchronization, local overrides, TXT/CSV/JSON/M3U and numbered audio filename imports, mapping preview and JSON export. See [Track library](docs/TRACK_LIBRARY.md). Browser-local storage only; native Wi-Fi configuration portal and QR provisioning remain future firmware work.


## Industrial Quick Control

Gen 1 opens Industrial Quick Control on unlock. Other profiles can select it under Settings → Main screen. Three themed horizontal volume adjusters, guarded START/STOP/VENT, status area opens menu, tap lights toggles PKE, hold lights for 1.5 seconds engages Field Lock. See [Quick Control](docs/QUICK_CONTROL.md).

Industrial Quick Control also includes an auxiliary transport module with named track readout, start/stop, pause/resume, previous/next and direct track library access.


Development batches use feature branches and pull requests into develop. See [development workflow](docs/DEVELOPMENT.md).


Batch 2 adds field setup, audio presets, favorites, playlists and local pack profiles; see [field functionality](docs/FIELD_SETUP.md).


Batch 3 adds display finishes, adaptive telemetry-driven animations, startup, ambient mode, local session statistics and optional virtual hardware; see [visual identity](docs/IMMERSION.md).


Local bridge with read-only and opt-in command modes: [live integration](docs/LIVE_INTEGRATION.md). Simulation remains the default.
