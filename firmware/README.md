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

Enable **Enable experimental GPStar control** in menuconfig only on a controlled test rig. Hold the lock button to unlock; tap to lock. Hold START, STOP or VENT to request a pack action. Drag a volume slider and release to request its target. The network task uses legacy volume step routes for all three channels, checking fresh reported volume after each successful HTTP request. It stops at the target, on overshoot, no observed progress, lock/link loss, rejection, or after 20 steps; the displayed value returns to reported telemetry. Only one request is active at a time. Music transport uses legacy routes. Commands run through a one-entry queue, fresh status check, and lock epoch; the next status sample is the authoritative display. A request already sent cannot be recalled. No automatic retries. Command acknowledgement and result display still need implementation; controls are not hardware validated. Do not enable experimental control for field use.

## Merge gates and next steps

- CI compilation; then flash on the actual Waveshare board and check boot, display, touch and reboot.
- Verify direct read-only status and reconnect on hardware; replace build-time credentials with device-local provisioning.
- Verify the native Industrial Control layout and touch gestures; add explicit command outcomes, on-device provisioning and test every command on GPStar V1.
- Integrate battery/charging, dimming, sleep/wake and OTA only after board validation.
- Record board revision and installed pack/wand/Attenuator firmware versions.

Reference: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06/tree/main/examples/esp-idf/02_lvgl_demo_v9

## Visual parity
The native Industrial Control panel follows `simulator/quick.js` and the `.quick-*` styles in `simulator/index.html`: industrial metal panels, red LINK wordmark, three channel colors, numeric readouts, fader tracks, scale marks, green track readout and hazard separator. Native LVGL uses a simplified continuous indicator and rectangular hazard marks rather than the browser's segmented LEDs and diagonal texture. Track titles, LIST, PKE face navigation and exact browser hold/swipe timing remain to be ported. The physical layout requires display/touch validation.
