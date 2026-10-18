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

The injected browser scenarios live alongside the workspace browser tests under
`browser-features/modules/test/workspaces/manual/`. The driver reads them as
scripts and executes them in Gecko's privileged context; they are not host-side
Deno programs.

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
the base retry list is pruned, repair/recovery, disabled workspaces, invalid
local metadata and recovery, child-first metadata clock zero, pending local
navigation while unrelated records or tab closures arrive, and account
start-over during an in-flight provider apply or signed-in-UID lookup. Failed UI
apply recovery retains unopened remote additions, subsequent local navigation,
explicit local tab closure and reordering without reclassifying unchanged
pre-apply values as new local edits.

The engine scenario uses actual native Sync classes and record encryption. It
simulates the base engine's repeated-failure pruning around real reconciliation
and application calls; it does not perform authenticated FxA networking.

The adapter scenario uses actual `gBrowser`, containers, SessionStore and tab
movement in the isolated browser. Its workspace store/context are isolated test
adapters, so it checks native tab behavior without claiming to exercise every
production entry-point wiring path. It covers stable IDs, receiving into the
local container, physical-order baselines, moving/duplicating between
workspaces, checkbox state ownership, and native Undo Close restoration.

The regular model tests remain runnable through the repository's browser runner:

```sh
deno task test --near browser-features/modules/test/workspaces
```

Production loader builds validate the complete Chrome/SolidXUL and ESM import
paths separately. A full patched Runtime build and physical-device FxA
acceptance remain release checks. Authenticated two-process cloud acceptance is
described below.

## Authenticated two-client acceptance

This separate workflow uses two browser processes on one Mac, each with its own
fresh profile and app identity. It exercises actual Mozilla Account
authentication and HTTPS Sync storage. Use an explicitly authorized dedicated
test account and enter its credentials directly in the two browser windows.

First produce the normal production Chrome and ESM loader outputs from the
feature revision. Prepare copies of a complete installed Floorp application:

```sh
python3 tools/workspace-sync/prepare-acceptance-app.py \
  --source-app /Applications/Floorp.app \
  --build-root /absolute/path/to/matching/build-checkout \
  --output-root /absolute/path/to/new/qa-directory
python3 tools/workspace-sync/launch-acceptance-apps.py \
  --setup /absolute/path/to/new/qa-directory/setup.json
```

The preparation command refuses an existing destination. It overlays the exact
production `jar.mn` entries, patches and verifies the packed native Sync
service, and creates locally signed QA A/B apps. The launcher supplies
`--no-remote` and explicit profile paths, with process output disabled. Use the
launcher for these apps so their exact dedicated profiles are always selected.

**Before signing in**, run the preflight:

```sh
deno run -A tools/workspace-sync/verify-live-two-client.ts \
  --setup /absolute/path/to/new/qa-directory/setup.json --phase preflight
```

The generated profiles set `remote.prefs.recommended=false` **before startup**.
Gecko's automation defaults otherwise replace `identity.fxaccounts.auth.uri`
with `https://{server}/dummy/fxa` during Marionette initialization, which
prevents real OAuth completion. Preflight rejects that configuration and checks
all five production account endpoints plus the cached native FxA and Hawk client
hosts. It does not read credentials or initiate a login.

Sign in and enable Sync in both QA windows using the same dedicated account,
including any CAPTCHA or MFA prompted by Mozilla. The QA profiles register only
`FloorpWorkspaces` plus Firefox's mandatory clients engine. Other data engines
are not registered. Password saving is disabled; the ordinary account session
can persist in the private QA profiles.

After completing both sign-ins:

```sh
deno run -A tools/workspace-sync/verify-live-two-client.ts \
  --setup /absolute/path/to/new/qa-directory/setup.json --phase status
deno run -A tools/workspace-sync/verify-live-two-client.ts \
  --setup /absolute/path/to/new/qa-directory/setup.json --phase acceptance
```

Status prints only readiness and same-account booleans. The acceptance driver
uses the production workspace UI/service and actual `Service.sync` transport. It
temporarily pauses automatic scheduling to arrange concurrent unsynced edits,
then restores scheduling. It checks selected and excluded workspaces,
bidirectional metadata/navigation/title/pin changes, concurrent additions,
delete wins over stale ordering, empty workspaces, opt-out/re-selection, and
parent deletion retaining local tabs. It removes only the generated test records
from Sync and compares a SHA-256 fingerprint of other engine metadata and
declined settings before the first explicit Sync and after cleanup. This does
not claim to observe account setup that occurred during the earlier interactive
sign-in. A loopback web server supplies synthetic pages; it shuts down when
verification ends. Snapshot assertions only read UI state and allow bounded time
for native page loads to settle. If a case fails, its generated cloud records
and dedicated profiles remain available for inspection; successful completion
deletes only the generated synced records.

A sanitized result report records case names, cloud host names and build hashes.
It contains no email, UID, password, token, key or account response payload. The
profiles remain in the preparation directory for follow-up. They can contain the
dedicated account's normal browser session and are not report artifacts. This
proves two isolated Desktop clients on one Mac; physical-device and iOS receiver
acceptance are separate checks.

On 2026-10-18, all eight live cases passed with two independently authenticated
QA profiles using Mozilla's production HTTPS Sync service. The packaged loader
source tree was `3535afc0af7d155429addda07e4bcbc47648357f` (the tree of feature
revision `42e38a9d2e35`), and the workspace engine SHA-256 was
`91c19e6960523b14a270598f2ae04e5546abad4415f1408876548b3a8ebc85b7`. This used a
copied complete Gecko 158 Floorp app with production loader overlays and the
native service-registration patch, rather than a full Runtime rebuild.
