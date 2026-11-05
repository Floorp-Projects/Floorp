# KAN-29: actual localized RTL, PR2877 independent comparison

Baseline source reference `984509d2053be7bae9027133bda7259cd706b208`; official Floorp12.21.1 / Gecko159.0.1 / build20261103153232. Debian13.6, KWin6.3.6 nested Wayland, Software WebRender. Both runs report actual product windowProtocol=wayland. Synthetic W3C pointer input, not physical input; this is not CachyOS or a full Plasma desktop.

## Locale precondition and installation

Official signed Arabic language pack for matching Gecko159.0.1, downloaded from `https://archive.mozilla.org/pub/firefox/releases/159.0.1/linux-x86_64/xpi/ar.xpi`; SHA256 `3b5d819d581c56a422c8525f207cb00e622e10ea2fca0c97a58dfeae071c26c8`. Normal `AddonManager.getInstallForFile` then install, `Services.locale.requestedLocales=['ar']`, save preferences and normal `Services.startup.quit(eAttemptQuit)`; wait for exit0 before restart. Neither signature nor browser protection preferences changed. On both final runs: pack signedState2, active, appLocale ar, appLocaleRTL=true, RTL_UI=true, computed root direction rtl, intl.uidirection unset. No root-dir fallback is allowed by the final runner. Thus these results are separate from the earlier root-dir fixture with RTL_UI=false.

Earlier attempts are preserved:29660 proved RTL on restart;29661 NewSession timed out (harness/startup failure);29663 saw installed/active pack but availableLocales only en-US and failed the strict precondition. These used process-signal cleanup between installation/restart. The final normal app-quit path succeeded for both29665 and29666. This supports using normal shutdown; it does not establish a product locale bug or conclusively diagnose the earlier intermittent registration. The signed Firefox pack logs missing `browser/allTabsMenu.ftl`; the tested Bookmarks and tab UI visibly paint Arabic, with native RTL_UI true.

## Candidate isolation

Existing open Draft PR2877 head `f43b22d34c0728e0ed800b8016d81b3212190954` was fetched into an independent worktree. Its position-calculation changes apply cleanly to baseline984509d. An independent copy of the official product changes only compiled `updateHoverOffset` logic: physical side from logical end XOR RTL, ignore zero-size rectangles, physical-edge width. All other ZIP entry contents and the native executable are identical; see `pr2877/product-patch.json` and `pr2877/patch-product.py`. This is a diagnostic product copy, not a full official candidate build. No implementation was changed to enter a UI state, and PR2877 head was not modified.

## UI matrix and actual paint

Identical matrix for original29665 and candidate29666: native vertical sidebar side2 × Floorp panel side2 × native launcher pin2, each3 hover rounds. W3C click of Bookmarks button opens actual400px panel body. Standard Sidebar widget is placed using the supported CustomizableUI placement API, then real UI clicks pin/unpin it. Startup/live prefs and SidebarController.setPosition configure vertical tabs and sides. The pin dimension is the native launcher; Floorp panel remains click-pinned. No CSS override/root direction/launcher-visibility API substitutes for UI actions.

| Measurement | Original | PR2877 diagnostic copy |
|---|---:|---:|
| States / hover attempts | 8 / 24 | 8 / 24 |
| Panel body400px | 24 / 24 | 24 / 24 |
| Viewport-sized1366px logical offset | 24 / 24 | 0 / 24 |
| Measured panel logical offset | 1366px | 448px |
| Late native-anchor movement >1px | 4 / 24 | 0 / 24 |
| Unreachable initial hover anchor | 0 | 0 |

`localized-comparison.json` contains all state/round coordinates, selected paint pixels and locale/graphics data; raw `wayland-panel-29665.json` and `29666.json` retain all samples. Sixteen PNGs preserve actual screenshots. In same-right-side/unpinned state (`1-0-0`), original screenshot shows shifted grey launcher area with tab contents absent from its painted position; candidate paints the Arabic tab contents adjacent to the actual Bookmarks body. Candidate same-side left/right anchors settle at448px/664.67px respectively, with expanded width253.33px; pinned widths252.33px. These PNGs were visually inspected. Different UI positions mean screenshot pixel equality is not a meaningful pass criterion; sampled pixels substantiate captured paint, not a proof of every hit target.

## Reproduction

With the existing product, Python driver and nested-KWin dependencies present:

```sh
QA_PORT=29665 bash KAN-29/nested-localized.sh
python3 KAN-29/pr2877/patch-product.py
QA_PORT=29666 QA_PRODUCT=floorp-pr2877 bash KAN-29/nested-localized.sh
PYTHONPATH=python python3 KAN-29/analyze-localized.py
```

Ports/profiles are deliberately unique; choose unused ports and adjust analyzer identifiers for reruns. The signed XPI binary, products and profiles are excluded from the evidence commit; URL/hash and installation script are supplied.

## Interpretation and remaining conditions

Actual localized RTL demonstrates the related directional defect and its improvement with the existing PR2877 calculation. The previous ordinary LTR24-round run remained unreproduced. This does not establish a fix for original LTR CachyOS/Flatpak report2876, physical input, other compositors/GPU or packaging. No duplicate PR, merge, release, status or Done change.

## Existing regression tests on current Runtime

Valid run29687: existing PR `flexOrder.test.ts`9/9 pass; current `panelSidebarLayout.test.ts`8/8 pass on actual official Runtime159.0.1/build20261103153232, with the candidate logic. The panel test file at PRhead and baseline has identical source hash. Tests bind to actual production module singletons/signals and actual Solid; assertions are unchanged. Test-only product copies expose the necessary internals and package external chrome test modules; this separate test adapter does not change the pure PR-logic copy used for UI matrix. No eval, inline-script permission relaxation or CSP/security preference changes are used for the valid run. Earlier module injection/path failures remain harness failures, with logs. See `pr2877/29687-flex.json`, `29687-panel.json`, test-copy/source hashes and eventual `pr2877/report.md`.

Original-product control run29688 completed all9 existing flex cases and failed exactly2: boxless offset expected0px/actual1366px; RTL expanded-anchor stability. Candidate29687 passes the same9. This is an actual browser/source-test comparison, distinct from the genuine Arabic UI matrix. The regression tests themselves deliberately set direction fixtures to cover both directions; the final UI matrix separately proves RTL_UI=true.
