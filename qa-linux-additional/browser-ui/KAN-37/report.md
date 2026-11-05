# KAN-37 / GitHub 2754 — additional QA, separate from Linux22

Source review (not a build claim for this official release binary): latest main `29a3eb23145da90706d8a94441373278521cc737`, clean detached `/workspace/floorp-kan37`. Official Floorp 12.21.1 / Gecko159.0.1 BuildID20261103153232, Debian13.6, private Xvfb99. Firefox159.0.1 is the native-page control on private Xvfb101. No shared product or branch edits.

Original issue 2754 reports empty shortcut thumbnails on Linux and Windows12.18.0; its three comments do not supply a current-version reproduction. Screenshot resembles Firefox classic New Tab, unlike fresh default Floorp Start. Related2997 reports Windows12.21.0 and says changing to Lepton restored normal display; same cause is unproven. Jira10084 was read, with no earlier actual test. Searches for PR2754/2997 found none; thumbnail search finds3003 (separate New Tab redesign), not a verified fix. Reporter signatures and notification tokens from2997 were deliberately not saved.

Fresh Floorp defaults to Lepton plus Floorp Start. Initial remote shortcut images failed to decode because the browser did not use this Cloud environment's session proxy. The managed policy is unrestricted/enforced. The public Google s2 endpoint was reachable via inherited curl proxy (301 to gstatic). The isolated browser profile was configured to the documented proxy:8080 and imported the environment-distributed CA certificates with normal NSS trust. TLS verification, browser sandbox and signing preferences were not disabled. This corrected remote image loading; the earlier missing remote images are a setup failure, not a reproduced product bug.

Actual UI operations: Floorp Start Settings → Disable Floorp Start checkbox, restart; Hub → Tab & Appearance → Proton radio; both used synthetic W3C pointer input. Normal OS XTest Ctrl+T was also tried. Classic New Tab loads vendor/activity-stream bundles but its entire React root remains empty, with parent App.initialized=true and content App.initialized=false. Fresh Firefox control behaves the same, including normal Ctrl+T and corrected proxy trust. This whole-page initialization limitation differs from the original screenshot's existing empty tiles and must not be called the reported thumbnail bug. Diagnostic native service/actor lookups were performed and labeled as such; they did not restore rendering. The native selfLoading preference was briefly tested false, had no observed effect, and was cleared back to the product default true. No renderer was manually invoked.

Floorp Start positive control: a 64×64 four-color PNG was persisted through native NewTabUtils pinnedLinks (explicit API data fixture, not UI-created or remotely fetched), followed by native pinnedCache expiration and TopSites refresh. Unchanged production Floorp Start received it through the normal parent-actor Top Sites API. The local fixture is an embedded data image, not an HTTP fetch. No production function, compiled asset, DOM image source or CSS was replaced. `local-fixture.py` and `fixture-favicon.png` reproduce this fixture.

`proton-initial.json/png` confirms both real remote sponsor/support images decode64px, four native bundled icons decode32–48px, and the local fixture decodes64px. Actual screenshot samples match all four exact colors: red [255,0,0], green [0,255,0], blue [0,0,255], yellow [255,255,0]. Product signatureRequired=false is its existing default, userSet=false; sandbox6; selfLoading=true, userSet=false. No successful default-signature=true claim is made.

## Completed comparison

| Design | Conditions | Images decoded and visible | Local exact four-color paint |
|---|---|---:|---|
| Proton | initial, reload, native cache clear + reload | 21/21 | 3/3 |
| Fluerial | initial, reload, native cache clear + reload | 21/21 | 3/3 |
| Lepton (default) | initial, process restart, reload, native cache clear + reload | 28/28 | 4/4 |

All 10 actual-paint conditions / 70 image-decode checks passed. The embedded local favicon persisted across a real browser-process restart; external Google s2 images and bundled native icons also remained displayed. Conditions have independent JSON and actual screenshots. CSS display/visibility and decoded dimensions were measured; no DOM image/CSS replacement was used. The image paths and exact screenshot colors remained consistent across the tested designs.

The first intended Lepton click targeted an offscreen hidden radio and actually chose Fluerial. This was detected from the independently recorded environment.design; those three captures were renamed Fluerial, not claimed as Lepton. Clicking the visible label then verified actual Lepton before its four valid conditions. UI creation/toggling and all automated input remain synthetic, not physical.

Host-wide execution/read delays occurred after the first saved positive capture. These were tooling/resource pressure, not claimed browser crashes. Firefox was stopped before the last sequential Floorp restart. Memory fell from16.28GB to14.62GB after stopping the owned browser. All private browser/Xvfb groups were cleaned up; no shared process was terminated.

Classification: **partially tested**. Floorp Start image retrieval/render/cache/restart works under these tested conditions after correcting Cloud networking. The original classic-page thumbnail condition remains unverified because the whole native page did not initialize in either product control. Do not label the entire issue fixed or globally not reproduced. No speculative fix or duplicate PR has been made. Original classic-page profile, Linux desktop/device conditions and Windows remain unverified.

Reproduction scripts and raw measurements are adjacent. Test profiles, NSS databases and browser caches remain outside publishable QA under `/workspace/kan37-runtime`. Jira is updated by the parent, not this child.

Reproduce positive control: `QA_PROXY=1 python start.py`; with `PYTHONPATH=/workspace/qa-linux-22/browser-ui/python`, run `python local-fixture.py`, then `python capture.py lepton-initial`. Use visible Hub design labels for changes and verify each capture environment.design. Restart the owned runner/profile for `lepton-restart`, then `capture.py lepton-reload` / `lepton-clear`. The local fixture URL is an identification URL only; its favicon is embedded and no localhost HTTP server is needed. `finalize.py` asserts all saved conditions and records product hashes.
