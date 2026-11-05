# KAN-102: signed-product trace assessment

Read-only assessment of root's `floorp-on-v2` results; no additional test or edit performed.

**The original persistent member-tab isolation failure of GH#2412 was not reproduced. A separate bounded Workspace/STG selection conflict was observed:** baseline STG group B (2), selected fixture tab6; after actual Workspace creation and return, settled STG group A (1), selected fixture tab3, while final member-tab isolation was correct. Do not relabel the latter as the original persistent leak.

## Important event-order qualification

The first external override happened **while leaving**, not only on return. `result-ws-away-after.json` records A3/4/5 TabShow at59203.8/59222.8/59227.9 while selected6 still belongs to the original Workspace. At that point the STG-owned hidden tabs become visible and their ownership is cleared. Only afterwards does TabSelect9 select the new Workspace tab, followed by TabHide of original-Workspace tabs. `result-ws-return-start.json` already has blank `hiddenBy` for all six fixtures. A guard examined only against the return-start snapshot would therefore miss the earlier point of corruption.

On return, tab6 is selected first (63139 TabShow6, 63171 TabSelect6), then A3/4/5 are shown (63195/63490/63499), then B7/8 (63620/63626). STG repairs isolation by hiding B7/8, selecting A3 (63939), and hiding B6 (63985). Final fixture visibility is A only. These observations are in `selection-conflict-trace-summary.json`; times are the recorded performance clock, not wall-clock timestamps.

STG's `onActivatedTab` (5.3.2 stg-background.js466-468) only logs, so selection events alone should not be described as the extension activation cause. The supported mechanism is **native selection may show a hidden tab**, and the resulting **hidden:false** handler at548-563 can invoke `applyGroup`. The current trace preserves correct original selected6 initially; the later external shows of A are the more direct observed precursor of the group switch.

## Is a show-branch hiddenBy guard sufficient?

It is a well-supported **candidate local mitigation**, not yet a demonstrated complete fix. Before the leave sequence, inactive A3/4/5 are hidden with `hiddenBy=simple-tab-groups@drive4ik`; calling Workspace `showTab` on them contradicts the extension's explicit visibility choice. Respecting that ownership on the initial current-Workspace refresh could avoid the first A shows and thus this particular trigger. The same hiddenBy check applied only after ownership has already been erased will do nothing.

Safety cannot be concluded from this one guard:

- `changeWorkspace` chooses its last-show/fallback tab **before** the bulk visibility refresh. If that candidate was hidden by an extension, native selection can expose it outside the proposed guard. The selected fixture6 in this trace was Workspace-hidden, so this is a separate uncovered case, not a claimed cause here.
- Outbound Workspace `hideTab` of previously visible STG tabs is an external `hidden:true` event. STG's cache removal (stg-background.js531-536 and js/cache.js96-98) also deletes the session `groupId`, not only a display flag. Preventing foreign-owner shows does not prevent this feedback or guarantee membership preservation across all group/Workspace combinations.
- `hiddenBy` is a single native SessionStore marker, not a composable list of simultaneous Workspace and extension hide reasons. `hideTab` returns immediately for an already hidden tab, so it does not automatically record a second owner. A guard must still restore tabs hidden only by Workspace, preserve extension-hidden tabs, and handle extension disable/uninstall and legitimate explicit user selection.
- STG chooses one active group **per browser window**; Floorp chooses one Workspace per window. The desired relation between these two independent isolation systems is not yet a defined compatibility contract. A single guard cannot establish a per-Workspace STG selection model.

Thus a one-line patch should not be called a complete compatibility fix without before/after native tests. The trace does justify investigating and testing a narrow “passive Workspace visibility refresh must not uncover an extension-owned hidden tab” invariant. It does not justify bypassing either product's actual API behavior.

## Meaningful regression boundaries

1. **Native ownership regression:** via an actual WebExtension `browser.tabs.hide`, hide a non-selected fixture tab and verify actual `hiddenBy`. Run the real Workspace visibility refresh without explicit selection of that hidden tab. Assert it stays hidden, its owner stays set, and no unwanted TabShow is emitted. Also include a tab hidden only by Workspace, which must become visible on return. This distinguishes ownership from blindly refusing to show every hidden tab; avoid fabricating only a SessionStore marker in a mock.
2. **Observed signed-STG end-to-end regression:** use the recorded two-groups fixture and real Workspace UI create/return path. Preserve baseline STG group2, selected6, member session IDs; after settling, assert the same active STG group and selected fixture, A hidden/B visible, and no membership migration. The current trace fails the group/selection expectation, so this tests the new bounded defect, not GH#2412's unobserved permanent leak.
3. Before shipping a fix, cover switching between **pre-existing** Workspaces, extension-hidden last-show/fallback target, empty target Workspace, extension disable/uninstall, and external STG group switch while another Workspace is active. Ordinary pinned/sharing/split-view exceptions remain separate. Baseline after-only event overlap is not itself a failure because STG normally shows the next group before hiding the old group.

These are review/test recommendations only; no code change or new PR is recommended as already proven safe.
