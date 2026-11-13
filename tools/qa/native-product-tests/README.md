# Native Mozilla tests against a finalized Floorp product

This is an invocation/provenance layer, **not a new test framework or an
implementation of Firefox test APIs**. It calls the checkout's unchanged `mach`
harnesses and manifests. It does not change expectations, skip rules, CSP,
remote actor registration, or leak checking. It neither imports evidence into
#2917's intake nor duplicates that evidence validation schema.

## Requirements

- Python 3.11+ compatible with the selected Runtime's mach (the proof used 3.13).
- macOS or Linux; Windows process cleanup is not implemented and is rejected.
- A native `--enable-tests` object directory from this Runtime checkout.
- A **finalized Floorp product** with production frontend injection, preferences,
  patches and packaging completed, not a standalone Runtime. This runner never
  builds, patches, signs, installs or copies product files.
- Product resources must contain `application.ini`, `omni.ja`, `browser/buildid2`
  and `testing-common/`. Install the **same objdir's** `_tests/modules` into the
  product resources as part of the test-enabled packaging step. Do not borrow
  another Firefox release's modules.
- No other harness/browser using the chosen product, objdir or standard test
  ports. An old shell runner does not participate in these locks: stop or finish
  it first. Do not run against a personal profile or an installed daily browser.

Preflight compares product/App identity (including BuildID) with `dist/bin`,
checks native tests enabled and source ownership, requires Floorp frontend
markers, and compares every testing-module path and SHA256 with `_tests/modules`.
It records the executable and native engine hashes (XUL/libxul.so) from product
and objdir. Engine hashes can differ after packaging/stripping; that difference
is **evidence, not proof that two differing libraries are equivalent**. The
caller still owns build provenance. This does not certify byte identity with a
shipping release, signing status, or complete UI behavior.

## Usage / CI

Use a new logs directory for each run; it must not exist. CLI environment paths
accept absolute paths. Harness test selections are source-relative and commands
run from the Runtime root with MOZCONFIG explicitly pointing to the selected
objdir's recorded .mozconfig snapshot, not a different default checkout.

```sh
python3 tools/qa/native-product-tests/run.py \
  --runtime /work/Floorp-Runtime --objdir /work/Floorp-Runtime/obj-tests \
  --binary /work/product/Floorp.app/Contents/MacOS/floorp \
  --resources /work/product/Floorp.app/Contents/Resources \
  --modules /work/product/Floorp.app/Contents/Resources/testing-common \
  --python /path/to/python3.13 --logs /work/results/run-001 --dry-run
```

Remove `--dry-run` to execute. Dry-run verifies inputs and prints the plan, but
starts **no mach command, browser, lock or test**. Optional repeated `--suite P0`,
`--suite P1`, `--suite P2` or individual IDs select groups; omission means all,
in configuration order. `--config` selects a versioned JSON configuration.
There is deliberately no arbitrary harness argument passthrough or retry-to-pass.
CI can call the same CLI and retain the entire logs directory.

### Coverage plan

| Priority | Kind | Scope |
|---|---|---|
| P0 | browser-chrome | tabbrowser/test/browser/tabs, all tests |
| P1 | browser-chrome | sessionstore browser.toml |
| P1 | browser-chrome | sessionstore browser_oldformat.toml |
| P1 | browser-chrome | sessionstore browser_startup_telemetry.toml |
| P1 | xpcshell | sessionstore/test/unit, full manifest |
| P1 | Marionette | sessionstore/test/marionette/manifest.toml |
| P2 | xpcshell | contextualidentity/tests/unit, full manifest |
| P2 | browser-chrome | contextualidentity/tests/browser, full manifest |

The three P1 browser manifests are **separate invocations/fresh profiles**;
startup telemetry explicitly requires a browser no other test has used.
Marionette receives the manifest file, not its directory, preserving OS skip
conditions. Configured registered-file counts describe this pinned proof source
and must be reviewed when upgrading Runtime. They are not assertions passed or
completed tests. `run.json` separately records observed unique `test_end` IDs and executed non-skipped unique IDs separately.

