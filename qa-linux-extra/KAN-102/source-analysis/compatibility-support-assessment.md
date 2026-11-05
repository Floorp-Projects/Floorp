# KAN-102: official STG compatibility boundary

Read-only support/source review. No test, edit, branch/Jira change or external post.

## Official statements

The [official README at fixed commit8949384537fcd44210ec4a6bf918f4da9f3a0c57, lines90-98](https://github.com/Drive4ik/simple-tab-groups/blob/8949384537fcd44210ec4a6bf918f4da9f3a0c57/README.md#L90) explicitly says:

> This extension may conflict with other programs similar in functionality.

It names other grouping/hiding managers, including “Totally not Panorama (Tab Groups with tab hiding)”, Panorama Tab Groups, Panorama View, Tiled Tab Groups, OneTab, and Tab Open/Close Control. This is an explicit warning of possible conflicts, **not** a blanket statement that every program using tabHide is unsupported, and not an explicit statement about Floorp Workspaces.

The **actual signed STG5.3.2 package** also includes `help/extensions-that-conflict-with-stg.html:72`:

> To avoid losing tabs or their location in windows and groups, it is strongly recommended to turn off or remove enabled extensions that conflict with the Simple Tab Groups addon.

Its `js/constants.js:104-121` defines a fixed `CONFLICTED_EXTENSIONS` list including Panorama variants, Sidebery, Workona, Tab Stash and other tab managers. `js/management.js:54-65` tests the IDs and opens this warning page when an enabled non-ignored listed extension is found. **Floorp Workspace is built into the browser, not a listed extension ID**; the warning mechanism does not establish support or non-conflict for that combination. Package hashes and exact excerpts are in `official-conflict-support-evidence.json`.

[Official STG issue #1325](https://github.com/Drive4ik/simple-tab-groups/issues/1325) is open with **0 comments** at read time. It reports Floorp12.3.5/STG5.3.2 merging/reopening groups and incorrect tab counts after Floorp12. It is a related user report, **not** maintainer acknowledgment, a declared Floorp support contract, a confirmed cause, or a known fix. The package's ordinary promises (multiple browser windows, saved last-active tab) do not define interaction with an independent per-window Workspace hiding manager.

## Classification supported by the live evidence

The parent-run signed-STG test now has two separately observed outcomes:

1. **Native final tab isolation succeeds**, while STG active-group and selected fixture change after Workspace creation/return. Earlier event traces show Floorp-originating shows of STG-hidden tabs and clearing of their ownership markers; this API-boundary behavior is established by source plus native events.
2. **Manage Groups UI stays at0/0 and the B/1 row is absent** in `floorp-on-v2/workspace-return.png`, while native/session snapshots still enumerate A/B member groupIds. That supports a persistent **UI/model divergence in this combination**. It does not establish deletion of tabs/groups or durable disk corruption. The absent row means the requested second STG switch did not execute; it must not be counted as a switch, tab-strip isolation failure, or successful recovery.

The strongest accurate classification is **reproduced Floorp Workspace × STG compatibility failure with bounded selection and manager-UI desynchronization**. The original GH#2412 persistent member-tab-strip leak remains **not reproduced** in this fixture. Official documentation already warns that competing grouping/hiding programs may conflict; it does not assign all responsibility to STG or absolve the browser. Conversely, observed Floorp showTab calls do not prove that a one-line Floorp ownership guard safely repairs STG's manager cache across all paths.

Therefore do not yet label the complete failure “an established isolated Floorp code defect with a safe minimal fix”, “upstream acknowledged”, “unsupported by explicit Floorp policy”, or “product data loss”. The exact reproducible integration failure can be recorded now, while the isolated repair boundary remains open. A normal Manage Groups reload is a recovery test: restored rows/counts would narrow this to stale live UI/cache; persistent0/0 would require checking the extension background's in-memory grouping separately from browser.sessions and storage. Neither result should be inferred before the parent's test completes.

The prior `selection-conflict-assessment.md` regression suggestions still apply. Any eventual implementation must have before/after native evidence for both selection preservation and actual Manage Groups row/count recovery, not just a final tab.hidden snapshot.
