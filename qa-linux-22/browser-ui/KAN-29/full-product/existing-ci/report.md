# Existing PR2877 CI failure, read-only inspection

Run **37798617165**, Linux job **113385030314**, remains **failed**. Artifact **11560575529** was downloaded and its ZIP digest matched the API SHA256 `588cf0a37e4753273a0468e5fc9a9f49f4ac8f630d2c6445f554ee9e6f70fc42`.

The combined browser suite reports **248 modules passed, 1 failed**. The failure is `pages-settings/test/integration/advanced.test.ts`: **Account sync and profile manager links call browser actions — Error: Missing Manage Firefox Feature Sync**. Read-only log inspection does not establish its cause or relationship to the PR.

`flexOrder.test.ts` passed (2889ms). All 12 panel-sidebar modules passed, including layout (4010ms) and overlay (7380ms). Downloaded Firefox coverage reports 8 files, 16 tasks, all 16 passed. Exact PASS excerpts and original log line numbers are retained. These module-level results do not enumerate each assertion.

Focused candidate-product success must not be presented as a green original CI or proof of every side effect. No test rerun, build or dispatch occurred. Raw logs are excluded from publication; summary.json retains their digests.
