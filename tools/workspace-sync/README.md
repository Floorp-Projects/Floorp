# Manual Workspace Sync browser verification

Run from the repository root after installing its normal dependencies:

```sh
deno run -A tools/workspace-sync/verify-browser.ts \
  --binary /Applications/Floorp.app/Contents/MacOS/floorp
```

Use a complete Floorp application with Gecko 158 or later, not a bare Gecko
Runtime without Floorp's workspace UI. This manual driver is separate from test
discovery. It requires a local esbuild installation available through the
repository's usual `node_modules/.bin/esbuild` link (macOS/Linux).

The driver always creates a fresh profile under `_dist/workspace-sync-manual/`.
It never accepts an existing profile or port. It launches the supplied browser
with `--no-remote`, verifies both its exact profile path and a dedicated marker
preference before running scenarios, and terminates only that spawned process.
No account credentials or authenticated cloud service are required. A temporary
loopback HTTP server supplies the test pages. Results and the isolated profile
remain in the generated directory for inspection.

The model and wire suites exercise 23 cases. Native engine scenarios cover
CryptoWrapper encryption round trips, per-tab uploads, independent additions,
delete wins, unacknowledged/partial/successful tombstone GC, local unsubscribe,
unknown-parent and later-batch parent deletion, repeated incoming failure after
the base retry list is pruned, repair/recovery, disabled workspaces, invalid local
metadata and recovery, child-first metadata clock zero, pending local navigation
while unrelated records or tab closures arrive, and account start-over during an
in-flight provider apply or signed-in-UID lookup. Failed UI apply recovery retains
unopened remote additions, subsequent local navigation, explicit local tab
closure and reordering without reclassifying unchanged pre-apply values as new
local edits.

The engine scenario uses actual native Sync classes and record encryption. It
simulates the base engine's repeated-failure pruning around real reconciliation
and application calls; it does not perform authenticated FxA networking.

The adapter scenario uses actual `gBrowser`, containers, SessionStore and tab
movement in the isolated browser. Its workspace store/context are isolated test
adapters, so it checks native tab behavior without claiming to exercise every
production entry-point wiring path. It covers stable IDs, receiving into the
local container, physical-order baselines, moving/duplicating between workspaces,
checkbox state ownership, and native Undo Close restoration.

The regular model tests remain runnable through the repository's browser runner:

```sh
deno task test --near browser-features/modules/test/workspaces
```

Production loader builds validate the complete Chrome/SolidXUL and ESM import
paths separately. A full patched Runtime build and authenticated two-device FxA
acceptance remain release checks.
