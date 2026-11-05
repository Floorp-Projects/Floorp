# KAN-50 historical acquisition/source provenance

Acquired publicly published, non-draft/non-prerelease Floorp v12.16.4 Linux x86_64 archive. Release published2026-07-26T01:41:39Z, release target811a5b821e1d9d47b40f22aee6df5db2254a54b1. Archive98,230,176bytes SHA2565192fcf1ac000e0f5e56d08cb088de799671958573d2fe5d42b8352f1599bdf7 verified against GitHub release asset digest. Metadata/hashes/release-manifest assets were separately digest verified. Retrieved with inherited approved proxy and normal TLS validation. No credential discovery/authentication.

Extracted independent path: ../products/12.16.4/floorp/floorp. This task did **not** execute historical product or modify existing profiles/prefixes. linux-meta declares12.16.4@153.0/runtimeBuild20260725075208, Runtime2d38da4d11be1e0e615f4ddd785ad5e77c95e18d. Outer application.ini explicitly says unused and lists a different BuildID20260725221441; effective process metadata must be measured by trial owner.

## Before/after PR2699 source

PR2699 merged as0f3269b79cbcfebdce59f53dc2586fadbb96c3e2 on2026-08-28T03:08:52+09:00, after this release. Saved official pinned source for release, actual merge first-parent and merge commit, with URLs/SHA256 in source-manifest.json. This chronology and actual code difference establish the older package precedes this real-tab integration change; they do not establish PiP symptom causality without trial/stacks.

At release, browser-init patch onDOMContentLoaded detects floorpWebPanelId, sets domContentLoaded=true and returns before normal tabbrowser initialization. onLoad likewise returns for panel. website-panel-window-child createContentBrowser() (lines212 onward) creates a standalone XUL browser, appends it to panel container and exposes floorpWebPanelContentBrowser. It does not create/select a native tab. Scoped git grep found no fake-gBrowser assignment in release panel module or tools/patches; do not call this a fake gBrowser implementation or assert exact runtime null value from static source alone.

Merge2699 explicitly calls browser-window-domcontentloaded-tabbrowser category before returning on panel DOMContentLoaded; comments require real tab mapping for WebExtensions. Child replaces standalone browser construction with prepareWebPanelTab(globalThis.gBrowser,userContextId), waits for gBrowser.selectedBrowser and marks its real selected tab BMS-webpanel-tab=true. This supplies the exact architectural before/after hypothesis: absent normal tab mapping can break consumers requiring a real tab, while new code provides one. Whether historical PiP error goes through such a consumer needs the original exception/old product run; no universal PiP fix is claimed.

Archive itself plus full release-public API JSON excluded from small-source export list; executable remains directly available for parent trials. No external/Jira/branch publication performed.
