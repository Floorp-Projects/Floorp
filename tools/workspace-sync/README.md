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

The harness exercises 75 cases: 23 model/wire, 27 native engine and 25 adapter
cases. Native engine scenarios cover CryptoWrapper encryption round trips,
per-tab uploads, independent additions, delete wins,
unacknowledged/partial/successful tombstone GC, local unsubscribe,
unknown-parent and later-batch parent deletion, repeated incoming failure after
the base retry list is pruned, repair/recovery, disabled workspaces, invalid
local metadata and recovery, child-first metadata clock zero, pending local
navigation while unrelated records or tab closures arrive, and account
start-over during an in-flight provider apply or signed-in-UID lookup. Failed UI
apply recovery retains unopened remote additions, subsequent local navigation,
explicit local tab closure and reordering without reclassifying unchanged
pre-apply values as new local edits. Further cases cover no normal-window
provider, one UI apply per changed workspace in an incoming batch, batch
failures and intervening local edits, unsubscribed deferred closures, and
invalid persisted baseline recovery.

Disabling workspaces during incoming processing rejects and retains those record
IDs for retry, aborts pending provider work, and suppresses UI retries until
re-enabled. Tests verify re-fetch after an advanced native cursor,
repeated-failure pruning, tombstone replay and recovery of partially applied
workspace state.

The engine scenario uses actual native Sync classes and record encryption. It
simulates the base engine's repeated-failure pruning around real reconciliation
and application calls; it does not perform authenticated FxA networking.

The adapter scenario uses actual `gBrowser`, containers, SessionStore and tab
movement in the isolated browser. Its workspace store/context are isolated test
adapters, so it checks native tab behavior without claiming to exercise every
production entry-point wiring path. It covers stable IDs, receiving into the
local container, physical-order baselines, moving/duplicating between
workspaces, checkbox state ownership, and native Undo Close restoration. It also
covers source-first capture during physical tab moves, global workspace deletion
from a private-window controller, and unsupported-page navigation. The latter
preserves the canonical web record and stable ID, never captures an internal
page's title, keeps the local page intact during incoming updates, and resumes
synchronization on returning to HTTP(S).

Delayed HTTP fixtures also verify that an existing tab retains its incoming URL
and title until native commit. Native Back (including during the pending load),
explicit navigation to the exact original URL, HTTP 204 without a commit and
account abort exercise intent cleanup. User-created tabs remain explicit during
that wait. An accelerated apply deadline also verifies that a still-live native
load keeps its URL/title intent, same-target retries do not restart it, and a
sibling's HTTP 204 cannot stop it. The fixture server supplies bounded
`/sync-slow` and `/sync-no-content` responses without authenticated transport.
Foreground old-page pushState/replaceState notifications also verify that the
native load type preserves a pending incoming target while Back still works.
Equivalent noncanonical URL retries reuse the existing load, and incoming
same-document URLs complete without requiring a new document load.

At revision `78aedc2cc830`, all 58 cases and the full smoke task passed. Nine
isolated negative controls confirmed that removing the corresponding review
fixes fails the intended assertions. Production Chrome and ESM builds also
passed; earlier packaging and physical-device evidence below retains its own
tested revision.

The regular model tests remain runnable through the repository's browser runner:

```sh
deno task test --near browser-features/modules/test/workspaces
```

Production loader builds validate the complete Chrome/SolidXUL and ESM import
paths separately. Normal artifact-based packaging and authenticated two-process
cloud acceptance and a physical Mac-to-Windows round trip are verified below. An
iOS receiver remains a separate check; this JavaScript feature needs no Gecko
native recompile.

## Authenticated two-client acceptance

This separate workflow uses two browser processes on one Mac, each with its own
fresh profile and app identity. It exercises actual Mozilla Account
authentication and HTTPS Sync storage. Use an explicitly authorized dedicated
test account and enter its credentials directly in the two browser windows.

First run `deno task feles-build build --phase before-mach` in the matching
feature checkout to produce the normal production loader outputs. The builder
supplies the Chrome resource base and ESM version/build metadata; standalone
loader builds without those arguments are insufficient. Preparation rejects
unresolved ESM metadata before copying an app or creating profiles. Prepare
copies of a complete installed Floorp application:

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
proves two isolated Desktop clients on one Mac; physical-device evidence is
recorded separately below, and iOS receiver acceptance remains a separate check.

On 2026-10-18, all eight live cases passed with two independently authenticated
QA profiles using Mozilla's production HTTPS Sync service. The packaged loader
source tree was `3535afc0af7d155429addda07e4bcbc47648357f` (the tree of feature
revision `42e38a9d2e35`), and the workspace engine SHA-256 was
`91c19e6960523b14a270598f2ae04e5546abad4415f1408876548b3a8ebc85b7`. This used a
copied complete Gecko 158 Floorp app with production loader overlays and the
native service-registration patch. That cloud run used the QA overlay workflow;
the independent normal-package check below verifies the production packaging
route and default engine wiring.

After the review fixes, all eight live cases passed again at revision
`78aedc2cc830f803a54395f44c567ae1f40cf1ea`, source tree
`c749dba5fe5f0fafb036ac05bfc25ad702f3e79f`, with engine SHA-256
`d7f4503c81c534b2e0b4c3d0b1ac6d852980c71e3ef2894bda1a85e3e2a1c0a8`. The two apps
used production Chrome/ESM outputs with the normal builder's base and metadata
arguments. All 208 Chrome and 168 ESM archive entries matched the generated
files byte for byte. Existing dedicated QA profiles retained their login
sessions; the Windows QA client was stopped for this two-client run. Generated
test records were cleaned up, and other engine metadata and declined settings
retained the same before/after fingerprint. The earlier physical run and
normal-package check below retain their original tested revisions.

