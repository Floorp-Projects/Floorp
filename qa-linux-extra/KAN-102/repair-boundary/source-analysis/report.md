# KAN-102 repair boundary: native visibility vs independent Workspace presentation

Read-only diagnostic design. No browser/product/code edits, tests, worktree changes or external posts. Preserve the parent-run baseline distinction: final native isolation succeeded; active STG group/selection and Manage Groups UI diverged; original GH#2412 persistent member-tab-strip leak was not reproduced by that fixture.

## Precise STG guards

Signed STG5.3.2 `stg-background.js:500-538` does **not** remove membership on every raw hidden event indiscriminately:

1. `excludeTabIds.has(tab.id)` → `Cache.setTab(tab)` and return. STG uses this set around its own show/safeHide operations.
2. It recalculates `changeInfo=Cache.getRealTabStateChanged(tab)`, calls `Cache.setTab`, and returns if the cached native state did not change. `js/cache.js:17-32` compares the listener's configured state fields, including HIDDEN.
3. If the tab is pinned and pinned itself did not change, return.
4. With a real `pinned` or `hidden` change, truthy `changeInfo.pinned || changeInfo.hidden` invokes `Cache.removeTabGroup(tab.id)`. **No check of extension source, Workspace attribution or hiddenBy occurs.** `js/cache.js:96-98` deletes the cached groupId and calls `browser.sessions.removeTabValue(tabId,'groupId')`.

Thus an ordinary non-pinned STG-member tab, not in STG's own exclusion set, which becomes natively hidden through Workspace is on a code path that removes STG membership. Different timing/exclusion/cache states can explain why an event and a later session snapshot do not directly establish which branch ran. Record cache/UI/native results independently; do not infer durable deletion solely from the branch or a0/0 screen.

## Native/API semantics that distinguish candidates

The executed official Floorp12.21.1 Runtime's `ext-browser.js:821-823` implements extension `Tab.hidden` as `nativeTab.hidden`. Native tabs container `tabs.mjs:919-940` builds nonHiddenTabs by `openTabs.filter(tab=>!tab.hidden)`; visibleTabs filters by `tab.visible`; tab.mjs:253-258 defines that from isOpen, !hidden and group.isTabVisibleInGroup. It does **not** test arbitrary Workspace CSS display. Native tab navigation/selection code uses these collections. A tab hidden only with CSS can therefore remain a native visible/navigation candidate even when its strip rectangle is absent.

The packaged `tabs.json` describes `hidden` as “True if the tab is hidden”; `tabs.hide` requires tabHide, may not hide every requested tab and returns IDs it actually hid. `tabs.show` is also a native operation. `ext-tabs.js:1661-1665` forwards hide to `gBrowser.hideTab(tab, extension.id)` and only returns successfully hidden IDs. Do not fabricate tab.hidden values or TabHide/TabShow events to make a diagnostic appear contract-compatible.

**Visibility is not a privacy/access boundary.** `TabManager.canAccessTab` (`ext-browser.js:1299-1312`) checks private-window access and container isolation, not hidden or CSS display. Appropriately permitted extensions can access tabs regardless of hidden status. CSS-only Workspace presentation should not be called a security/privacy fix or privacy breach solely from hidden:false. It does, however, change the native visibility/query/navigation contract and may violate Workspace isolation expectations.

`showTab` removes native hidden and deletes SessionStore hiddenBy. `hideTab` skips already hidden/selected/pinned/closing/sharing tabs; a second simultaneous hide reason is not added. Runtime `ext-tabs.js:55-71,103-111` restores an extension's owned hidden tabs across windows on **extension disable** and on update removing tabHide; onUninstall here only clears popup confirmation (do not assume all uninstall lifecycle hooks from this one snippet). This native restore can conflict with an inactive Workspace if both systems need the same tab hidden.

## Candidate A: native Workspace hiding, preserve foreign hidden owner and selection

Definition: keep honest native Workspace hide/show; never passively show a tab hidden by an active external owner; do not select an extension-hidden last-show/fallback tab merely to enter a Workspace. Restore native Workspace-hidden eligible tabs. This is a candidate, not an established implementation.

**What it can isolate:** whether avoiding the initial show of STG-inactive A tabs prevents unwanted applyGroup and selection changes. The root trace's first ownership override occurred during the create/leave path, so instrument before creation, not only on return.

**What it cannot resolve by itself:** outgoing Workspace must natively hide previously visible STG member tabs. Outside STG's guards this honest hidden:true feeds its membership-removal path. One native hidden boolean plus one hiddenBy string cannot simultaneously represent independent hide reasons while preserving STG's current “external hide removes group” interpretation. Selecting no extension-hidden tab prevents one trigger, not every hide/show feedback. Preserving a hiddenBy marker alone is insufficient to claim composed ownership, lifecycle restoration, or per-Workspace STG active-group support.

This is a **conditional incompatibility**, not proof that all safe repairs are impossible: a coordinated protocol or an explicit upstream-supported representation could change the composition rules. A browser-only patch cannot both emit unchanged honest native hidden events and guarantee an unmodified extension will not react to them as its code specifies. Suppressing/faking events or editing STG's exclusion/cache internals is outside this candidate and would not demonstrate native compatibility.