### Native CLI contracts checked

- `testing/mochitest/mochitest_options.py`: `--appname`,
  `--testing-modules-dir`, `--headless`, logging and `--setpref`.
- `testing/mozbase/moztest/moztest/resolve.py`: manifest positional paths resolve
  all tests in that manifest (including distinct configurations of shared files).
- `testing/xpcshell/xpcshellcommandline.py`: `--app-binary`,
  `--testing-modules-dir`; `runxpcshelltests.py` runs **Floorp --xpcshell**.
- `testing/marionette/mach_commands.py` / harness runner: `--binary`,
  `--headless`, `--setpref`, explicit manifest and standard raw/TBPL logs.
- This Runtime's native Marionette `geckoinstance.py` supplies
  `MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1` only to the generated browser environment
  when unset, permitting its chrome-context tests. The old
  `--remote-allow-system-access` argument is **not added**. No account, OS or
  persistent browser security settings are changed.

The checked-in browser preferences are explicit offline fixtures: empty optional
new-tab top-sites/background and sidebar web panels, plus loopback experiment
and language-pack endpoints. They avoid live-service requests, not failed
assertions. These configurations do not test those optional online features.
`MOZ_DISABLE_NONLOCAL_CONNECTIONS=1` is always retained. Marionette's HTTP server
may use a different port; loopback fixture failures remain local and are logged.
Do not disable the network guard to hide failures.

## Outcomes and cancellation

Exit 0 requires every selected suite to have a zero native exit, raw `suite_end`,
no unexpected structured test statuses and at least one executed (non-`SKIP`, known-status) `test_end`. An all-skipped suite is not a pass.
Incomplete, empty or failed runs cannot be reported as passed. Exit 1 means test
failure/incompleteness; 2 means setup/lock/preflight failure; 130 means interrupted.
Native exit codes, commands, timestamps, config, identity, hashes, full console,
raw JSONL and TBPL remain in the evidence directory. This is not a guarantee that
all registered tests executed: inspect observed coverage and native skips.

Two atomic locks cover host standard ports and the objdir. Existing locks are
never stolen. On interruption the harness process group is terminated best-effort,
**but mozrunner can create separate process sessions**, so cleanup is not claimed
complete. Locks are retained and `manual_cleanup_required` is recorded. After
normal harness exit, the runner checks for remaining processes under the exact
selected product directory, stops the queue and retains locks if any remain.
It does not kill arbitrary Floorp processes. Verify owned browser/helper/server
processes and ports are clear before manually removing retained locks. Persistent
servers outside the selected product directory need operator inspection too.

## Unit tests (no browser)

```sh
python3 -m unittest discover -s tools/qa/native-product-tests -p 'test_*.py'
```

Tests exercise provenance mismatch rejection, module equality, manifests and
native command generation, lock behavior, and conservative completion reporting.
Full execution is intentionally a separate gated step after any existing queue.

## Next step: an optimized Debug product (separate OBJDIR)

The current proof uses an unoptimized Debug build and can be too slow for native
suite timeouts. Do not hide that by increasing timeouts, removing leak checks or
reclassifying failures. Build a **separate** object directory using the same
pinned Runtime + Floorp frontend, with `--enable-tests`, `--enable-debug` and
`--enable-optimize` (supported compiler/toolchain required). Remove the old
`--disable-optimize` rather than mixing conflicting flags. Keep the existing
object directory and logs intact. Repeat the full product assembly and use that
new objdir's testing modules; never point the new binary at the old modules.
Then invoke this CLI with the new paths and compare representative tests before
a full rerun. Optimized Debug is still not byte-identical to the shipped release.
The runner does not create or change a build configuration for you.

A CI job needs only the pinned product/build artifacts, the same Python/toolchain
requirements as native Mozilla harnesses, and this invocation (shell exit must
not be masked):

