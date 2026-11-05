# KAN-102 / GH2412: signed STG and real Workspace interaction

**Current Linux: six settled STG UI-switch observations pass. Actual Workspace switching reproduces a separate group-selection/management-UI synchronization conflict. The original persistent inactive-tab strip leak is not reproduced.** This is not a claim that the issue is fixed, expected, or solely upstream. No safe isolated Floorp repair has been established, so no speculative product patch/Draft PR.

## Prior evidence and scope

Read Jira10150/10169/**10360**, GH2412 plus4 comments and related PR/upstream searches before execution.10360 already covers Windows11 headful official12.12.2/current/Firefox, WS on/off,2x100tabs and100-vs-empty, horizontal/vertical/sidebar,6/10 UI switches. Those large-tab/old-product tests were not repeated. New coverage focuses current Linux actual Workspace leave/return, visibility-owner metadata and transition observations, with bounded Linux WS-off/Firefox controls. Original Windows10/old extension/imported200-590 real tabs remain unmatched. No user profile, account or personal URLs used.

Actual product: official Floorp12.21.1 archive, release source **984509d2053be7bae9027133bda7259cd706b208**, native159.0.1 /20261103153232. Control official Firefox159.0.1 /20261102144711. Latest main read as **29a3eb23145da90706d8a94441373278521cc737**, independent clean worktree `/workspace/Floorp-stg`, branch `qa/kan102-stg`; source analysis is distinguished from executing a rebuild of main. Native binary/omni hashes in environment.json. This did not run KAN50's modified PiP package.

## Signed product route and measurements

Official AMO STG5.3.2, file4469818, SHA256 **efebf6a9f73a1747044624ddbad7a78fd90ffccdb34a426cf6bb555eda307c49**, matches AMO hash and prior Windows test bytes. Standard non-temporary AddonManager file install in each fresh profile; runtime **signedState2/isActive=true**, xpinstall.signatures.required=true and sandbox6 before/after, same native PID, graceful exit0 in all3 sessions. AMO metadata booleans alone were not used to qualify signature. No protection/TLS verification disabled; inherited proxy and its configured CA trust were used.

Three owned Xvfb/Openbox/private-DBus GUI sessions on actual Debian13.6/Cloud kernel6.18.57, software GPU, not Windows/native GPU/Wayland or physical input. Two groups have3 localhost HTML fixture tabs each. Group creation uses the signed extension's own `add-new-group` messages and native tabs.create; switching uses **WebDriver pointer clicks on actual STG manage-page tab rows**, not mocked handlers. The pinned manage tab and ungrouped initial blank are excluded from group-isolation expectations.

Read-only chrome observations record selected extension tab ID, native hidden, computed display/visibility/rectangle, Floorp Workspace ID and **SessionStore hiddenBy** owner. Separate extension-page observations use real browser.sessions groupId/window group values. TabShow/TabHide/TabSelect events and20ms samples retain distinct semantic states. Event snapshots occur during native dispatch (before showTab deletes hiddenBy); settled/sampled owner values are interpreted separately. Layout exposure is not asserted to be a physically presented frame, and observer overhead prevents treating durations as performance measurements.

| Product/config | Actual STG row switches observed | Settled active fixture tabs | Settled inactive visible | Selection |
|---|---:|---:|---:|---|
| Floorp WS on, within same WS |2 (initial B, B after reload) |3/3 |0/3 |in requested group |
| Floorp WS off |2 (B→A) |3/3 |0/3 |in requested group |
| Firefox |2 (B→A) |3/3 |0/3 |in requested group |

Inactive tabs carry hiddenBy=simple-tab-groups@drive4ik, display:none and zero rect. Short show-before-hide overlaps are also sampled in controls, matching signed STG's normal new-group-show→old-group-hide order. They are not automatically classified as GH2412 persistent leaks or physical flicker.

## Additional Workspace interaction actually reproduced

1. With STG groupB active/tab6 selected, create a second Floorp WS through the real product service API as fixture setup, then return through the actual native Workspace popup. GroupA's STG-hidden tabs are shown by Floorp, hiddenBy is cleared, and STG reacts to external hidden:false by activating groupA. Settled selection6→3 and STG window group2→1. Native tab strip converges to correct groupA-only visibility, but the still-open management page shows **both groups0/0**, despite all6 actual tabs and native session memberships remaining.
2. A normal management-page reload recovers the6 fixture rows. It also shows an extra initially ungrouped blank adopted intoB; no actual fixture tab is lost.
3. SelectB using its real row, then **leave and return using only existing Workspace popup entries** (both directions actual pointer clicks, no create call). Away: STG-hiddenA retains its extension owner; visibleB is hidden by Workspace with empty owner. Return: showTab uncoversA, group/selection again changesB2→A1 /6→3. Settled native tab strip again hasA3 visible/B3 hidden, but management UI shows **A3/B0**. Native browser.sessions still identifies all fixture membershipsA1/B2. This is a repeated integration selection/UI-sync conflict, not evidence of saved-tab data loss.

Raw files: floorp-on-v2/result-ws-*.json and result-existing2-*.json; screenshots workspace-return.png and existing-workspace-return.png. Normal reload before/after UI counts saved separately. `verification.json` and independent `verify-results.py` recheck all6 settled UI switches, both conflict paths, native membership preservation and three profiles' protections/PIDs.

The first IPC trial failed because command.json was read during a non-atomic write; product was then intentionally terminated in harness cleanup. Atomic rename and saved requests corrected this, and new fresh profile floorp-on-v2 is the qualified run. A later attempted B-row click while the UI showed0 rows raised NoSuchElement: **no group switch occurred**, so it is not counted as tab-strip failure. Empty management UI itself is independently captured. An xdotool missing-library invocation failed before GUI input and was repeated with the existing task-local library path. These infrastructure/harness events are not product crashes or failed STG visibility tests.

## Source contract and repair decision

Signed STG5.3.2 shows the next group before hiding the old one; it deliberately handles external hidden:false by applying the tab's group and external hidden:true by removing cached group association. Official Floorp Runtime showTab removes native hidden, emits TabShow and clears SessionStore hiddenBy. Floorp Workspace updateTabsVisibility unconditionally calls showTab for all same-Workspace tabs, including STG-owned hidden tabs. Source excerpts/hashes and upstream searches are in source-analysis/.

Official STG README at8949384537fcd44210ec4a6bf918f4da9f3a0c57 explicitly warns **“This extension may conflict with other programs similar in functionality”**, listing other tab-hide/group managers. Signed5.3.2 bundled conflict help recommends disabling conflicting extensions to avoid disruption. This is an explicit general conflict warning, **not an explicit Floorp-Workspace unsupported declaration or proof this exact result is expected**. Built-in Workspace is not covered by the addon's fixed conflict-ID detector. Upstream1325 is an open user report of Floorp12/STG5.3.2 reopen/merge behavior with no confirming fix; native group-header issues1237/1366 are distinct. No external upstream post made.

A show-only hiddenBy guard is not yet a complete safe repair: Workspace's selection happens before the bulk show loop, outbound Workspace hide emits external hidden changes, and one hidden/owner slot does not compose two independent visibility managers. Preserving only one branch may conceal tabs on return or keep UI/session caches inconsistent. No isolated product fix or regression-tested contract is established, so no speculative patch or duplicate PR. A coordinated visibility-owner/selection contract and native-plus-signed-STG regressions are the concrete remaining engineering condition.

## Remaining conditions and handoff

Original persistent tabstrip leak, original Windows10/12.12.2 and actual STG version/import backup remain unmatched. The existing Windows large-tab evidence remains valid and is not replaced by these small Linux fixtures. Physical input and compositor-visible flicker are unqualified. No blanket Linux/environment-blocked result: current Linux products, signed addon, GUI, DBus and network installation are operational. Keep original environment/import labels until their specific conditions are met; add a reproduced Workspace/STG conflict and visibility-owner-contract condition. State/P2 unchanged; no merge/release/Done. Additional-target matrix only; original Linux22/C-five hash remains **4ac5d5f17f97d70e82f7b9f17d6abcc1aa17a92e3708eeadaba6ac7c8415e3cd**.

KAN50 exact report/raw/matrix/package provenance and full source/QA/Runtime SHAs are enumerated in **KAN50-handoff.json**. Its immutable QA commit is **2bd52695c32c40123fe97d77e23a7fff6eb521c8**, under `qa-linux-extra/KAN-50/full-package/`; report.md/matrix.json, four explicit result.json paths, package-run-metadata.json/package-verification.json and genuine manifest paths are included. KAN50 source **9d042bc17be83c15d2ff3a25eb712f04c4d3eb45**, Runtime **8965ffc346d426ccdf919bd18585e4d4a0c21654**. No KAN50 evidence was modified by this task.

Jira synchronized: KAN-102 comment10410; added repro-workspace-stg-conflict and needs-visibility-owner-contract, preserved prior labels/P2/To Do. Evidence commit8155fd5682a9a996e0e85f47c9b7ee63433f2d56. No product patch/PR, merge/release/Done.

Follow-up: two implemented diagnostic candidates completed real signed-STG repair-boundary experiments, including multiwindow, six normal product restarts, native keyboard and disable/remove recovery. Both fail the combined contract; no speculative PR/full-package workflow. See [measured boundary, acceptance matrix and UX choices](repair-boundary/report.md). Original persistent tabstrip symptom remains unreplicated.
