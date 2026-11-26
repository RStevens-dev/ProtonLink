# Expanded simulator laboratory

This revision provides local watch preferences, four Field Lock faces, warning views, hold-to-confirm overlays, swipe navigation, configurable favorite dock, era accents, ambient dimming, industrial control styling, capability-filtered streams and local lighting previews. It adds resizable columns, focus view, virtual gateway controls, API explorer, fault injection, state comparison and action recording/replay.

## Compatibility
The upstream mock configuration exposes ledCycLidHue/Sat/Lum and ledPowercellHue/Sat/Lum, songList and streamModes. The workshop is preview-only: no POST or EEPROM requests are emitted. Hue conversion and brightness are illustrative. Profiles are simulated assumptions, not verified firmware releases. The conservative profile includes four core streams; the mock profile matches the five documented fixture modes; the extended profile exposes route candidates. OpenAPI previews are explicitly incomplete.

The pack state and cached watch status can diverge on dropped responses. The gateway is modeled as a link and local API emulator, not a separate complete serial firmware emulator. Delays are simulated, not measured. Request-loss probability and injected errors do not reproduce every real-world failure. Recording/replay stores action intents, not exact packets or fault timing; replay uses a clean default profile.

## Remaining validation
Native device drivers, power management, haptic feedback, physical GPStar compatibility, actual configuration saves, full serial gateway emulation, realistic pack geometry, complete API coverage, screenshot export and production visual regression remain unverified or future work. No real network requests are sent.
