# A2/B2 final boundary addendum

Read-only audit update. No new browser action, product/code change or remote action. This supplements `a2-b2-audit-options.md`, whose phase1 lifecycle status was written before the later root-run evidence became available.

## Decisive cross-Workspace actions

The root clicked an actual STG groupB row while another Floorp Workspace was current. These are successful real STG actions, unlike the earlier absent-row attempt:

- **A2**, `native-cross/result-cross-after-switch.json`: selected6 belongs to Workspace `369253d5-f012-4ed4-a4ef-69ca01461f50`, but current Workspace remains `35d1c643-4398-41e5-b1f9-9b83285d8e3d`. Tab6 has hidden=false, visible=true, display:flex, width225; nativeVisible includes6/7/8. This establishes cross-Workspace behavioral isolation failure after an external group action. An owner-preserving show branch and guarded Workspace-selection routine do not intercept STG's own legitimate native show/select operations.
- **B2**, `css-cross/result-cross-after-switch.json`: selected6 belongs to Workspace `f4657bc6-1cd2-4e6c-a179-5077f046799a`, but current Workspace remains `47c80f46-626b-4841-8149-b13f582bfe81`. Tab6 has hidden=false, visible=true, **display:none and width0**; nativeVisible includes6/7/8. This establishes selection/presentation disagreement, not just a theoretical native-list concern.
- In `css-layer-v2/result-life-keyboard-after.json`, the root's real Ctrl+Prior selects fixture8 while its strip display isnone, width0 and native-visible=true. Baseline andA2 select globally pinned manager2 instead. Thus B2's keyboard risk is experimentally observed for that action. Do not infer every navigation path from one key.

These are **tab visibility/selection and Workspace UX failures**, not privacy access-control claims. Native hidden tabs are not a security boundary against appropriately permitted extensions. Also keep the new cross-Workspace action case distinct from original GH#2412's report of ordinary STG switches leaving previous-group member tabs visible.

## Lifecycle evidence limits

For baseline/A2/B2, the inspected `after-disable`, `after-remove`, and `after-restart-removed` snapshots each still contain all6 fixture tabs, displayed after removing the competing extension/native-hidden reasons. This is evidence against permanent fixture loss in those tested paths; it is not proof that all possible user data/migrations are safe. The wrapper's nonzero exit must not erase completed evidence, nor be reported as a clean exit. Root reports LIFECYCLE_DONE in all3 logs; an independent verifier is reviewing the complete raw lifecycle/multiwindow results. This addendum does not replace that verification with an aggregate “all lifecycle checks pass”.

Compact exact inspected fields are in `a2-b2-final-boundary-evidence.json`.

## Updated product decision boundary

Neither diagnostic is a shippable compatibility fix. **A2** retains native isolation on Workspace-driven round trips but loses manager synchronization and can be overridden by an STG action while the wrong Workspace remains current. **B2** preserves STG's model better by avoiding native hides, but native navigation and STG selection can target a visually absent tab. Both failures are explained by their actual design boundaries, not hidden by the successful six-tab recovery snapshots.

The concrete choices remain:

1. **Choose one manager for the profile**, with a clear reversible choice and preservation of existing tabs/groups. This offers the clearest near-term supported behavior; no silent feature/extension disablement or automatic data migration.
2. **Use separate profiles for the two workflows.** Ordinary same-profile windows are not equivalent isolation because STG intentionally activates its group's window and manages group state per window; only tested window policies could offer that alternative.
3. **Define explicit cross-Workspace activation behavior as part of a structural integration.** For example, an STG-selected tab could bring its owning Workspace into view, or a documented global tab-selection model could deliberately switch Workspaces on native selection. That changes UX and needs atomic selection/presentation plus group-membership rules, normal APIs, lifecycle and multiwindow handling. It is a design option, **not evidence that one TabSelect handler fixes either diagnostic**. CurrentA2/B2 do neither.
4. **Adopt a deliberately global visual-filter model** only if the product agrees tabs remain globally navigable and all selection paths bring the appropriate tab into a visible context. B2's “selected but invisible strip item under another Workspace” must still be repaired; do not present raw CSS-only behavior as acceptable isolation or lie about tab.hidden/API results.

A narrow passive-owner improvement can still be useful engineering work, but it must be described as such and cannot by itself justify “STG supported/fixed”. Final handoff should show these tradeoffs and the completed recovery evidence, while requesting a concrete UX decision rather than approval of an unproven diagnostic patch.
