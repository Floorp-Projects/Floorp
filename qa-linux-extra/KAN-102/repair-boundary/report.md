# KAN-102: measured repair boundary and required UX contract

**Two concrete diagnostic candidates were implemented and exercised with the real signed STG. Neither meets the requested compatibility acceptance boundary. No speculative PR or full-package workflow was started.** Original GH2412 persistent inactive-tab-strip leak remains unreplicated; this work concerns the separately reproduced Workspace/STG selection and manager-UI conflict.

## Exact candidates and provenance

Base actual browser is official Floorp12.21.1/Gecko159.0.1/Runtime20261103153232, release target **984509d2053be7bae9027133bda7259cd706b208**. Source/current main independently rechecked as **29a3eb23145da90706d8a94441373278521cc737**. Independent worktrees:

- `/workspace/Floorp-stg`, branch `diag/kan102-visibility-boundary`: A2 native-owned restoration + extension-owned hide/selection exclusion.
- `/workspace/Floorp-stg-css`, branch `diag/kan102-css-boundary`: B2 independent Workspace visual layer + same selection exclusion.

Each source delta is one WorkspacesTabManager.tsx file (A2 +13/-6 lines; B2 +14/-8). Source patches, Git source-blob SHAs, complete SHA256s and tested shipping-bundle diffs are exported. Source patches reverse-check against their exact diagnostic worktrees. Independently applying each bundle diff to the verified original reproduces the exact tested member hash.

Execution uses isolated **one-resource diagnostic copies, not complete source builds**. Only `omni.ja` member `chrome/noraneko/nora-content/assets/js/index35.js` changes. Every native executable/library and every other file/member matches the official base; integrity was rechecked after testing. No Runtime ext-tabs/tabbrowser implementation, extension API getter, signed addon or other extension event handler was patched. `materialize.py` reconstructs these diagnostic copies from the original hash-matched product and exported diffs. Old source maps are not regenerated; bundle diffs, not symbolicated source-map locations, are authoritative.

A2 keeps extension `hiddenBy`, records/restores its own persisted `floorpQaWorkspaceHidden` marker, and excludes extension-hidden tabs from current-tab, last-selected and first-tab fallback selection. B2 leaves native hide state untouched and filters foreign-Workspace tabs with a separate CSS attribute, retaining native globally pinned visibility. Both are substantive hypotheses beyond a show-only guard.

The initial A/B seeds were superseded before formal A2/B2 lifecycle qualification. A2 remains a diagnostic implementation: its marker is written before native hide success, so a pinned tab can have a marker while remaining shown; ordinary unpinned B-tab hide/owner results are independently confirmed and do not depend on that artifact. Legacy unowned-hidden profile migration is not qualified. These are additional reasons not to treat A2 as a shipping patch.

## Product route and boundaries

Real AMO STG5.3.2 XPI **efebf6a9f73a1747044624ddbad7a78fd90ffccdb34a426cf6bb555eda307c49**, native signedState2, ordinary non-temporary installation and signatureRequired=true. Three still-installed disabled cross-case XPIs also match those exact bytes. Product sandbox6 and signature verification remain enabled through all launches/restarts. No private profiles/accounts/cookies/personal URLs or extension-private group-storage edits. Fixtures are two groups of3 localhost tabs, with initial blank/pinned manager/neutral Workspace tab excluded from the six-member invariant.

Actual Debian13.6/Cloud kernel6.18.57, dedicated Xvfb/Openbox/private DBus; not original Windows/native GPU/Wayland or physical input. STG switches are real WebDriver row clicks; the keyboard experiment is real XTest Ctrl+PageUp. Workspace transitions use actual popup clicks in phase1 and the product service API for controlled lifecycle setup. Native Window2 creation is real OpenBrowserWindow. The observer reads real native selection/hidden/layout, real browser.tabs/sessions from the signed background process and the live management DOM without selecting the manager to query it. It does not mock, suppress, fabricate or selectively mask TabHide/TabShow events to any extension.

## Results that reject the small candidates

