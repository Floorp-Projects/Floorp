# KAN-50 / GitHub2637: historical reproduction and current native PiP regression

## Result

**Official12.16.4 reproduces the reported blank, close-button-resistant panel PiP with a real decoded local video. Official12.21.1 passes the same native video/PiP/close regression.** Normal tabs pass on both versions. No video/PiP handler was mocked. The original YouTube/YouTube Music sites and original Ubuntu/GPU environment remain distinct and unqualified; shared KAN99 live-YouTube attempts encounter Cloud proxy CONNECT403.

| Official product | Normal tab | Panel native video PiP | Native close button | Classification |
|---|---|---|---|---|
|12.16.4 / Gecko153.0 / Runtime20260725075208|real video/clone/close pass|blank player, source video continues, cloning=false|player remains after two clicks; another toggle creates another blank player|historical issue reproduced|
|12.21.1 / Gecko159.0.1 / Runtime20261103153232|real video/clone/close pass|actual video/clone=true, decoded frames advance|player count returns0, parent PID/sandbox maintained|current local-fixture regression pass|

The **same pip-regression.py** and identical60s VP8 bytes independently repeat both products in fresh owned profiles: current exit0/passed=true, old exit1 with `panel video was not visually cloned into PiP`. Both automated normal-tab controls pass; old panel remains open after native close, current panel closes. These are recorded actual-product positive/negative controls, not assertions about DOM construction alone. Interactive exploratory runs additionally verify the current native context-menu PiP path and the old repeated-toggle/multiple-player symptom.

## Context and provenance

Read current Jira KAN50 body/comment10099, GitHub2637 body/comment5231179398 and exact issue PR searches. Original Ubuntu12.16.4@153.0 report concerns video overlay PiP, blank unclosable windows and continuing panel playback. The Windows comment says right-click can close; this evidence only establishes failure of the player's close button, not that every possible window-manager close operation fails.

Current source inspection main29a3eb23145da90706d8a94441373278521cc737 differs from published12.21.1 source984509d2053be7bae9027133bda7259cd706b208 only in OpenBot CI/docs/tests. The executed product is the official published archive, not a new main build. Current archive SHA2567dcedcbfef747819153b0b9c541a9c1f4f4965b0720f16d5d237d39b0c30d613; executable4f05c1330b38e50040440be6a3b6e0e7a5b52bc79edb5c5478d978c81f5a603d previously checked against that archive.

Public old release v12.16.4 was acquired separately, SHA2565192fcf1ac000e0f5e56d08cb088de799671958573d2fe5d42b8352f1599bdf7 matches declared GitHub digest/hash file. Release source811a5b821e1d9d47b40f22aee6df5db2254a54b1, Runtime2d38da4d11be1e0e615f4ddd785ad5e77c95e18d. Actual Runtime153.0/20260725075208 agrees with release runtime metadata; unused outer INI has a different package timestamp and is not used for symbol/version inference. Acquisition snapshots marked not executed refer to the discovery step; this report and trial JSON record subsequent root-owned execution.

## Code-confirmed historical mechanism and existing repair

The old live panel has `typeof gBrowser === undefined`, no selectedBrowser/mapped native tab, and standalone content browser floorp-webpanel-content-browser. Clicking the actual native video overlay opens the player window first, then the owned console records:

`TypeError: can't access property "getTabForBrowser", parentWin.gBrowser is undefined`, actual shipped PictureInPicture.sys.mjs line999.

The extracted, hashed shipped module shows `openPipWindow` before `parentWin.gBrowser.getTabForBrowser(browser)`; `win.setupPlayer` and browser/player weak-map registration happen after that lookup. Thus the observed throw aborts before video cloning and close bookkeeping, explaining the actual blank window plus ineffective player close button. Source video frames/time continue and cloning stays false. A second real overlay click produced two blank players. Cleanup used owned-window direct close after capturing the failure; this is not reported as a successful player close-button test.

