# B10 environment-label audit — read only

**Proposed label removals:0. Proposed Jira writes:0.** All10 latest issues and complete comment pages were read, then compared with published Linux22 matrix/reports/raw evidence at commit5bad47baae11f062af39b477cd6e55640922cddc (qa/linux22-browser-ui-evidence-20261105). Matrix bytes match that commit, and every referenced matrix evidence file exists. No browsers, source edits, branch changes, Jira changes, status/parent/Sprint moves or PR changes were performed.

## Why no removal is justified now

The satisfied generic Linux condition has already been cleaned up: no issue retains needs-linux. Fedora44 container work already narrowed needs-fedora-44 to needs-fedora-desktop; panel-body/localized-RTL/right-panel-reveal labels were already removed on29/87. Those facts must not be counted a second time as new removals.

Every issue still has env-blocked, but the latest comments explicitly retain it for unmet **original** conditions. It does not mean no Linux product was run. Removing it merely because Debian/Xvfb/nested KWin/Fedora-container tests ran would change that documented meaning while leaving original conditions outstanding. Current specific labels also remain unmet. Reproduced bug plus an original-environment blocker is consistent; it does not mean the bug is unverified in all environments.

| Key | Latest comment | Environment labels to retain | Evidence boundary |
|---|---|---|---|
| KAN-3 |10294|env-blocked, needs-fedora-desktop, needs-physical-drag|Native Wayland/full production/strict packaging are now satisfied, but original Fedora desktop/package and physical/UI migration routes are not.|
| KAN-5 |10283|env-blocked, needs-fedora-desktop, needs-flatpak, needs-physical-drag|Fedora container and API adoption/new-window OS proxy drag do not prove original Flatpak/desktop or existing-window UI drop.|
| KAN-29 |10284|env-blocked, needs-cachyos, needs-flatpak|Real localized RTL/panel body/candidate comparison are satisfied; original LTR CachyOS/Flatpak symptoms remain unresolved.|
| KAN-31 |10285|env-blocked, needs-fedora-desktop|Fedora userland initial proxy search is not original Fedora desktop/package or all early timing/locales.|
| KAN-39 |10286|env-blocked, needs-fedora-desktop, needs-physical-wheel|Valid same-input native/stack/Firefox distances now exist; physical wheel/acceleration/DPI/original desktop remain.|
| KAN-40 |10287|env-blocked, needs-fedora-desktop, needs-physical-middleclick|Selected/nonselected members pass synthetic pointer tests; original device/desktop and comparator classes remain.|
| KAN-54 |10288|env-blocked, needs-manjaro-plasma|Nested KWin duplicate close groups are not full Manjaro Plasma/three-control/specification coverage.|
| KAN-87 |10289|env-blocked, needs-niri|Native Wayland panel/image/signed-theme paint is covered, but original CachyOS/Niri/other original compositor and hardware conditions are not.|
| KAN-93 |10290|env-blocked, needs-kde-neon|Nested KWin/light-dark/current stock Firefox is not original KDE Neon theme/accent/font and Firefox154+Lepton.|
| KAN-98 |10291|env-blocked, needs-arch-sway-wayland|Source/CI/combined product/strict package are satisfied; original Arch+Sway+Wayland and physical/DPI remain. Wayland proof from another issue does not transfer gesture coverage.|

needs-repro/needs-spec and area/priority/triage/tested labels are outside the environment-only removal scope and remain unchanged. In particular, related RTL reproduction on29 does not establish original LTR reproduction.

KAN-54 remains parentKAN-52, backlog-deferred, To Do and outside the stabilization Sprint; no parent/status/Sprint change is proposed. Other parents/statuses are preserved exactly. This audit does not reinterpret34 environment-blocked triage items as the project's unresolved total.

## Conditions already satisfied but not stale current labels

- KAN-3: native Wayland, candidate full production build, CI, genuine XPT and strict local package completed. Earlier missing-package paragraphs are historical, superseded by later appended evidence; official signed/default identity remains distinct.
- KAN-29: real panel body, repeated hover/pin combinations, true Arabic localized RTL, existing candidate real paint/test comparison completed. The corresponding needs-panel-open/needs-localized-rtl labels are absent already.
- KAN-39: valid warmed-up interior same-input distance comparisons completed. Initial cold0px comparison is historical harness limitation, not the latest result.
- KAN-87: actual right-body/top reveal and image paint, then one normal signed AMO image-theme path completed. needs-right-panel-reveal is absent already.
- KAN-98: sparse/returning cases, source/CI, full production combined candidate and strict local package completed. This does not resolve original compositor/device coverage.

## One useful executable follow-up after packaging

**Recommend KAN-5: individual stack-proxy drop into an existing window.** Published existing-window coverage calls native adoption; the OS XTest drag covered creating a new window. Two real managed windows, native focus behavior, the installed official product and existing Xvfb/Openbox/XTest tools are now available without another package install. One fresh profile, two windows, one source member and an ordinary-tab positive control can verify that actual drag events transfer exactly one tab and preserve all URLs/titles/counts. Check fresh display/window ownership first and execute sequentially after the package task.

This would satisfy a bounded missing UI-route test, **not** physical drag, Fedora desktop or Flatpak. It would not authorize removing any of those labels. No browser was launched here. Lower-cost alternative: KAN-40 ordinary-tab/whole-stack synthetic OS middle-click controls; still not a physical-device condition.

Flatpak's executable exists locally, but the app directory has no stable active deployment metadata. A partial directory/runtime download is not a ready app or issue-specific Flatpak test. No currently executable Flatpak product claim is made. Full KDE/Neon/Niri/Sway/original desktop claims likewise cannot be inferred from existing Openbox/nested KWin capability.

## Artifacts and application guard

matrix.json contains exact current/proposed labels (identical), satisfied versus unmet conditions, latest comment IDs, parents/statuses and per-issue evidence hashes. jira-snapshot.json omits authors/accounts/avatars and retains only issue state/comment IDs and relevant retention excerpts. published-evidence-snapshot.json records the pinned published matrix and referenced-file hashes. followup-readiness.json distinguishes tools from actual Flatpak deployment.

write-plan.json intentionally contains no operations. Any later nonempty label change must re-read current labels/comments and require evidence satisfying that exact condition; do not replace the full label list from this snapshot after another actor edits it. Root owns Jira decisions. No new comments or other writes are requested by this zero-change audit.

## Post-package follow-up

post-package-recheck.json freshly re-read all10 latest labels, parents/statuses and latest comment IDs after KAN97 package completion; each matches this audit. Later synthetic UI followups now complete KAN5 existing-window ordinary/proxy OS drops and KAN40 ordinary/whole-stack OS middleclick controls. See ../followups/. Those observations supersede only the corresponding missing UI-control paragraphs above, not original Fedora desktop/physical/Flatpak conditions. No existing environment label corresponds solely to these now-completed UI routes; justified removals remain0. KAN40 original package type is unspecified and no Flatpak requirement is inferred from KAN5. KAN54 remains outside Sprint under KAN52.
