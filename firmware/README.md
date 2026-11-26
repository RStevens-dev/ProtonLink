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

Expected: PROTONLINK, OFFLINE | FIELD LOCK, and TEST TOUCH. Tapping changes the status to Touch OK | FIELD LOCK. Equipment commands are not implemented. Optional direct Wi-Fi telemetry is read-only.

## Direct telemetry (development only)

Run `idf.py menuconfig` and open **ProtonLink connection** to enter the Attenuator Wi-Fi SSID, password and its actual `/status` HTTP URL. Leave SSID blank for offline display testing. Do not commit a generated sdkconfig or share firmware images containing real Wi-Fi credentials. This build-time configuration is temporary; production provisioning must use device-local storage. The watch polls every two seconds and expires stale readings; no PUT requests are implemented. HTTP is unencrypted, so use a trusted isolated Attenuator network.

## Merge gates and next steps

- CI compilation; then flash on the actual Waveshare board and check boot, display, touch and reboot.
- Verify direct read-only status and reconnect on hardware; replace build-time credentials with device-local provisioning.
- Add native telemetry model and stale-state lock; port Industrial Control and validate each command on GPStar V1.
- Integrate battery/charging, dimming, sleep/wake and OTA only after board validation.
- Record board revision and installed pack/wand/Attenuator firmware versions.

Reference: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06/tree/main/examples/esp-idf/02_lvgl_demo_v9
