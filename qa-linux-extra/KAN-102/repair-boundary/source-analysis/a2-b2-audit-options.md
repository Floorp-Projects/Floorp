# KAN-102 A2/B2 phase1 audit and product choices

Read-only review of root-run diagnostics; no new test, edit or external post. Lifecycle/multiwindow/restart/disable/remove checks are in progress and are **not inferred as passed or failed here**.

## Evidence strength and diagnostic limitations

The provenance identifies both candidates as isolated official-release copies with one shipping JS resource changed, **not complete source builds or release candidates**. Original release source984509d2, inspected frontend main29a3eb23, unchanged executable SHA2564f05c133…, changed member `chrome/noraneko/nora-content/assets/js/index35.js`. The provenance records real unmodified signed extension use and no suppression of native events. Source and bundled diffs express the two intended visibility models; phase1 runtime observations agree with those changes. This supports mechanism diagnosis, not production packaging parity or final QA completion.

`a2-b2-phase1-audit-evidence.json` compactly extracts actual snapshots and provenance; full source results remain in the sibling candidate directories.

### A2: intrinsic event-contract conflict plus incomplete diagnostic bookkeeping

The show branch reads/restores a persisted `floorpQaWorkspaceHidden` marker, preserves extension-owned hidden tabs, and excludes extension-hidden last-show/fallback selections. After existing-Workspace return, active STG groupB and selected fixture6 remain. This narrows the earlier unwanted groupA activation to an avoidable unhide/selection trigger.

However, **existing-away** natively hides B6/7/8, their extension group fields disappear, and Manage GroupsB becomes0/0. **Existing-return** restores native B6/7/8 visibility, selected6 and extension group2 membership, but Manage GroupsB stays0/0. The same distinction occurs on the new-Workspace path. A1/4/5 retain STG owner/membership throughout. This matches signed STG's ordinary external-hidden handler plus an observed live-manager divergence after membership restoration; the specific missing notification/cache update is not isolated; the hide-driven feedback is intrinsic to unchanged STG's interpretation of honest native hidden:true, not explained by CSS or a fake event. Whether normal manager reload repairs it remains the root's separate test.

There is also a **diagnostic bookkeeping artifact**: A2 sets `floorpQaWorkspaceHidden=1` before calling hideTab and before knowing it succeeded. The pinned global manager tab receives marker1 while still native-visible. Native hide can also refuse selected/sharing/closing tabs. This does not explain ordinary B6/7/8 membership loss, but means the marker cannot yet be treated as reliable production ownership. It also has no established migration semantics for preexisting hidden tabs or combined hide reasons. Those are additional engineering obligations, not evidence that the core STG external-hide behavior is an artifact.

**Conclusion:** A2 is evidence that a narrow ownership/selection improvement can prevent one bounded symptom. It does **not** demonstrate STG compatibility, correct manager rows, or complete multi-owner visibility semantics. Keep it diagnostic until the runtime behavior and owner-state lifecycle are specified and tested.

### B2: expected independent-layer effect, not equivalent native isolation

B2 preserves globally pinned tabs and leaves native hidden state to STG. In neutral Workspace, B6/7/8 are still `hidden=false`, `visible=true`, and in both nativeVisible/nativeNonHidden, while CSS display isnone and geometry is0. This is a direct designed consequence of the source change and native getter contracts, not a screenshot artifact.

Manage Groups retains all6 fixture rows/members. Yet neutral Workspace tab9 is adopted into global STG group2, making ManageB count4, consistent with STG's per-window model. B2 therefore does **not** establish an independent STG group system for each Floorp Workspace. It only avoids native Workspace hide events and their membership-removal feedback.

**Conclusion:** B2 supports the causal event-contract hypothesis and may be a useful separate visual-filter architecture probe. Its successful STG rows are not a complete Workspace compatibility pass: native navigation/API visibility, restore, selection and list semantics still require a deliberate design. Calling a CSS-hidden tab native-hidden, or faking query/event results, would invalidate that conclusion.

## Concrete UX options and tradeoffs

| Option | What the user gets | Cost / behavior boundary |
| --- | --- | --- |
| **Choose one tab grouping system for a profile** | Predictable Floorp Workspaces **or** STG grouping with its normal UI; avoid presenting dual-manager operation as supported. Offer a clear choice and instructions after detection, without silently disabling either feature. | User gives up combining the two systems in that profile. A real profile-wide preference is simpler to explain than an unimplemented per-window exclusion rule. Existing tabs/groups must be preserved and the change reversible. No data migration is implied. |
| **Keep systems in separate profiles/windows where enforceable** | Users retain both workflows with distinct tab sets; profile separation is strongest because addon/config state is separate. | More windows/profiles and less seamless movement. STG documents moving focus to its group's other browser window, so merely opening a second window in the same STG-enabled profile is not guaranteed isolation. Avoid promising it without window-policy tests. |
| **Narrow Floorp ownership/selection repair with a compatibility limitation** | Prevent unwanted passive unhide/group activation where possible while retaining native Workspace hiding. | A2 shows this can preserve selected6/groupB but still leaves ManageB0. It must not be marketed as “STG fixed”. External-hide membership/UI conflict needs its own resolution or user choice. Marker validity, lifecycle and hidden last-show edge cases remain. |
| **Explicit Workspace visual-filter model** | STG keeps native group ownership; Workspaces visually organize a strip while tabs remain globally available to native browser features. | This changes Workspace semantics. Ctrl+Tab, All Tabs, close-successor selection, extension queries and restored selection may reach other visual Workspaces unless all related native paths are deliberately integrated. B2 alone is insufficient. Product copy must describe globally available tabs honestly, not imply native isolation. No privacy guarantee follows from a hidden strip. |
| **Coordinated dual-manager integration** | Potentially preserve STG groups and Floorp Workspaces together, with an agreed active-group/selection/visibility relation. | Requires a defined per-window vs per-Workspace model and cooperation/contract changes. A native multi-reason hide representation alone still yields effective hidden:true, which unchanged STG may interpret as membership removal. Integration must resolve that interpretation rather than invent fake events or manipulate extension caches. This is structural work, not a safe one-line patch. |

For a near-term shippable decision, the **single-manager choice** has the clearest behavioral boundary. The other options are alternatives for review; none authorizes an automatic settings change or migration. A user-facing explanation can say that both features control which tabs are shown and cannot currently be combined reliably, then preserve the user's choice.

## Promotion criteria

Before claiming an actual compatibility repair, require preserved native Workspace isolation, signed-STG active group/selected tab, membership **and real manager row/count state** across existing and newly-created Workspace round trips, plus the lifecycle/multiwindow/restart tests already running. If choosing the visual-filter model instead, acceptance must explicitly change and test native navigation/list/API semantics; do not retain the former native-isolation claim. Original GH#2412 persistent tabstrip leakage remains separate from these measured bounded failures.
