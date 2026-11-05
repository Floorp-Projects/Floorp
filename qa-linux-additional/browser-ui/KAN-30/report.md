# KAN-30 / GH2907 — additional browser UI QA

**Latest:** native-followup.md supersedes the Sidebery/desktop-deactivation remaining conditions below. Normal Sidebery click and trusted native deactivate were qualified under Openbox with native focus restored; dependent test-only Draft3043 created. Historical attempts remain intact.

Latest source review: `29a3eb23145da90706d8a94441373278521cc737`, independent clean worktree. Firefox-sidebar source is identical to release reference984509; this is source provenance, not a claim that the release binary was built from later CI-only main. Official Floorp12.21.1 / Gecko159.0.1 Build20261103153232. Existing PR2934 remains open/non-Draft, head `b7866aaa4905be4d86fb3dff1237d6a70c483f10`; its only change is window blur→deactivate. Current main still uses blur. Original issue, PR and Jira10066/10151 read. No existing PR head or shared settings changed.

Original: Windows10 22H2,12.20.1@157.0.1, overlapping Firefox sidebar + hover; Ctrl+Tab or Sidebery tab selection hides the hovered sidebar. Cloud: Debian13.6, private Xvfb109/X11 and DBus, synthetic OS XTest pointer/keyboard, not physical input. Normal startup config selects overlap+hover. QA tabs are data-page API fixtures; actual selection uses OS Ctrl+Tab. No hover/visibility attributes, CSS or production functions forced.

## Actual browser and paint

Primary pair29922 baseline /29923 candidate establishes real native X11 focus before testing. Hover-only and sidebar-focused Ctrl+Tab both select another real tab. Baseline keeps `:hover=true`, native isOpen=true/hidden=false, but expanded is removed and opacity becomes0 after close delay. Trusted focus/blur trace occurs without intervening mouseleave. Candidate keeps expanded/opacity1 in all six samples per case. Actual screenshots accompany both cases. Pointer leave closes both products (opacity0). Earlier independent pair29904/29907 agrees.

Candidate is an explicitly diagnostic product copy: unchanged real native executables copied; other native resources symlinked; exactly one compiled module entry's existing PR listener changed. GreD/chrome registry confirms its own omni.ja loads. candidate-patch.json records archive/module/executable hashes. This validates the PR computation in a real product, **not a full official source/package candidate build**. No protections changed: sandbox6; signatureRequired=false is official default, userSet=false.

Xvfb-only focus transfer to owned xmessage in29922/29923 emitted no deactivate (empty trace; opacity1 both). This is **unqualified**, not a passed desktop deactivation control. Browser regression below separately tests deactivate with synthetic DOM events.

## Sidebery remaining condition

Normal signed AMO Sidebery5.6.1 install succeeded: signedState2, active, non-temporary; hash in sidebery-package.json. Trusted toolbar click did not select/open native sidebar, including normal quit/restart/native focus. In29921, standard SidebarController.showInitially was explicitly used only as panel-selection fixture. Paint shows empty tab rows, so no valid Sidebery row selection counted. This unresolved extension/Cloud integration is neither original-bug reproduction nor PR fix evidence. Screenshot/raw observations retained. Original Windows and normal Sidebery path remain.

## Regression evidence for existing PR

regression/sidebarOverlay-regression.patch adds a small lifecycle test using existing harness helpers. Final29935 baseline fails on blur preserving hovered sidebar;29936 candidate passes blur-hold, deactivate-close, rehover-open and pointer-leave-close. Explicit window.dispatchEvent and MouseEvent are synthetic DOM fixtures, distinct from OS XTest UI/paint. Lint passes. Isolated runner has documented runtime-config adapter; integrated patch uses canonical helpers. Full standard CI integration remains unverified. Intermediate globalThis target trials29933/29934 failed to discriminate and were discarded. Existing2934 is canonical: no duplicate fix PR created or head changed.

## Excluded setup attempts

29901 missing libxdo fixed with scoped loader path;29903 display collision excluded;29905 preparation assertion failed, unchanged copy excluded;29906 symlinked executable resolved original GreD, excluded.29909 timed out at cgroup16GiB pressure, not a product crash. Exclusive display ownership and real executable/resource-root checks now prevent false candidate claims. Sidebery normal-selection failures are missing conditions, not product defects.

Reproduce: `PYTHONPATH=/workspace/qa-linux-22/browser-ui/python python run.py <fresh-port> cases-focus`; after `python prepare-candidate.py`, set `QA_PRODUCT=$PWD/candidate-pr2934/floorp`. Absolute paths reflect this Cloud workspace: obtain matching release, Marionette/Xvfb/xdotool dependencies elsewhere. Do not source earlier environment scripts that disable sandbox. Profiles, native binaries, caches and extension packages are excluded from QA commits.

Classification: **current Linux reproduction; existing PR2934 diagnostic copy resolves tested Ctrl+Tab hover defect**. Current main is not fixed; no Done/merge. Remaining: full candidate/standard CI, original Windows desktop, valid normal Sidebery row selection, physical input and qualified desktop deactivation.

Additional isolated WM control: official Debian twm1.0.10-1+b1 and xfonts-base1.0.5+nmu1 extracted privately.29924 missing X11 fonts fixed.29925 baseline CtrlTab still collapses; initial xmessage map deadline missed.29926 candidate still holds on CtrlTab and closes on leave; mapped xmessage focus transfer still emits no deactivate. Thus twm+XSetInputFocus also **does not qualify full desktop app-deactivation**; no product failure/pass inferred. wm-control.json and logs preserve concrete limits; no repeated same-condition run planned.
