# Evidence for the existing PR2877 owner/root

Do not modify the PR head as part of this QA follow-up. Verified open Draft head: `f43b22d34c0728e0ed800b8016d81b3212190954`. No duplicate PR or merge.

Suggested addition to the existing PR description after reviewing the linked evidence:

> Additional Linux native Wayland validation on official Floorp12.21.1 / Gecko159.0.1, baseline source984509d: a normal signed Arabic language pack with graceful app restart establishes actual appLocale=ar and RTL_UI=true. Both original and an independent diagnostic product copy containing only this PR's offset calculation completed8side/pin states ×3hover rounds with the real Bookmarks panel body open400px. Original reserves1366px in all24rounds and has4late anchor movements; candidate reserves448px and has0late movements. Screenshots show the same-side unpinned tab contents paint correctly with the candidate. The candidate copy is not a full official candidate build. The previously reported ordinary LTR CachyOS/Flatpak symptom remains unreproduced and is not established as fixed.
> On current Runtime159.0.1/build20261103153232, existing flexOrder9/9 and panelSidebarLayout8/8 pass with the candidate. Original fails2of the same9flex tests (boxless1366px and RTL hover anchor). Tests bind to real production module singletons/Solid through separate diagnostic exports and external chrome test modules; assertions/CSP/browser protections are unchanged. The original PRhead's panelSidebarLayout source matches current baseline.

QA branch: `qa/linux22-browser-ui-evidence-20261105`, files under `qa-linux-22/browser-ui/KAN-29/`:
- `localized-report.md`, `localized-comparison.json`, raw29665/29666matrix and16screenshots: actual Arabic RTL UI, no root-dir override.
- `locale/setup-29665.json`, `setup-29666.json`, signed pack URL/hash, normal installation/restart script.
- `pr2877/product-patch.json`, `patch-product.py`: exact candidate scope/nativebinary and ZIPentry hashes.
- `pr2877/29687-flex.json`, `29687-panel.json`, `29688-flex.json`: unchanged existing regression assertions on live currentRuntime. These source tests include their own direction/configuration fixtures, separate from genuineArabicUI.

All input is synthetic. Original CachyOS/Flatpak/distro, hardware GPU, physical pointer and full candidate build remain conditions. Jira KAN29 comment10284 updated; status/parent/Done unchanged.
