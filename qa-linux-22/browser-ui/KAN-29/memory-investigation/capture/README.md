# Single requested-Arabic low-resource observation

Executed one fresh English bootstrap and one requested-Arabic observation on official parenta629 / native157.0.1/build20261007125927. See analysis-report.md and analysis-summary.json. The bootstrap completed normally0 and installed signed matching157Arabic; a pre-capture QA copy failed on a dangling lock, then copying alone was repaired and the same normal-ended fresh profile continued. No dirty old profile or159 migration was imported.

bootstrap-executed.py and capture-executed.py preserve the actual two phase implementations. run.py includes the lock-copy correction for reference. These are historical diagnostic scripts, **not a validated profiler recipe**: unsupportedcpu warned, registeredUSR2 produced noJSON, memoryreport signals were unregistered and not sent. Do not rerun them expecting a proven profiling path. The authorized Arabic observation was performed once; no second run is implied or requested here.

Raw profiles, logs, fullprivate results and snapshots remain outside publication. Only curated technical metadata, numeric resource curves and hashes are published. Instrumentation changes/limitations and the exact next prerequisites are in ../code/instrumentation-limitations.md and ../next-observations.md. No source, product, validation, CI or lock setting changed.
