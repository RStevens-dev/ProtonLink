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

Expected: PROTONLINK, OFFLINE | FIELD LOCK, and TEST TOUCH. Tapping changes the status to Touch OK | FIELD LOCK. No equipment commands or networking are enabled yet.

## Merge gates and next steps

- CI compilation; then flash on the actual Waveshare board and check boot, display, touch and reboot.
- Add Wi-Fi provisioning and read-only Attenuator status with reconnect and bounded timeouts.
- Add native telemetry model and stale-state lock; port Industrial Control and validate each command on GPStar V1.
- Integrate battery/charging, dimming, sleep/wake and OTA only after board validation.
- Record board revision and installed pack/wand/Attenuator firmware versions.

Reference: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06/tree/main/examples/esp-idf/02_lvgl_demo_v9
