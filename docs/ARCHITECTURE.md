# Architecture

## Layers
1. UI: display-only state, touch gestures, screen stack and intent dispatch.
2. Command policy: locked/offline/stale/unsupported guards, confirmation, equipment eligibility.
3. State store: normalized telemetry with timestamps, unknown values and capability flags.
4. Transport interface: read status/configuration, discover, submit intent, subscribe to updates.
5. Local simulator: independent deterministic state reducer; no network.
6. Future Attenuator adapter: HTTP/WebSocket, reconciliation and error handling.
7. Board abstraction: display/touch, Wi-Fi, NVS, power management, optional microphone.

Keep all write routes behind command policy. Do not let UI widgets call HTTP directly. Voice requests use the same guard. Read-only updates remain enabled while locked. On sleep or disconnect cancel pending gestures and lock; do not automatically undo equipment actions.

## Display
410 × 502 logical pixels. Reserve approximately 50 px header, 380 px content and 72 px navigation with rounded-corner insets. Use a fixed viewport and flexible/scrollable content. Lock face can use full-screen layout. Main pages use launcher shortcuts and horizontal swipes; lists retain vertical scroll. Native hardware dimensions and touch transform must be validated.

## State and compatibility
Unknown fields stay unknown. No assumption that all upstream shared mock values exist on Gen 1. Detect available API paths when possible and allow explicit legacy capability profile; do not send probes with side effects. Keep secrets in local storage outside the repository. Never publish personal connection details.

## Testing
Simulator tests are not hardware tests. Add UI viewport regression, gesture cancellation, disconnect/stale state, command timeout, partial preset failure, track boundary and firmware capability fixtures. Later add hardware-in-loop tests with owner-controlled prop, verifying that no command is sent while locked and that physical controls remain usable.