```sh
python3 tools/qa/native-product-tests/run.py \
  --runtime "$RUNTIME" --objdir "$OBJDIR" --binary "$PRODUCT_BINARY" \
  --resources "$PRODUCT_RESOURCES" --modules "$PRODUCT_RESOURCES/testing-common" \
  --python "$MACH_PYTHON" --logs "$CI_ARTIFACTS/native-run-$CI_RUN_ID"
```

Always upload the logs directory on failure as well as success. Provision a
single dedicated worker per run; wrappers using other locks or none at all must
be separately gated before invoking this runner.

## Isolated P0 chunks (complete re-run)

Add `--suite P0 --p0-chunk-size 10` to the same CLI invocation to run every
registered entry in the original P0 `browser.toml` in ordered groups of up to
10 files, each in an independent `mach mochitest` invocation. Use size `1` for
one file per invocation. `--dry-run` prints the expanded plan without starting
anything. The current manifest contains 203 files, yielding 21 chunks at size
10. Counts are derived from the actual manifest rather than that historical
number. `--suite p0-tabs` also selects all generated chunks; individual generated
IDs such as `p0-tabs-chunk-003` can select a specific chunk.

The effective generated config is saved in `run.json`; each chunk includes
`test_paths`, its selected `registered_files` count, and `source_registered_files`.
File arguments go through Mozilla's native resolver and original manifest
metadata (including OS skips and preferences); this does not create a replacement
manifest, change expected results, or increase timeouts. Each selected file must
exist under the suite directory and be registered in its original manifest.
Full-suite ordering/state interactions across chunk boundaries are intentionally
not equivalent to one uninterrupted run, so report chunked results as such.
An all-skipped chunk is not a pass. Failures do not prevent subsequent chunks,
but leftover product processes or interruption retain locks and stop the queue
for operator inspection. Chunking does not promise recovery from a hung native
harness or automatic cleanup of detached processes.

### Native resolver path correction

Commands run from Runtime source root and pass source-relative test paths, matching
Mozilla resolver identifiers. Absolute source file paths were not valid resolver
identifiers on this build and produced zero tests. `MOZCONFIG` is explicitly set
to the existing `OBJDIR/.mozconfig` recorded snapshot, never rewritten; recorded
`.mozconfig.json` source/object directories must match. Dry-run does not execute
the resolver and is not evidence of test execution. Zero-test attempts remain
failed; none may be reported as successful coverage.

### Isolated profile release-note fixture

The harness profile sets `floorp.releaseNotes.mode=disabled` and `floorp.releaseNotes.choiceConfirmed=true` to suppress the optional first-run release-notes tab/prompt. This prevents a connection to the live release blog under the unchanged non-local network guard. No installed profile or product defaults are changed; release-note UX itself is outside this test fixture. Both prefs are required because an unconfirmed choice otherwise falls back to blocking presentation.

### Native xpcshell product resources

The runner invokes Mozilla `testing/xpcshell/runxpcshelltests.py` directly for
xpcshell suites. Supply `--xpcshell-python` with this checkout's common
virtualenv interpreter; dependency imports are checked before launch. This
avoids the current mach string `--manifest` handling bug without editing
Mozilla's harness. Four workers and the original default timeouts are retained.
The selected product supplies `--app-binary`, `--xre-path`, and testing modules.
Original build mozinfo remains unchanged (including `appname=floorp`).

After acquiring both locks, each new log directory receives a wrapper TOML
including the exact installed original manifest and adding only
`floorp-appdir=browser`, corresponding to original `firefox-appdir=browser`.
Conflicting per-test app directories and nested includes are rejected. Native
manifestparser verifies all test execution metadata is identical; only the
appdir alias and wrapper-relative reporting labels may differ. Original and
wrapper hashes are recorded. No source/installed manifests or product prefs
are edited, and dry-run creates no wrapper. Verbose JSONL command records
allow auditing actual `-g` and `-a` paths (product Resources and Resources/browser).