## Physical Mac-to-Windows acceptance

On 2026-10-18, a separate Windows x86-64 PC and the Mac QA A client completed an
authenticated round trip through Mozilla's production Sync service. QA B was
stopped so this run had two active test clients. Windows used a private QA
overlay of the authenticated official Floorp `v12.20.2` complete application,
with production Windows loader output from revision `620f0398f950`, the locked
Gecko 158.0.1 Runtime, and the native registration/resource patches. Its engine
SHA-256 matched the Mac engine recorded above. The distributed kit contained no
profile or account data.

The tester confirmed that Windows received the selected workspace's name, Book
icon and two web tabs, while the local-only control workspace was absent. The
tester then renamed the workspace, added and pinned a new web tab, and closed
one original tab. A native Sync on Mac and read-only UI snapshots confirmed the
expected name, exact URLs and pin state, the closed original tab's absence, and
exactly two live web tabs with two distinct stable IDs. Windows interaction was
manual; the successful physical round trip does not imply that its separate
PowerShell report command completed successfully. That helper was updated to
report progress and use explicit browser and transport timeouts. Its read-only
check does not capture or synchronize local changes. Helper transport was
checked against the real Mac QA browser using PowerShell 7, and against stalled,
slow, erroneous and cleanup-timeout fake peers. Execution of the revised helper
under Windows PowerShell 5.1 remains a separate check.

Initial setup followed by three cold restarts in another fresh, signed-out
production QA profile preserved three web tabs, their three stable IDs and one
pinned tab; two tabs intentionally shared a URL and remained distinct. A
negative control with the explicit startup URL `about:preferences#sync`
accumulated one settings tab per restart. Open that page only on first profile
creation; subsequent launches should resume the dedicated profile without a URL
argument. These local restart checks do not claim authenticated multi-device
restart coverage. The physical-run profiles and generated workspace were
retained for follow-up; no credentials, account payloads or profile archives are
part of the acceptance report.

## Normal production package acceptance

This check is separate from `prepare-acceptance-app.py`. It uses the source and
artifact preparation steps in `.github/workflows/package.yml` and the actual
Gecko artifact build and packager. Keep the Runtime checkout, Floorp checkout,
object directory, build state and output under an isolated working directory.
Use the pinned Runtime revision and authenticate the selected artifacts before
building. No account or release signing credentials are required.

Apply the Runtime's upstream patches first, including its branding/configure
patches, followed by Floorp's common Runtime patches and the Runtime packaging
patches. Check out Floorp as `noraneko` within that Runtime tree. Keep artifact
mode enabled and select the normal release branding. Follow the workflow's
version, buildid2, symlink-expansion and preference-override steps as well. The
central commands are:

```sh
# Floorp checkout: production startup, Chrome, all pages and ESM assets.
NODE_ENV=production deno task feles-build build --phase before-mach

# Runtime checkout, with --enable-artifact-builds in its mozconfig.
./mach configure
MOZ_ARTIFACT_FILE="/path/to/runtime.dmg:/path/to/update_framework_artifacts.zip" ./mach build

# Floorp checkout, using the workflow's normal object directory layout.
CANON_PLATFORM=macOS-x86_64 deno task feles-build build --phase after-mach
./static/gecko/pref/override.sh /path/to/app/Contents/Resources/browser/defaults/preferences/firefox.js

# Runtime checkout: package, then apply the workflow's macOS post-package step.
./mach package
./mach python -m mozbuild.action.unpack_dmg /path/to/generated.dmg /path/to/unpacked
python3 noraneko/tools/app-shim/sync-omnijar-patches.py --sync \
  --resources /path/to/unpacked/Floorp.app/Contents/Resources \
  --patch-dir noraneko/tools/patches
python3 noraneko/tools/app-shim/sync-omnijar-patches.py --verify \
  --resources /path/to/unpacked/Floorp.app/Contents/Resources \
  --patch-dir noraneko/tools/patches
```

Recreate the unsigned DMG with the workflow's `mozbuild.action.make_dmg` command
and official installer artwork, then mount that final image read-only and repeat
verification. Source patch checks alone are insufficient for cached Runtime
omnijars; the post-package synchronizer is part of the normal macOS route.

On 2026-10-18 this route succeeded using locked Runtime `daily-1155`, commit
`7dd619742b25567e479eeda97ae071d10542ad0b` (Gecko 158.0.1), and Floorp source
tree `fbc1e274676fd45f966f30978eef5b5e647c0465` (feature revision
`d4b38a77c644`). The final image contained all **208 Chrome and 168 ESM `jar.mn`
entries**, byte for byte matching the generated production files. Its resource
manifest, native Sync registration patch and default
`services.sync.engine.floorp-workspaces=true` preference were present. All 22
packed Runtime resources passed the production patch verifier. This local macOS
check used the authenticated locked release assets; it did not execute the
GitHub runtime-v2 manifest intake, signing or publishing jobs. The engine
SHA-256 remained
`91c19e6960523b14a270598f2ae04e5546abad4415f1408876548b3a8ebc85b7`.

A copy of that packaged app was signed locally with an ad-hoc identity solely
for launch verification. With a fresh, unauthenticated profile and **no user
preference overriding engine registration or enablement**, native Service
registered and enabled `floorp-workspaces`, the resource module imported, and
the workspace UI/provider capture initialized successfully. The profile's
account remained unconfigured. No profile or account data was included in the
package. Release signing, notarization and the iOS receiver are separate checks;
rebuilding Gecko's native code is unnecessary for this feature.
