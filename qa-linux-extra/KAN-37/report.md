> Follow-up 2026-11-06 JST: original Linux12.18 native qualification now succeeds with the authorized QA-profile test-pref. See [reported-fixed-version/report.md](reported-fixed-version/report.md). The earlier narrative below is preserved as historical evidence; original-thumbnail and Firefoxpolicy uncertainties remain.

# KAN37 / GH2754 — Linux native NewTab initialization follow-up

Recorded2026-11-06 JST (host2026-11-05UTC). **The current official Linux Floorp native-page initialization limitation is cleared under the normal startup contract.** Classic `.newtab-main` renders; its native pinned favicon paints exactly in7 qualified conditions. This is not a reproduction or global resolution of the original missing-thumbnail report. Firefox fresh-control whole-page absence is recorded separately. No product patch or Draft PR is justified by these observations.

## Prior work and immutable scope

Read Jira10084/10384, GH2754 and all3 comments, related2997, and prior QA6031f8efce3953c7d8ef18b09819f08a90815f17 / qa-linux-additional/browser-ui/KAN-37/report.md. Existing Floorp Start70decode/10paint was **not repeated**. Only its settings UI was used to choose native NewTab. Related3003 is a separate redesign, not a proven thumbnail fix. Coordinator's Windows14native paints/Jira10459 are separate evidence and were not rerun.

Read-only independent source worktree qa/kan37-linux-native at latestmain29a3eb23145da90706d8a94441373278521cc737, AGENTS read. Untouched official current archive product source984509d2053be7bae9027133bda7259cd706b208, actual12.21.1@159.0.1/nativeBuildID20261103153232. Firefox official159.0.1/nativeBuildID20261102144711. Products are separate builds/distributions, not an identical native package. Current Floorp46 and Firefox51 file/symlink entries exactly match their original prefixes after tests. No source/compiled-resource/DOM/CSS renderer substitutions.

## Initialization contract and measured result

Before browser startup, fresh task profile sets `remote.prefs.recommended=false`; retains configured proxy and regular NSS CA trust. This opts out of automation default changes, **not browser security enforcement**. Normal startup, actual Floorp Start Settings checkbox `Disable Floorp Start` via WebDriver, normal Quit/code0, same-profile process startup with about:newtab. JSON setting preserves the established schema; design is independently read from `globalConfigs.userInterface`. No hand-seeded incomplete design configuration.

Native current page: `.newtab-main=true`, #root innerHTML18437 before fixture,5 `.top-site-outer` elements before fixture. Parent ActivityStream naturally present/initialized, full26–27 feeds (including feeds.newtabinit) rather than the prior2-feed configuration. Fixture adds one pinned site, giving6 counted elements including native add control. #root length/layout are observable state, not injected markup. Source audit shows recommended automation defaults set `browser.newtabpage.activity-stream.testing.shouldInitializeFeeds=false`; full normal feed initialization uses a true fallback when that pref is absent. Applying settings after initialization does not restore omitted feeds. This provides a concrete automation confound consistent with the prior empty whole page; the broad remote-recommended switch changes multiple defaults, so this is not a single-pref causal isolation or Linux renderer bug finding.

Local64px red/green/blue/yellow PNG persisted through actual NewTabUtils.pinnedLinks, native pinnedCache expiration and TopSites.refresh. This is a native API data fixture, not UI-created content or an HTTP/favicon retrieval success. TopSites had to be naturally initialized; test never calls AboutNewTab.init/onBrowserReady, content renderer, feed constructor or fake policyready/observer notification. Theme changes use the supported full design preference; no CSS replacement. Actual Xvfb framebuffer captured with PIL, native icon bounding rectangle + mozInnerScreenX/Y + DPR give the4 sample coordinates. All4 quadrant pixels exactly match RGB255 values.

| Native current design | Conditions | Exact paints |
|---|---|---:|
| Proton | reload, native cache clear+reload | 2/2 |
| Fluerial | reload, native cache clear+reload | 2/2 |
| Lepton | reload, native cache clear+reload, normal process restart | 3/3 |

These7native paints are separate from the older70 Floorp Start decode checks. Main qualified sessions: floorp-seed-v2, floorp-native-v2, floorp-native-restart, firefox-policy-v2; all4 normalQuit/exit0. Raw pixels, rectangles, design state, native-page evidence and screenshots are exported. Process restart reused actual pinned storage and preserved the native-page setting.

