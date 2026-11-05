# KAN-86 / GH2774 source audit

Read-only review on 2026-11-05; no browser execution, product edits, update delivery, remote posts or protection changes by this agent. Runtime skill and AGENTS.md read; current cloud running/connected, unrestricted enforced HTTP, no secret bindings. Parent owns actual responsiveness tests.

## Immutable source

- v12.18.0: `1728a7678c34aec1ad502edb06a168fdf2a07be8`.
- v12.18.1 tag: `ac42feadf1140a3c5f015e46db3c64104e97f60a` (tag resolved only; not substituted for reported12.18).
- v12.21.1: `984509d2053be7bae9027133bda7259cd706b208`.
- Latest main at fetch: `29a3eb23145da90706d8a94441373278521cc737`.

Official source URL template: https://github.com/Floorp-Projects/Floorp/blob/1728a7678c34aec1ad502edb06a168fdf2a07be8/browser-features/ . Downloaded source filenames retain version prefix. Main and12.21.1 files in this five-file sample are identical. ReleaseNotesChoice.tsx, App.tsx and release-notes.ts are byte-identical between12.18 and12.21.1. Actor parent differs; compare saved copies rather than assuming whole actor unchanged.

## Exact fixture prerequisites

`modules/modules/NoranekoStartup.sys.mts` initializeVersionInfo (12.18 lines85–113): first-run is missing/empty `browser.startup.homepage_override.mstone`; `isUpdated` is legacy `floorp.startup.oldVersion` differing from `AppConstants.MOZ_APP_VERSION_DISPLAY` AND not first-run. Startup writes current display version back to oldVersion. This is a local preference comparison, not proof an updater/MAR was run. Do not put the Floorp semantic version into oldVersion assuming it equals the runtime display version; inspect AppConstants from each official product.

`modules/common/release-notes.ts`: audience is new only for first-run + no prior oldVersion + welcome.page.shown false; otherwise existing. Startup only initializes audience if no user value. One-time prompt requires `floorp.releaseNotes.audience="existing"`, `floorp.releaseNotes.choiceConfirmed=false`, `floorp.releaseNotes.promptShown=false`. It does NOT itself require isUpdated. Startup sets promptShown=true after creating tab. A confirmed or already-shown profile is an intentional no-prompt control.

`onFinalUIStartup` runs `openReleaseNotesInRecentWindow().catch(...).then(showReleaseNotesChoice)`. It awaits SessionStore.promiseInitialized; if updated and effective mode not disabled it creates `https://blog.floorp.app/release/${NoranekoConstants.version2}` then waits asynchronously for Workspace ID (20×100ms max), attributes/selects that tab. Then prompt creates/selects `about:welcome?releaseNotes=1`, attributes current Workspace if available. Unconfirmed mode always effectively blocking even if mode support/disabled was seeded. No need to select support to test prompt.

Minimal prompt-only startup fixture: seed audienceexisting/confirmedfalse/promptShownfalse; give mstone and oldVersion their already-current observed values to avoid triggering a blog update page; retain default blocking/protections. Direct navigate `about:welcome?releaseNotes=1` separately isolates page/render/input behavior and is NOT startup/update coverage. To test update-detection branch, seed mstone nonempty and oldVersion to a distinct prior display string; clearly label PREF-SIMULATED UPDATE DETECTION, not genuine MAR. A real upgrade requires untouched compatible prior-profile/product, official normal update path or authenticated MAR with normal verification; no fake update.status/history or signature/sandbox bypass.

`pages-welcome/src/App.tsx` lines80–94 marks welcome.page.shown=true on mount and selects ReleaseNotesPrompt by URL query. `ReleaseNotesChoice.tsx` lines25–85 reads mode/confirmed asynchronously, only ready then enables confirmation; two sequential pref writes on confirm, finally clears saving. Dismiss button works without ready, calls NRDismissWelcomePage or falls back to about:newtab. Parent actor validates current top-level welcome document, removes own tab with closeWindowWithLastTabfalse if another open nonhidden tab exists, otherwise loads about:newtab. Distinguish clicking a radio (local React state only), confirm (prefwrites+dismiss), dismiss (actor/native tab close), typing/window movement (general responsiveness). Direct production chrome welcome uses Services pref access; local dev5187 uses settings-actor RPC, so dev RPC availability is not a faithful production diagnosis.

