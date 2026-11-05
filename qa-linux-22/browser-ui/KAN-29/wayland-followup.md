# KAN-29: panel-open repeated-hover/pin follow-up

Official Floorp12.21.1 / Gecko159.0.1 / BuildID20261103153232; baseline main984509d. Debian13.6, dedicated nested KWin Wayland, actual product Troubleshoot windowProtocol=wayland, Software WebRender. Product implementation was not changed. Synthetic W3C input, not physical hardware.

## UI and fixtures

The Bookmarks panel body is opened by clicking #default-panel-bookmarks, and stays400px wide. The standard Sidebar toolbar widget is placed using CustomizableUI's normal placement API because it was absent from this profile's toolbar; pin/unpin then uses real W3C clicks on #sidebar-button. Startup preferences choose vertical tabs/expand-on-hover and design; side selection uses actual prefs plus SidebarController.setPosition. These configuration/widget-placement APIs are fixtures, not UI-originated side-setting changes.

LTR is the real current UI direction. The intl.uidirection string did not make this product a localized RTL session, so the RTL comparison explicitly sets the root dir attribute. RTL_UI remains false. This is a directional layout fixture, matching the type of layout boundary covered by PR2877, not an Arabic/Hebrew locale or proof of original-OS RTL behavior.

Eight states per direction cover native side2 × Floorp panel side2 × native launcher pinned/unpinned2. Each state attempts three leave/hover rounds. Full PNG screenshots and four time samples per round capture actual paint and rectangles. Panel body is open in all measurable states. The pin variable here is the native vertical-tab launcher; the Floorp Bookmarks panel remains click-pinned throughout.

## Results

- LTR:24/24 rounds measurable. Unpinned launchers finish expanded at about253.33px, pinned about252.33px. After normal transition, the final two samples differ by at most1px. All measured panel widths are400px. No repeated-hover failure matching the original LTR CachyOS report was reproduced.
- RTL fixture: actual logical offsets become1366px in the1366px-wide browser. Hover outcomes include collapse/repositioning and one temporarily unreachable anchor. Pinned states remain reachable. These observations are not classified as the original LTR reproduction or as a fully localized RTL defect demonstration.
- One focused RTL geometry sample includes the missing6px splitter. A read-only recalculation using PR2877's physical-edge/empty-rect algorithm gives448px while the unchanged product publishes1366px. This directly correlates the excessive-reservation boundary with PR2877. The PR implementation was NOT injected/applied, and candidate paint/fix success is not claimed.

Existing DraftPR2877 headf43b22d34c0728e0ed800b8016d81b3212190954 remains the related implementation. Its own original-LTR unproven limitation remains valid; no duplicate PR was created.

## Reproduce and evidence

`../kan29-wayland-run.py`, `../kan29-wayland.py`, and `nested-panel.sh` recreate the sixteen-state UI probe. `nested-panel-measure.sh` limits the extra splitter measurement to one RTL state. Adjust dedicated sysroot/runtime paths for another host. `analyze-wayland.py` reads PNG pixels and derives the summary without altering images.

- wayland-panel-29603.json / wayland-panel-29604.json: completed LTR/RTL fixture matrices, error=null.
- wayland-panel-29605.json: one focused geometry measurement.
- wayland-summary.json: rectangle/pin/paint samples and per-round outcomes.
- pr2877-geometry-correlation.json: read-only candidate-algorithm calculation.
- wayland-29603-*.png / wayland-29604-*.png: final state paints.

The first attempt29601/29602 stopped after a missing Sidebar toolbar UI target. Those partial traces are retained as harness setup failures and not counted as product failure or completed pin comparisons.

Remaining conditions: original CachyOS/Flatpak/distro package, localized RTL runtime, physical hover/input, and PR2877 candidate rendering in the tested current environment. Keep the issue open; no Done/merge/release transition.

## Native-direction attempts (not counted as localized RTL)

Additional direction preflights/attempts were retained instead of claiming the root-dir fixture was native RTL. `intl.uidirection` string rtl and integer2/1 left the initial product direction=ltr and RTL_UI=false. Trial29606 therefore fell back to the same explicit root-dir fixture;29607 measured direction only and skipped the layout matrix.

A normally installed Mozilla Arabic language pack for159.0.1 was signature-valid (signedState2), type=locale, isActive=true, requestedLocales=[ar], signature-pref user value absent. After restart, the product still reported appLocale=en-US, appLocaleRTL=false, direction=ltr and RTL_UI=false. Trial29608 is therefore also a root-dir fixture, not a successful Arabic UI run. The cause of that locale registration/selection outcome was not established or treated as a product vulnerability/bug in this assignment. No protection preference was disabled. The original localized RTL condition remains unverified.

`locale/setup-29608.json` and the initialDirection fields of wayland-panel-29606/29607/29608.json are the concrete capability evidence. The public language pack URL is recorded in setup JSON; SHA256 is3b5d819d581c56a422c8525f207cb00e622e10ea2fca0c97a58dfeae071c26c8. The binary is not needed in the evidence branch; retrieve and verify it from that public URL when reproducing. `nested-panel-arabic.sh`/`../kan29-locale-run.py` show the exact normal install/restart route.
