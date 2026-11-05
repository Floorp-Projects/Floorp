Additional native Wayland validation against unchanged PR head `f43b22d34c0728e0ed800b8016d81b3212190954`, using official Floorp12.21.1 / Gecko159.0.1 (baseline source reference `984509d2053be7bae9027133bda7259cd706b208`) on Debian13.6, nested KWin6.3.6, Software WebRender.

A matching **normally installed, signed Arabic language pack** and normal app quit/restart established **appLocale=ar, isAppLocaleRTL=true, RTL_UI=true** in both runs. This comparison uses genuine localized RTL, with no root-dir override.

With the actual Bookmarks panel body open (400px), both products completed 8 side/pin states × 3 synthetic hover rounds:

| Measurement | Original | PR2877 diagnostic copy |
|---|---:|---:|
| Viewport-sized 1366px offset | 24/24 | 0/24 |
| Measured panel offset | 1366px | 448px |
| Late anchor movement >1px | 4/24 | 0/24 |
| Panel body remained 400px | 24/24 | 24/24 |

Screenshots also show tab contents painting correctly beside the panel in the candidate's same-side unpinned state. [Report and reproduction](https://github.com/Floorp-Projects/Floorp/blob/60896bdc090293609d84cad33e4396434b348019/qa-linux-22/browser-ui/KAN-29/localized-report.md), [raw comparison/locale/graphics](https://github.com/Floorp-Projects/Floorp/blob/60896bdc090293609d84cad33e4396434b348019/qa-linux-22/browser-ui/KAN-29/localized-comparison.json), [original paint](https://github.com/Floorp-Projects/Floorp/blob/60896bdc090293609d84cad33e4396434b348019/qa-linux-22/browser-ui/KAN-29/wayland-29665-1-0-0.png), [candidate paint](https://github.com/Floorp-Projects/Floorp/blob/60896bdc090293609d84cad33e4396434b348019/qa-linux-22/browser-ui/KAN-29/wayland-29666-1-0-0.png).

**Scope of candidate fixture:** an independent copy of the official product contains only this PR's compiled offset-calculation changes. Other ZIP entry contents and the native executable match. No implementation was changed to enter the tested UI states; supported configuration/widget-placement APIs and synthetic UI clicks configure/open/pin the panels. This is **not a full official candidate build**. [Exact patch and hashes](https://github.com/Floorp-Projects/Floorp/blob/60896bdc090293609d84cad33e4396434b348019/qa-linux-22/browser-ui/KAN-29/pr2877/product-patch.json).

The existing regression assertions were also run against the current actual Runtime159.0.1/build20261103153232 through separate test copies bound to real production module singletons/Solid:
- Candidate: **flexOrder 9/9 and panelSidebarLayout 8/8 pass**.
- Original: **2/9 flex cases fail** (boxless offset expected0px/actual1366px and RTL expanded-anchor stability).
- The panel test source at this PR head matches the current baseline. External chrome test modules were used without relaxing CSP or browser protections. Their own direction/configuration fixtures are separate from the genuine Arabic UI comparison. [Regression report](https://github.com/Floorp-Projects/Floorp/blob/60896bdc090293609d84cad33e4396434b348019/qa-linux-22/browser-ui/KAN-29/pr2877/report.md), [candidate9](https://github.com/Floorp-Projects/Floorp/blob/60896bdc090293609d84cad33e4396434b348019/qa-linux-22/browser-ui/KAN-29/pr2877/29687-flex.json), [candidate8](https://github.com/Floorp-Projects/Floorp/blob/60896bdc090293609d84cad33e4396434b348019/qa-linux-22/browser-ui/KAN-29/pr2877/29687-panel.json), [original failures](https://github.com/Floorp-Projects/Floorp/blob/60896bdc090293609d84cad33e4396434b348019/qa-linux-22/browser-ui/KAN-29/pr2877/29688-flex.json).

**The original ordinary LTR CachyOS/Flatpak symptom remains unreproduced and is not established as fixed.** Original distro/packaging, physical input/GPU and a full official candidate build remain unverified. No PR head changes, duplicate PR, merge, release or Done transition were performed.