| Acceptance condition | A2 native ownership + selection | B2 independent display |
|---|---|---|
| Same Workspace, actual STG B→A→B | three switches pass,3 active/0 inactive fixture visible | same |
| Create/return and existing Workspace roundtrip | B selection preserved, but live B manager rows fall to0/0 | B selection and six fixture rows preserved |
| Native/API visibility when staying in neutral other Workspace | native hide and layout agree | **three foreign fixture tabs CSS-hidden but native-visible, API hidden=false** |
| Real Ctrl+PageUp from neutral Workspace | selects globally visible pinned tab2, like baseline | **selects foreign fixture tab8 while CSS display:none/rect0 and current WS unchanged** |
| STG B selection while staying in other Workspace | **selects and shows foreign tab6 in current WS** | **selects foreign tab6 which remains CSS-hidden** |

A2 therefore proves the narrower benefit of owner/selection guards but not a complete compatibility repair. The outbound real Workspace hide is still an external hidden:true event to STG. Its actual handler removes cached/session group affiliation; later show/re-adoption does not correctly repopulate the already-open management UI. A normal management-page reload recovers rows in the inspected path, but the same failure recurs on another leave/return. No STG cache or private metadata was overwritten to manufacture success.

B2 prevents that native-hide feedback and keeps STG rows, but changes the existing Workspace visibility/API/navigation contract. The keyboard counterexample persists after the initial pinned handling was corrected. It is not merely an offscreen DOM artifact: an actual native key chooses the hidden foreign tab. This does not establish a private-data access-control vulnerability: tabs.hidden is not the browser's permission/private-window boundary. It does establish that the CSS replacement cannot be shipped as an equivalent Workspace filter without native visibility/navigation/selection integration and an explicit contract.

The unmodified baseline cross-case loses its B row before the requested click; NoSuchElement plus an empty live manager is saved, and **no B switch is counted** there. A2/B2 actual cross-switch observations are not presented as a matched successful baseline action. The original GH2412 settled inactive-STG-tab leak is still a separate unreplicated condition.

## Lifecycle and multiple-window acceptance coverage

| Product | Window1 selection preserved after Window2 WS creation/return | Keyboard target | Actual normal-restart PID chain | Disable/remove/removed-restart fixture result |
|---|---|---|---|
| baseline | False | 2 / pinned=True / display=flex | [70026, 71139, 72649] | 6/6 recovered |
| native-owned-v2 | True | 2 / pinned=True / display=flex | [68904, 71133, 72537] | 6/6 recovered |
| css-layer-v2 | True | 8 / pinned=False / display=none | [69518, 71135, 72575] | 6/6 recovered |

Window1's original WS remains selected in all observed profiles; the unmodified baseline nevertheless exposes STG-hidden tabs and changes its STG selection when Window2 mutates the shared Workspace definitions. The guards prevent that narrow passive-refresh selection change. Window2 is closed normally before restart.

Each of baseline/A2/B2 performs **two actual normal product restarts**: first with STG enabled while the alternate Workspace owns hidden tabs, then after actual addon disable→enable→uninstall and a WS away/return. Six fixture tab records survive the enabled restart. Missing/changed group associations are captured as counts, not silently repaired. Management header `0/3 tabs active` after lazy restore means three members with none loaded; it is not the same as `0/0` with no rows. Normal reload, group A/B row switching, multiple windows, new/existing WS, keyboard, restart and removal are all measured, not inferred from a mock.

All3 main profiles show all six fixtures after addon disable, uninstall, return and removed-addon restart. The three additional cross profiles also recover all6 after disable/return. All6 formal profiles finish with sandbox6, signatureRequired=true and normal browser exit0; no permanent fixture hiding is observed in these fresh-profile paths. This is a bounded positive recovery result, **not an aggregate compatibility pass or a legacy-profile migration guarantee**.