## Candidate B: independent Workspace presentation (CSS or equivalent layer)

Definition: STG retains ownership of native tab.hidden; Workspace uses a separate presentation flag without changing native hidden for its own scope. Do not label this a semantics-preserving one-line alternative.

**What it can isolate:** whether Workspace-generated native hidden transitions cause STG's membership/UI divergence. If STG remains stable only inB, that supports the event-contract interaction; it does not establishB is production-correct.

**Unresolved obligations:** native visibleTabs/nonHiddenTabs, keyboard next/previous and close-successor selection, All Tabs/list counts, extension `{hidden:false}` queries, tab dragging/adoption, native group/split wrappers, selected tab/content and session restoration can disagree with Workspace presentation. STG can also natively show/hide tabs across separate Workspace scopes because its group is per browser window. Every UI/API path needs an explicit policy; simply hiding the strip may leave content selection on an invisible other-Workspace tab. Native API results must remain truthful, not be overwritten to match CSS.

## Minimal discriminating experiments and acceptance

Use normal non-pinned, non-sharing, non-split fixtures first. Use the actual signed STG and native APIs; capture tab IDs/Workspace IDs, selected tab/window, native hidden, hiddenBy, per-tab session groupId, STG background current group, native events, settled Manage Groups row IDs/counts, and screen samples. Do not read private user tabs. Treat native snapshots and manager UI as separate surfaces.

| Experiment | CandidateA discriminator | CandidateB discriminator / mandatory acceptance |
| --- | --- | --- |
| **Pure passive refresh** with STG groupB active/A hidden, no Workspace or explicit selection change | A-hidden owner and hidden state remain; no TabShow ofA; Workspace-only hidden fixtures must still be restorable. Tests the narrow owner guard without outbound native hides. | STG/native states unchanged; no fake API event. This alone does not cover switching. |
| **Two pre-existing Workspaces**, move away/return via actual UI, same STG fixture | Isolate outgoing real hidden:true from create-new-Workspace side effects. Track whether session/cache membership and UI change even thoughA's owner was preserved. | STG group/selection/members/UI stable AND no other-Workspace selection through Ctrl+Tab, next/previous, All Tabs and close-successor paths. A mere missing strip rectangle fails equivalence. |
| **Create Workspace then return**, same baseline | Reproduce exact prior bounded conflict. Accept only preserved active STG groupB, selected fixture6, all6 memberships and real manager rows/counts after settled operations—not only A-hidden/B-visible final snapshot. | Same integration acceptance plus independent Workspace isolation; observe API/native visible list disagreement explicitly. |
| **Extension-hidden last-show/fallback** | Entering Workspace must choose an eligible visible/Workspace-owned-hidden tab or a normal new tab; do not uncover STG-inactive tab, delete its owner, silently switch STG group or strand a selected hidden tab. | Independent presentation must not leave an invisible other-Workspace selected content tab. |
| **Disable STG, remove STG, and update/remove tabHide**, tested separately in active and inactive Workspace | Normal native owner restoration must not reveal inactive Workspace tabs, leave orphan hidden tabs, or require a raw marker/cache reset. Re-enable with a normal fresh signed extension session and compare memberships/UI. | CSS/inactive Workspace isolation still holds during native extension restoration; active tabs remain reachable; no stale presentation state after removal. Do not suppress lifecycle callbacks. |
| **Two browser windows**, each with distinct Workspace fixtures; STG group switch/move/adopt tab | No wrong-window selection/group migration, foreign owner loss, or blocked legitimate STG focus-to-group window. Preserve per-window baselines. | Native-vs-presentation visibility contract and tab adoption remain coherent in destination; STG's documented other-window focus action is not accidentally disabled. |
| **Normal shutdown/restart** in both Workspace states | Restore honest hidden reasons, selected Workspace and STG memberships/UI; no orphan inaccessible tabs; exercise normal saved sessions, not process killing. | Persist independent presentation and native hidden separately; do not restore all CSS-hidden native-visible tabs into strip/content selection. Extensions see truthful current native state. |
| **Manager-page reload recovery**, no synthetic second switch | Distinguishes stale manager UI/cache from native/session membership change. Count a second STG switch only if its actual row exists and click executes. Record whether reload restores rows or remains0/0. | Same criterion; reloadingUI is diagnostic recovery, not proof of fixed group-switch semantics. |

The minimal first split is **pure passive refresh → pre-existing Workspace round-trip → exact create/return**. It differentiates local unhide ownership override from unavoidable native external-hide feedback. Lifecycle, multiwindow and restart are required before promoting a diagnostic result to a repair claim. Normal STG show-before-hide can overlap briefly; settled isolation and sampled user-visible frames are separate acceptance checks.

No candidate should be marked passing by disabling protections, bypassing real STG operations, changing extension group/cache state directly, forcing fake events, or hiding native/API disagreements from the evidence. A meaningful native regression is the actual before/after integration behavior above, alongside a separate ownership preservation test—not a mock that only mirrors the proposed guard.
