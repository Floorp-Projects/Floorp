> Current outcome: authorized execution completed at30055, both valid OS-drop cases passed. See report.md and summary.json. The prepared-only gate discussion below preserves the earlier preparation phase.

# KAN-5 existing-window OS-drop follow-up — prepared, not executed

**Browser start is closed until root says package complete/start followup.** This preparation launched no browser/display/window manager. Gate-check.log records the deliberate closed-gate exit2. Python syntax compilation passed; runtime UI selectors and drag path remain unvalidated until the authorized run.

## Latest source review

Read latest JiraKAN5 description/labels/all3 comments (latest10283), original GitHub2710 and its0 comments, and canonical merged PR2763 headb98749d110d5dd4e8cd87120c284394a047a0799, merged2026-09-21. Read both top-level conversation and all6 inline comments. Relevant resolved review notes confirm one tab from a second-row proxy, native first-row multiselection retention, adopted destination references, and terminal recovery when proxy unmounts before dragend. Those are source/constructed-event findings, not independent physical OS drop proof.

Original report is Floorp12.17.1@154.0.2 Flatpak on Fedora44. Published Linux22 already covers native API adoption and one individual proxy OS drag creating a **new** window. The missing path here is actual OS drag into a pre-existing window. Existing PR2763 remains canonical; no duplicate fix/PR is proposed. Original Fedora/Flatpak/physical-input labels remain unchanged regardless of this bounded synthetic Linux result.

## Bounded run after gate

Use existing official product, or root's forthcoming unmodified full package with only unrelated KAN97 changes. The latter is a current-main package control, not a KAN5 fix candidate. No binary/omni changes or CSS/security modifications.

1. Own unused display123 and unused port30051 by default, actual Openbox EWMH session (truthful XDG_CURRENT_DESKTOP=openbox), fresh owned profile, native focusmanager.testmode=false. Check own GreD equals chosen product directory and read existing signature/sandbox settings without altering them.
2. Open actual Hub Tab & Appearance UI. If stacks disabled, click visible Enable tab stacks label, verify ordinary pref changed, save normally and perform clean application quit(exit0) then same-profile restart. No startup forced stack pref.
3. Create QA data pages using Ctrl+T UI plus navigation fixtures. Create another window using Ctrl+N and ordinary data page. Put both windows at normal side-by-side native rectangles; record actual distinct XIDs/PIDs/EWMH/geometry.
4. Ctrl-click three visible source tabs, right-click source tab and click native New Group UI (direct item or submenu), close editor popup. Read resulting three-member stack/proxy DOM; no addTabGroup/adoptTab/detach APIs. Missing UI preconditions stop as fixture failure with menu/screenshot evidence, never silently fall back to APIs.
5. Positive control: actual OS XTest native ordinary-tab drag into the existing target's tab-strip boundary. Require trusted source dragstart and trusted target drop, exactly one moved tab, unchanged combined URL/title multiset, loaded nonpending content, and exactly two windows.
6. If positive control passed, click actual stack chip/proxy to select one member, then drag another second-row proxy directly to the same existing window via OS pointer press/move/release. No artificial DragEvent/DataTransfer or manual transfer payload. Inspect1s and3s state, exact tab counts/URLs/titles/selected state, source remainder, trusted terminal events and `_dragData` observations. No new window/late duplicate detach is allowed in the success predicate.
7. Save desktop/window screenshots, raw event coordinates/types/trust, before/after/late snapshots. Clean only owned process groups on every exit. Any unexpected result remains observed behavior/fixture diagnosis pending root review, not an automatic new defect or new PR.

These are OS XTest synthetic events, not physical mouse input; Openbox normal windows are not Fedora desktop, Flatpak or any other issue's Niri/Sway environment. No original-environment label is considered fulfilled by this run alone. Source branch stays unchanged. This is a follow-up of existing KAN5, not a new issue or denominator change.

## Command, only after the explicit start message

```sh
QA_KAN5_START_FOLLOWUP=1 QA_PRODUCT=/path/to/unmodified/floorp \
  PYTHONPATH=/workspace/qa-linux-22/browser-ui/python \
  python3 run.py 30051
```

QA_DISPLAY may choose another fresh private X display. Both runner and probe reject execution unless the gate flag is present; do not set it during preparation or before root's start message. Shared/canonical products are read only, profiles are local runtime/<port>, and no new package/native build is performed. Paths reference already installed Xvfb/XTest/Openbox dependencies and can be adapted later.

Curate primary-source-summary.json, readiness.json, scripts, README and eventual small raw results/screenshots. Do not curate github2710-raw.json/pr2763-raw.json with account/profile metadata, runtime/cache/profile contents, large browser logs or binary products. Root owns Jira comments/labels and final Linux22 curation.