Existing **merged PR2699** (merge0f3269b79cbcfebdce59f53dc2586fadbb96c3e2) replaces the standalone/no-tabbrowser panel setup with actual tabbrowser initialization and prepareWebPanelTab. The current live panel resolves a real mapped tab; its video clones and closes. This is a code-level repair of the exact missing-tab prerequisite. It is not an isolated backport/bisect proving PR2699 is the only intervening contributor: Gecko and other product code also changed between releases. No duplicate repair PR is created. Existing webPanelBrowserLoad.test.ts covers mocked tab resolution/container preparation, not a complete native video/PiP lifecycle; the new executable QA regression supplies that missing actual-product evidence without claiming repository CI/full source-build coverage.

A separate current Runtime console error remains: forceAppWindowActive on the embedded chrome context throws NotAllowedError after setup. Video cloning/playback and player close still passed. This is recorded independently and is **not** the old pre-setup missing-gBrowser failure; background-caption/occlusion/API-promise behavior is not qualified or declared fixed. No unrelated Runtime change was guessed.

## Real media, input and environment

Fixture is a generated60s VP8 testsrc2 WebM,480x270/24fps, served only from localhost. Native decoded-frame counts and time advance, original video's isCloningElementVisually state, actual Toolkit:PictureInPicture window state and screenshots are retained. XTest clicks the **real UA video play/PiP toggle and PiP close controls** using measured rectangles. The read-only browser frame script only observes video/UA controls; it never calls PiP, creates fake players or changes the page's video state. No overlay handler is replaced. Exploratory observer/module/frame/coordinate harness mistakes were corrected and are excluded as product failures; the first automated pair failed before media actions due to an ownerGlobal coordinate helper, then the corrected identical script was run on both products. That setup error remains local.

Actual host is Debian13 Cloud Linux6.18.57, Xvfb/X11/Openbox/private DBus, software-rendering device only; not Ubuntu or native Wayland/accelerated GPU. GPU inventory from KAN7 showed no DRM/NVIDIA/dxg hardware. Both products preserve sandbox6 before/after, fresh profiles, standard media validation and TLS settings. No existing prefix/profile, kernel, driver, privacy protection or site authentication changed. Real network YouTube behavior is not inferred from local media. No hardware decode/fullscreen/DRM/Wayland qualification is claimed.

## Evidence and reproduction

Run pip-regression.py inside dbus-run-session with the documented isolated dependencies; use a new --out directory and free --display/--port/--http-port. --binary selects an unchanged official archive; --source-sha records its verified release target. The script generates its local VP8 fixture by default; --video-file permits the same generated byte sequence for paired trials. Actual commands/output summaries are in verification.json. Historical expected failure is exit1, not a passing product result.

Raw regression-current-v2/result.json and regression-old-v2/result.json preserve runtime, frames, cloning, real close outcome and errors. Interactive result.json and historical/trial/result.json preserve context-menu and repeated-player details. Only small text/JSON and selected synthetic-fixture screenshots are exported. Profiles, XPI/binaries, generated videos, credentials, complete environment and giant logs stay local. PiP source excerpts carry shipped-member SHA256; pinned release/PR sources and archive checksums are under historical/acquisition. All owned sessions were terminated after observations.

Remaining: reachable live YouTube/YouTube Music overlay PiP, original Ubuntu/package/GPU conditions and the separate current occlusion/caption error. Keep issue open; classify historical fixture reproduction/current fixture pass rather than universal fix. No new product patch/PR, merge, production release or Done transition. Original22/C-five issue matrices are unchanged; KAN50/KAN99 have a separate additional table.

QA publication: evidence commit2834b1aca2dcde0f383b79bed9e066e2cac39f30; Jira KAN-50 comment10363. Labels narrowed to the remaining live-site/filter conditions; issue remains open/未着手.


Follow-up: exact-head9d042bc standard CI passed; complete stable-v2-validation Linux-x64 unsigned package run42086912660 succeeded. Untouched complete product passes4/4 open/close-order cases (52/52 checks), with all46 product file/symlink entries unchanged. See [full package report](full-package/report.md). Prior one-resource candidate observations above retain their diagnostic scope.
