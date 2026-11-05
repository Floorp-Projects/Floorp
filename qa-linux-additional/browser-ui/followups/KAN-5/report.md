# KAN-5 existing-window OS-drop follow-up

**Run30055: both valid cases passed.** The ordinary native-tab positive control and individual second-row stack-proxy drop into an already existing window each produced trusted source dragstart and trusted destination drop, moved exactly one expected URL, preserved all URI/title multisets, kept all tabs nonpending and retained exactly two normal windows at1s/3s snapshots. This is one follow-up on existing KAN5, not a new issue or denominator change.

Product: root-authorized `/workspace/kan97-runtime/full-package/floorp/floorp`, current main plus unrelated KAN97 changes; **not a KAN5 fix candidate**. Actual GreD matches. Existing merged PR2763 headb98749d110d5dd4e8cd87120c284394a047a0799 remains canonical. Latest originalGH2710/comments and Jira5/latest10283 were read first; sanitized primary-source-summary.json records context.

Actual UI setup: visible Hub Enable tab stacks label click, normal quit0 and same-profile restart, Ctrl multi-selection of three loaded QA pages, native context menu Home/Down/Return on Add Tabs to New Group, actual tab-group-name input typed QA KAN5 and Enter commit. One native group/three visible second-row proxies persisted. Source/target created via native Ctrl+N UI, configured to side-by-side EWMH normal windows; distinct XIDs share actual processPID. OS Alt+9 (read from product key_selectLastTab binding) selects ordinary control into view. No group/adopt/detach API, artificial DragEvent, manual DataTransfer or CSS injection.

| Case | Source/target tabs before→late | Expected move | Evidence |
|---|---|---|---|
| Ordinary native tab |5/1→4/2|ordinary control exactly once|trustedstart/drop, payload preserved|
| Individual stack proxy |4/2→3/3|source B exactly once|trustedstart/drop, A/C remain in two-member source stack|

Existing blank about:blank has empty contentTitle before and after; only that known pre-existing blank is permitted empty. All controlled QA document titles remain loaded/nonempty, the complete URI/title multiset including blank remains identical, and no pending tab is permitted. No assertion, native handler or protection was disabled to produce success. Native dragend was not captured after source removal, but target trusted drop, unchanged two-window late state and cleared observed dragData support completed transfer; no invented dragend claim.

Debian13.6/X11, actual Openbox/Xvfb123, software-rendered product, synthetic OS XTest input—not physical mouse or original Fedora44/Flatpak. focusmanager.testmode=false restores native focus behavior. Actual product signature-required=false/userSet=false is recorded as existing product state; test does not modify it. Content sandbox6. No sandbox-disabling environment. Native startup/default-browsing automation and normal UI stack prefs are the only configuration changes.

Setup and final browser exits0, probe exit0, no forced browser termination. All56 full product tree entries (type/mode/content hash) match before/after. payload-before/after/comparison-30055.json preserve complete manifests. Both windows' visible loaded pages and source two-member stack were visually inspected in screenshot30055-individual-stack-proxy-existing-window.png. Source/PR/Jira unchanged.

## Fixture history (not product failures)

-30051:750px source tab strip did not establish three-tab multiselection; no drag. Wider normal setup window then demonstrated correct multiselection.
-30052:positional NewGroup menu activation followed by Escape did not leave a group. That run cannot isolate missed menu command from cancellation; no drag.
-30053:native keyboard selection delivered trusted exact NewGroup command, producing group1/proxies3; Escape then removed both. This proves editor cancellation by the fixture, not a product inability to create stacks. No drag.
-30054:actual name input/Enter kept group1/proxies3; ordinary-control preselection used incorrect Ctrl+9. Product's Linux key definition is Alt+9. No drag.
-30055:only that proven key-binding correction; both actual drop cases passed. The ordinary preselection is native UI, not selection state injection. Known-empty-blank predicate was corrected while maintaining all payload/trust/count checks.

Each setup/final browser exited0 and each run's full56-entry product comparison has zero differences; probe exits1 on the first four fixture stops,0 on the final valid run. No more browsers are needed. The remaining owned ended profile was removed after /proc reference check; earlier owned profiles had already been removed after normal-exit/socket checks. The final scan confirms no process references any of the five profile paths and display123 socket is absent. cleanup-receipt.json distinguishes earlier removals from the final removal; no other task profiles/builds were touched.

## Remaining conditions and handoff

This fills the published **existing-window actual synthetic OS proxy-drop route** gap in this tested Linux environment. Original Fedora44 full desktop/Flatpak distribution, physical drag, original version comparison and other compositor/package conditions remain unmet. Do not remove original-environment labels, mark Done, or claim every platform fixed. No new defect/fix/PR. Root owns Jira follow-up and public Linux22 integration.

matrix.json, summary.json, drop-30055.json, setup-30055.json, normal/final-quit JSON, full payload manifests/comparison, native-keybinding.json, root-independent-checks.json and screenshots are the final proof. Four earlier raw fixture attempts/screenshots remain as history. publish-list.json lists small artifacts and excludes raw account/profile metadata, runtime/cache, binaries and large browser logs. Scripts preserve the explicit post-package gate, actual UI route and native OS input. The README's initial prepared-only section is historical; this report is the executed outcome.