The original orchestration had X11 access and post-restart URI/handle lookup failures. Keyboard failure happened before input; it was resumed with the already-authorized X11 capability, without changing browser protection. URI lookup failed after successful normal product restart; only the remaining Chrome-context addon operations were resumed. Recovery scripts and failure records are included. Do not treat nonzero intermediate wrapper exits as product crashes or an aggregate passing test command. The independent verifier checks completed raw observations and final browser exit records. Multiple passive QA listeners can duplicate event trace records; unique native-event counts and performance timings are not claimed.

## Why a UX contract must be chosen

Signed STG treats its groups as **window-wide**. A tab in a newly created neutral Workspace is adopted by the current STG window group (B count becomes4); the fixture members remain the same six. Workspace wants foreign tabs excluded from the current tab surface. With the current single native hidden state and unchanged STG handling, honest Workspace hide causes STG group removal, whereas leaving hide untouched preserves STG data but loses the current native visibility semantics. Reverse-direction STG show/selection also needs a defined owner. This is not a claim that software interoperability is impossible; it shows that the small guards/CSS substitution do not define it safely.

| Concrete option for coordination | Result/impact | Work before a PR can honestly claim repair |
|---|---|---|
| **Explicit single manager per profile** | STG with Workspace off, or Workspace without STG; preserves each existing mode but removes simultaneous use | explicit user/profile choice and migration/recovery UX; no silent disable applied here |
| **Automatic handoff to the selected tab's owning Workspace** | respects the selection request while keeping a Workspace boundary | native keyboard/API selection routing, visibility layer, group scope when STG members span multiple Workspaces; auto-handoff alone does not fix outbound group removal |
| **Coordinated multi-owner visibility model** | supports intentional combined use without lying about native events | native lists/eligibility, truthful API semantics and reason/ownership lifecycle, upstream STG group preservation/UX; larger integration and privacy-contract decision |
| Separate profiles as operational fallback | both tools remain available with separate sessions | clear account/session/container separation cost; not a same-profile fix |

A deliberate collapsed/visual-layer model could be designed, but B2 is only its falsifying minimal experiment. It needs the missing native/API/routing work and a public definition of what Workspace invisibility means. Silently filtering native events only for STG, returning a fake hidden value, rewriting the extension's private groups or silently moving tabs between Workspaces is outside the allowed repair. None was done.

No candidate satisfies the current combined contract, so no Draft PR/full product workflow is justified yet. A future partial PR must explicitly target the newly reproduced selection/ownership conflict and retain the original GH2412 symptom and remaining manager-sync problem as unresolved; it must not close either by inference.

## Reproduction, evidence and remaining scope

`materialize.py` + both `*-provenance.json` / source/bundle diffs reproduce exactly the diagnostic member from a hash-matched original release. `session.py`, parent setup-case.py/control.py, `experiment.py`, `lifecycle.py`, recorded recovery tails, `cross-group.py` and `finish.py` show the actual route. They require the documented Marionette venv, Xvfb/Openbox/theme/library paths and an execution context authorized for X11; no sandbox/verification-disable flags. Each case uses its own profile/ports/display. Scripts hard-code only task-local fixture/provenance paths and may be adjusted when replayed elsewhere.

The allowlist exports native/API/manage observations with event arrays removed to avoid duplicate trace bulk. Values of retained fields are unchanged, and each excerpt records the original local raw SHA256. Full profiles, credentials, cookies, product/XPI archives, huge browser logs and temporary download URLs are excluded. `verify-boundary.py <export-root>` checks the rejection counterexamples, real restart PID chains, recovery and normal exits. Its exit0 means observation consistency, **not release eligibility**. Source/controller hashes and installed-addon byte checks are separate.

Original Windows10/12.12.2/STG version/import backup, physical input and original persistent tabstrip symptom remain unmatched. This is not Linux environment-blocked: actual signed products, GUI, DBus, input and normal restarts are operational. Original Linux22/C-five table and KAN50 evidence remain unchanged. State/P2 unchanged; no merge/release/Done.

Jira KAN-102 comment10431 records this repair boundary and explicit UX options. Added needs-workspace-stg-ux-decision, preserving earlier labels/P2/To Do. Evidence commit5e43d208bd99c29f38695a742a9021d5713e9063. No PR/full package/merge/release/Done.
