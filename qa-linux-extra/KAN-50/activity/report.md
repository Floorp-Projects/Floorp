# KAN-50 follow-up: embedded PiP activity and lifecycle repair

## Actual defect and scope

The current official12.21.1 product has a **separate reproducible AppWindow activity/cleanup defect**, distinct from the historical missing-gBrowser blank player already repaired by2699. Opening panel PiP throws while forcing an embedded chrome browsing context active. Registration aborts. Closing a normal-tab player then releases the actual AppWindow even though a panel player remains, and final panel unload throws again, leaving its native PiP tab attribute and global player count behind.

This is not a console-only suppression patch. The candidate resolves activity ownership/counting/listeners to the containing topChromeWindow. Ordinary-tab and embedded-panel players share one AppWindow reference count; the last player releases it and cleanup completes.

| Real media observation | Unchanged official product | One-resource candidate |
|---|---|---|
| two normal/panel players, root activity count |1 (panel registration throws)|2|
| close normal while panel remains |root force false/count missing|root force true/count1|
| minimize actual parent, windowState2 |root inactive/source document hidden|root active/source document visible|
| WebVTT text changes during minimization |yes|yes|
| final native close: actual PiP windows |0|0|
| final native close: global player count / panel PiP attribute |1 / true|0 / absent|
| originating-window exception |NotAllowedError|none|

**No subtitle freeze was reproduced.** Decoded VP8 and real WebVTT captions advanced in both baseline and candidate. The measured failure is activity retention and unload bookkeeping; hardware occlusion/caption behavior outside this fixture is not qualified.

## Reproduction and validation

Read existing Jira10363 and previous72ab evidence before work. Source inspection/patch base remains main29a3eb23145da90706d8a94441373278521cc737; actual official executable is release984509d2053be7bae9027133bda7259cd706b208 / Gecko159.0.1 / Runtime20261103153232. Runtime lock8965ffc346d426ccdf919bd18585e4d4a0c21654 is pinned. Its downloaded PictureInPicture.sys.mjs is byte-identical to the shipped archive member (SHA256642c18358f9d9534dc15086b31a2d2d549e8227b77c0b6d623f53634f65ecafc).

activity-regression.py uses fresh owned profiles and identical generated60s VP8 bytes plus per-second WebVTT cues. Actual native video playback starts through the standard muted video.play API; XTest opens the real native video context-menu PiP and clicks each real player close button. Read-only frame scripts observe actual playback/caption DOM. No PiP/playback handler, caption output, browsing-context activity or count is mocked. The native WM actually minimizes the parent; snapshots assert windowState2 and caption progression. Normal-first and panel-first candidate orders pass. Candidate-video tests10/10 and11/11 (the latter includes native-suite execution); baseline3/10, expected failure on7 lifecycle/activity checks. All sandbox levels remain6 and parents remain responsive before owned cleanup.

The committed browser-colocated pictureInPictureActivity.test.ts adds two tests with **real AppWindows and real embedded chrome browser documents**, executing the actual PiP activity methods/native setter and checking both opening orders and reference release. Both fail on the original Runtime with the same NotAllowedError; both pass on the candidate. No mocked Window/BrowsingContext is supplied. The final type-only annotation adjustment bundles to byte-identical executed JavaScript. Scoped typecheck, lint, formatting, source/artifact patch-parity host test1/1 and patch application/reverse checks pass. Candidate source fixture equals the tested archive replacement exactly.

The initial exploratory Openbox launch lacked its theme lookup path and exited. Its requested minimization never changed windowState3, so those samples are invalid as minimization/occlusion evidence and are excluded. The prior basic PiP smoke also only establishes actual Xvfb video/close behavior, not a managed-desktop qualification. This follow-up supplies isolated XDG data/config directories, verifies a running Openbox and EWMH _NET_ACTIVE_WINDOW, and asserts real minimized state2. An initial automated overlay-geometry attempt was also a harness failure before PiP observations; final activity comparisons use the visible native video menu. These setup failures are retained locally and not counted as product failures.

## Source and patch

Pinned CanonicalBrowsingContext::SetForceAppWindowActive checks IsChrome and IsTop and throws on the embedded context. Old addOriginatingWinToWeakMap used browser.documentGlobal, the panel chrome window; source excerpts include the contract and call path. That exception precedes map registration. unload invokes the same invalid setter before decrementing currentPlayerCount and clearing the native tab indicator.

The small JavaScript repair takes browser.documentGlobal.browsingContext.topChromeWindow for add/remove AppWindow tracking. It attaches TabSelect and window-visibility listeners to that containing AppWindow as well. Counting separately and merely catching the setter would incorrectly release activity when one of several origins closes, so mixed normal/panel reference counts are included in the regression. Normal top-level browser behavior retains the same owner.

Source patch is .github/patches/floorp-runtime/common/picture-in-picture-app-window.patch; matching tools/patches patch targets the shipped moz-src member. No sibling Runtime checkout was modified. Source fixture at the exact lock applies/reverses; packaging parity is tested. JavaScript-only change requires no native rebuilding.

Candidate is an isolated **diagnostic copy**, not a full production build. candidate-verification.json proves only omni.ja changed and only moz-src/toolkit/components/pictureinpicture/PictureInPicture.sys.mjs differs within it. Native executable/libxul and all other files/members match the official product. Patch commit9d042bc17be83c15d2ff3a25eb712f04c4d3eb45.

## Environment and remaining scope

Debian13 Cloud Linux6.18.57, real managed Xvfb/X11/Openbox/private session DBus, software GPU only. This does not provide the original Ubuntu desktop, native Wayland/GPU occlusion or other OSes. Protections and validation remain enabled; no signature, TLS, sandbox, compositor or kernel identity bypass. Known live YouTube CONNECT403 was not retried. Both original historical PiP scope and actual live-site qualification remain distinct from this repair. All owned sessions are stopped; cleanup.json records no running owned products.

Evidence contains raw numeric/media/activity/caption observations, selected synthetic-fixture screenshot, fixed-source hashes/excerpts, actual native test results, scripts and verification. Profiles, generated media, binaries, cookies, credentials and full environment are not exported. This is an additional follow-up to KAN50, not a new Linux22 denominator entry. Draft PR/Jira publication metadata is recorded after creation; no merge, production release or Done transition.

Publication: Draft PR3042, patch head9d042bc17be83c15d2ff3a25eb712f04c4d3eb45; Jira KAN50 comment10379. Standard CI42084373774 in progress at metadata checkpoint (smoke/ARM/Windows provenance checks passed; Linux browser suite running, macOS job queued). This does not claim complete CI or a production package.


Follow-up: exact-head9d042bc standard CI passed; complete stable-v2-validation Linux-x64 unsigned package run42086912660 succeeded. Untouched complete product passes4/4 open/close-order cases (52/52 checks), with all46 product file/symlink entries unchanged. See [full package report](../full-package/report.md). Prior one-resource candidate observations above retain their diagnostic scope.
