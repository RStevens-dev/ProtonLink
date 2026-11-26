# Development workflow

Production main is branch-published. Develop is integration only. Create feature or fix branches from develop; open one PR per batch; require regression and browser smoke checks before merging. Release via a separate reviewed develop-to-main PR. Repository branch protection must be configured by the owner; no claim it is currently enabled. Native Pages publishing does not gate on CI. Hardware behavior is not validated by simulation.

Batch 1 adds local telemetry freshness, link recovery polling, command outcome classification (telemetry receipt is not proof of actuation), quality metrics, configuration backup/restore, scenario runner, native-size preview, layout inspection and browser smoke tests. No external equipment changes.
