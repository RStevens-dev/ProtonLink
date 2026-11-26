# ProtonLink native watch — hardware bring-up

Target: Waveshare ESP32-S3-Touch-AMOLED-2.06, 410 × 502 display. Uses the official managed Waveshare BSP v2 and LVGL 9.5; no custom pin assignments. ESP-IDF 5.4+ (5.x). The 16 MB flash setting follows the vendor demo; confirm actual flash on the purchased board before changing partitions.

## Build / flash

From an activated ESP-IDF environment:

```sh
cd firmware
idf.py set-target esp32s3
idf.py build
idf.py -p YOUR_PORT flash monitor
```

Expected: native Industrial Control panel with three draggable volume sliders, music transport, pack controls and Field Lock. Without Wi-Fi the meters are unknown and commands disabled.

## Direct telemetry (development only)

Run `idf.py menuconfig` and open **ProtonLink connection** to enter the Attenuator Wi-Fi SSID, password and its actual `/status` HTTP URL. Leave SSID blank for offline display testing. Do not commit a generated sdkconfig or share firmware images containing real Wi-Fi credentials. This build-time configuration is temporary; production provisioning must use device-local storage. The watch polls every two seconds and expires stale readings; the default build never sends PUT requests. HTTP is unencrypted, so use a trusted isolated Attenuator network.

## Experimental control (disabled by default)

Enable **Enable experimental GPStar control** in menuconfig only on a controlled test rig. From Field Lock, hold the display for two seconds, release, then swipe left within five seconds. Hold the four cyclotron lights for 1.5 seconds to lock; tap them to toggle PKE Monitor. Hold START, STOP or VENT for one second to request a pack action. Drag a volume slider and release; the request is eligible after 600 ms without another adjustment. The network task uses legacy volume step routes for all three channels, checking fresh reported volume after each successful HTTP request. It stops at the target, on overshoot, no observed progress, lock/link loss, rejection, or after 20 steps; the displayed value returns to reported telemetry. Only one request is active at a time. Music transport uses legacy routes. Commands run through a one-entry queue, fresh status check, and lock epoch; the next status sample is the authoritative display. A request already sent cannot be recalled. No automatic retries. Command acknowledgement and result display still need implementation; controls are not hardware validated. Do not enable experimental control for field use.

## Merge gates and next steps

- CI compilation; then flash on the actual Waveshare board and check boot, display, touch and reboot.
- Verify direct read-only status and reconnect on hardware; replace build-time credentials with device-local provisioning.
- Verify the native Industrial Control layout and touch gestures; add explicit command outcomes, on-device provisioning and test every command on GPStar V1.
- Integrate battery/charging, dimming, sleep/wake and OTA only after board validation.
- Record board revision and installed pack/wand/Attenuator firmware versions.

Reference: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06/tree/main/examples/esp-idf/02_lvgl_demo_v9

## Visual parity
The native Industrial Control panel follows `simulator/quick.js` and the `.quick-*` styles in `simulator/index.html`: industrial metal panels, red LINK wordmark, three channel colors, numeric readouts, fader tracks, scale marks, green track readout and hazard separator. Native LVGL now uses segmented indicators, gradient handles, diagonal hazard marks, a transport panel, and switch lamps. It implements Field Lock gestures, PKE monitor navigation and a paginated track database using the reported musicStart/musicEnd range. The radar is a simplified visualization; native track names currently use the browser's ordinal fallback, not imported or synchronized titles. Full subsystem menus, configurable channel order and remaining face variants are not yet ported. Visual and physical touch parity require manual validation.

## Native macOS simulator

The desktop target compiles the exact same `main/ui.c` and `main/control.c` as the ESP32, using LVGL 9.5 and SDL2 with simulated telemetry. It never connects to equipment. Download the appropriate Apple Silicon (arm64) or Intel (x86_64) artifact from the Native desktop simulator GitHub Actions workflow after it passes. Extract both ZIP layers if necessary, keep the executable in the extracted folder, and run from Terminal:

```sh
cd /path/to/dist
./ProtonLinkDesktop
```

The artifacts are ad-hoc signed development executables, not notarized Mac apps. Alternatively build locally with Homebrew:

```sh
brew install cmake
cmake -S firmware/desktop -B build-desktop
cmake --build build-desktop --parallel
./build-desktop/ProtonLinkDesktop
```

Hold the Field Lock screen for two seconds, release, then swipe left with the mouse within five seconds. Tap the four dots to toggle PKE Monitor; hold them to lock. Drag sliders and release. Press 0 for link loss, 1 for normal link, 2 for firing, 3 for critical/lockout, Escape to quit. Simulated vent lasts four seconds. The desktop does not model network latency or physical display/touch/battery characteristics.

SDL2 is fetched at build time and linked statically. CI tests both the original executable and the signed, extracted ZIP. For a separate bounded LVGL pool test:

```sh
cmake -S firmware/desktop -B build-bounded -DPROTONLINK_BOUNDED_MEMORY=ON
cmake --build build-bounded --parallel
./build-bounded/ProtonLinkDesktop --smoke
```

The 2 MiB pool is a provisional UI regression budget, not a model of total ESP32 memory or DMA requirements. Smoke testing exercises LVGL pointer hold/swipe, link-loss locking and simulated telemetry states and reports LVGL pool usage; it does not replace physical touch and network tests. Watch startup logs report internal RAM and PSRAM free, largest free block and historical minimum free at display/UI/network initialization. Confirm actual memory budgets on hardware.