## Firefox and real policy differences

Firefox fresh profile with the same pre-start remote recommendation opt-out/proxy/CA: AboutNewTab.initialized=true but activityStream=null, #root0, no .newtab-main/content-main. Actual default distribution.id=mozilla-official, TOU current/minimum4, acceptedVersion0/date"0", bypassNotification=false, all without user acceptance overrides. Enterprise policy status0/no active keys. No TOU acceptance UI was clicked; no policy signal or readiness promise was manufactured. This evidence is compatible with native policy gating but **does not prove TOU is the cause**: the ready signal could precede the late observer and addon/Nimbus/ProfileAge prerequisites were not fully traced. Firefox remains an unqualified native-page image control, not an image-rendering negative result or Floorp-specific defect.

Floorp's official packaged browser/omni.ja defaults/preferences/firefox.js itself declares termsofuse.bypassNotification=true. Observed acceptedVersion0/date"0", distribution.id absent, user overridesfalse; no test writes bypass or acceptance. Linux policy eligibility checks default distribution.id prefixmozilla. Do not describe stock Floorp policy state as genuine user consent or a bypass introduced by this test. Packaged source excerpt/hashes saved. The corresponding AboutNewTab/Redirector/TelemetryReportingPolicy/RecommendedPreferences modules in current Floorp/Firefox are byte-identical, despite different prefs/distribution behavior.

Passive observers attached after Marionette connection; recorded native newtab-top-sites-changed after real pinned-data update. No observed policyready event after attachment is not proof no event fired. Console filters and raw native states are preserved without credentials/headers.

## Original12.18 Linux attempt and setup failures

A dedicated copy of the already hash-verified official12.18.0 archive (source1728a7678c34aec1ad502edb06a168fdf2a07be8) was tried; no MAR comparison rerun or updater command was requested. Its seed reports native156.0.1/build20260916130640. Under normal settings, the owned copy automatically updated; next native attempt reports159.0.1/build20261103153232 and logs `CPD: got update status: succeeded`, outer INI12.21.1. File comparison records this actual mutation; the original archive/reference prefix remained untouched. That attempt **does not qualify12.18 native initialization or paints**. Updates/security protections were not disabled to hold the old version. The actual-current unexpected-version attempt hit a5s content-handle timeout, lost its automation session and cleanup terminated it(-15); neither an original-version thumbnail failure nor a proven native crash/hang. No qualified paint uses it. Original Linux12.18 native context remains unfulfilled; coordinator's original Windows evidence is separate.

Earlier scalar/default-branch pref probes threw when a user-only pref had no default getter value; these are harness read errors, not policy/product failures. Corrected passive reader records unavailable default values and used new v2profiles. Two initial startup captures were invalid and cleaned up, not counted normal product tests. An old native launch raced previous owned HTTP-server close and failed address-in-use before browser launch; unique ports/display fixed the harness setup.

Native default-browser prompt dimmed2 preliminary framebuffer samples to127RGB. Escape dismissed that normal UI without changing host default browser, then verified captures returned255RGB. The two occluded samples are excluded from7qualified paints. Original first occluded PNG was overwritten by the verified retry's same filename; raw127sample is retained with screenshotRetained=false. The restart occluded screenshot is preserved separately. Neither is classified as missing thumbnail. Correct positive capture is associated with the native-prefixed command ID.

## Remaining scope

Debian13/kernel6.18.57 x86_64, private Xvfb/Openbox/DBus, synthetic input, no DRM/NVIDIA/dxg device. Original Linux desktop/profile/custom thumbnail source/cache history and physical GUI are unmatched. The original image failure remains unreplicated; native current local-data paint does not establish every remote screenshot/favicon path. Original Linux12.18 native test was invalidated by ordinary auto-update. Firefox readiness/TOU causal boundary unresolved; no consent bypass to force it.

Current Linux whole-page initialization is now qualified; keep Windows-native fulfillment from10459 and original-thumbnail-context/needs-repro labels. No merge/release/Done. Original Linux22/C5 denominator, KAN50/77/86 evidence and KAN102 product hold unchanged. This additional KAN37 entry is separate from original Linux22 and does not overwrite prior QA6031f8ef.