## Changes and related history

PR2744 https://github.com/Floorp-Projects/Floorp/pull/2744 merged2026-09-14 introduces choices/defaults/one-time prompt and Runtime blog restrictions. Existing users preserve blockers until explicitly confirmed support. This is introduction, not freeze fix. Do not enable support merely to reproduce an existing-user default prompt.

PR2975 https://github.com/Floorp-Projects/Floorp/pull/2975 merged2026-10-25 clears DEFAULT startup.homepage_override_url early to avoid an independent milestone Blog root page ignoring disabled choice; preserves explicit user override. Fixes2954/related2951; not2774. This default override is absent12.18/present12.21.1, together with Nova opt-out and newtab redirector readiness changes in Startup. PR2904 append current_version is unmerged at fetch; not a shipping prerequisite/fix.

GH2774 is Windows11/12.18, no screenshots/stack/sample/profile attached. Four comments only include reporter reinstall17.x did not fix and checklist dispute; no maintainer causal diagnosis/fix. Checklist claims new profile/addons disabled but maintainer disputes completeness; do not elevate these to verified elimination of extension/profile causes. No freeze-specific related PR identified in returned release-note search; search is bounded, not exhaustive proof of absence.

## Native hang hypotheses and discriminators (not findings)

1. CPU/render event loop stall in page or chrome: first baseline page load then radio-only interaction, compare content and chrome async ping latencies plus CPU sample. Same UI source across versions cannot itself prove same native behavior.
2. Startup/session/Workspace contention: compare direct route vs actual startup prompt, Workspaceon/off, restored vs one-tab profile; asynchronous2s poll is bounded, not evidence of an infinite main-thread loop. SessionStore initialization can delay prompt creation, but already-rendered prompt freezing is not explained by await alone.
3. Native tab-dismiss/replacement/newtab interaction: isolate dismiss vs confirm, last visible tab vs another visible tab, hidden/pinned Workspace cases. Record tab count/selected URL/window survival. Actor has last-window guards in12.18 already; do not call2975 its fix.
4. Blog/runtime guard external work: updated branch opens remote blog before prompt. Compare prompt-only vs pref-update simulation, network failure/load, default blocking with no extensions vs supported blocker extension present. No guard/protection disable to manufacture success; support is an explicit separate real UI choice only if tested.
5. Windows native/GPU/input integration remains original-environment gap for Linux results. Debian/Xvfb responsiveness cannot close Windows11 hang without equivalent evidence.

Acceptance: rendered heading and enabled controls, radio selection, successful confirm storing valid mode+confirmedtrue, normal dismiss leaving usable browser; repeat normal restart respecting promptShown/confirmed. Report CPU/latency/operation timeout and process survival separately; a driver timeout or infrastructure failure is not product freeze. A reproducible native hang needs stack/profile before asserting root cause or proposing patch.

## Matching155 native updater contract (reference, not product execution)

Old official12.17.2 platform.ini reports Milestone155.0 and BuildID20260903040334. Shipped update-settings.ini says ACCEPTED_MAR_CHANNEL_IDS=None (literal); do not edit it. Downloaded Mozilla reference https://hg.mozilla.org/releases/mozilla-release/raw-file/FIREFOX_155_0_RELEASE/toolkit/mozapps/update/updater/updater.cpp as updater-firefox155.cpp. This is upstream155 reference, not proof of exact Floorp patched/compiled configuration. Lines3443–3462 document `updater arg-version patch-dir install-dir apply-to-dir which-invocation [wait-pid [callback-working-dir callback-path args...]]`; argv1 argument-version noted as needed by MMS352; first invocation literal`first`199. Actual archive filename is update.mar3047. Under MOZ_VERIFY_MAR_SIGNATURE, signature is verified3065 then channel/version product information3071 before applying. `--channels-allowed`3330 reports "Not Applicable: No support for signature verification" in builds lacking this compile-time support: inspect actual binary result, not source alone. Normal signature/channel failure is an observed capability failure, never authorization to disable checks. A native-updater CLI copy test can establish real MAR application but does not establish AUS delivery, Windows maintenance-service behavior or an original Windows hang fix.

The12.21.1 actor addition routes external HTTP(S) links through browser chrome forGH2787; welcome-dismiss block is unchanged from12.18. Isolate "learn more" link interaction from radio/confirm/dismiss.
