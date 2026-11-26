# Track library

GPStar's Attenuator web UI reads `songList` from device preferences and maps each newline to consecutive track IDs starting at `musicStart`. Blank lines preserve positions. ProtonLink mirrors that mapping, bounds IDs to the reported `musicStart`/`musicEnd` range, and falls back to relative Track N. Local overrides take precedence over synchronized names. Missing or invalid metadata does not replace cached names. No playback commands are sent by editing names.

Use the simulator's Track library development tab or the watch Music / Library manager shortcut. Paste one title per line, import TXT (sequential), CSV (id,title), JSON (array of id/title objects), M3U/M3U8 (playlist order), or select audio files with actual numeric ID prefixes; audio content is not read or uploaded. Review the mapping before saving. Explicit IDs are validated, duplicates and out-of-range values rejected. JSON export provides the effective mapping for transfer. Names are escaped before rendering. Browser storage persists per origin, with same-origin tab updates; it does not synchronize across physical devices.

Automatic sync uses simulated GET /config/device at initialization and reconciliation; legacy profile intentionally omits songList. This is not a live network connection to an Attenuator. A native ESP32 configuration web server, phone-to-watch transfer, discovery, QR provisioning and real firmware validation remain outstanding. Simulator behavior only.

Upstream reference: https://github.com/gpstar81/GPStar-proton-pack/blob/main/source/Attenuator/assets/index.js (getDevicePrefs and updateTrackListing).
