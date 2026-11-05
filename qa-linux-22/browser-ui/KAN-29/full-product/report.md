# KAN-29 exact PR2877 complete-product verification

Result is **partial, not a full RTL product PASS**. Complete unmodified package assets were executed, with no diagnostic asset replacement, native/source/lock edit, root-dir fallback, signature/security disabling or renderer change. Existing PR2877 remains canonical; no new PR/merge/release/Done.

Baseline is official12.20.0 exact parent a6290b5811b8c5309b399ac340ec40b52f97b73c. Candidate is exact f43b22d34c0728e0ed800b8016d81b3212190954, standard package run42115797905/attempt1 (tar SHA2560e21b01b50834fbecea0ebedc7c155b414ccf1d4025c25c9f792463a7abde991). Same actual native157.0.1/App+Platform Build20261007125927 and native executable/libxul digests; each actual GreD and four input payload digests gated before collection. Authoritative package manifests/acquisition are separate from actual live metadata. Every measured full product tree remained byte-identical afterward.

## Actually collected

| Complete product/backend | LTR | RTL | Actual executable child exits | Runner / DBus wrapper |
|---|---|---|---|---|
| Parent baseline/nested Wayland |8states,24hover,16tab selection+paint,8close/reopen PASS|No states; NewSession45s timeout|LTR0/install0; firstRTL UNKNOWN|1 /1|
| Baseline RTL-only resume/nested Wayland|Not replayed|No states; NewSession90s timeout|RTL-9, normalQuitRequestedfalse|1 /1|
| Candidate/nested Wayland|8states,24hover geometry PASS;16tab selection+paint PASS; close/reopen7/8|No states; QA memory bound during startup|LTR0/install0/RTL-15|1 /1|
| Candidate/X11 Openbox|Only native-right/panel-right/unpinned:3hover+2selection/paint PASS; reopen remained0|No states; same QA memory bound|LTR0/install0/RTL-15|1 /1|
| Parent baseline/X11 Openbox targeted control|Same single state:3hover+2selection/paint and reopen PASS|Not started, explicitly prohibited|LTR0|0 /0|
| Parent baseline/X11 500ms-hold case|Same single state:3hover+2selection/paint PASS; reopen remained0 due actual native tab-content hit|Not started|LTR0|0 /0|
| Candidate/X11 500ms-hold case|Same single state:3hover+2selection/paint and reopen400 PASS|Not started|LTR0|0 /0|

Native Wayland and X11 are separate backends on Debian13 virtual displays, not original CachyOS/Flatpak or physical input. W3C mouse input is synthetic even when browser events are trusted. X11 used MOZ_ENABLE_WAYLAND=0 as backend selection only; both graphics snapshots report Software WebRender without test renderer override.

Startup vertical/hover/design/panel400px preferences, native side-position API and placement of standard Sidebar widget are explicit inherited QA fixtures. Pin/open/close/tab selection use actual UI clicks. Two controlled real data-page tabs are an API fixture; no handler/style repair or forced render/focus state. Official matching157 Arabic pack was downloaded through normal proxy/TLS and installed using normal AddonManager. Each install reported signedState2/appDisabledfalse, followed by requested normal quit/direct child wait0. Arabic native paint alone was never substituted for live RTL_UI/appLocaleRTL/computed-direction and protocol gates.

## Targeted close/reopen observation

Candidate X11 trusted close click reached panel-sidebar-close and closed width400→0. The requested reopen point1258,59 initially hit a bookmark image, but during100ms pointer movement the native sidebar's drag-to-pin promo card became the actual trusted mousedown/up/click target. Width stayed0 at0.3/0.8/1.5s. This is observed input occlusion with cause unisolated, not declared harness-only or proven product bug.

The one authorized exact-parent X11 control used the same requested route/duration/settle, fresh en-US and matching before-close/after-close rectangles. Its actual trusted events reached the bookmark-image descendant and reopened400px at all three times. This bounded sample difference warrants follow-up; it does not alone establish PR causality or a deterministic new regression. Actual hover microtiming was not independently timestamped. Candidate used synchronous PSS reads; baseline had the monitor responsiveness correction, a possible scheduling confound. No retry/hover-avoidance was used to rewrite the failed candidate observation as PASS. See x11-close-reopen-comparison.json.

### Authorized500ms-hold comparison

One further fresh case each inspected actual hit/rect/CSS immediately after movement and after500ms, then clicked once. Results reversed: baseline hit native tab-content and remained0 at0.3/0.8/1.5s; candidate hit the bookmark descendant and reopened400px at all times. Both actual child/probe/worker/wrapper exited0 after requested normal quit, and product trees stayed unchanged. No immediate case was replayed or overwritten.

Baseline immediately after movement showed promo-card/native sidebar in-flight beyond the rail: sidebar rect right1404.8 on1280px viewport, z-index4. After507ms, promo-card was hidden but sidebar still had ongoing-animations and rect right1336.9, so500ms was not a settled-state guarantee. Candidate after movement and524ms hold was collapsed/stable at right1238, with bookmark hit. Stable physical offset remained42px for both. This proves input occlusion exists before PR2877 and weakens a deterministic candidate-only regression interpretation; transient root cause remains unisolated. See x11-hold500-comparison.json. No new fix was made; the later explicitly authorized horizontal control is recorded below. No additional browser is now permitted.

Static source comparison: in genuineLTR the new physical-side direction expression equals the old branch; only zero-rect skipping can differ. Stable captured offsets/rects were identical before and after close. A transient zero-sized element/observer/native hover animation interaction is a hypothesis, not demonstrated cause. Source patch and static analysis are retained; no production source was changed.

## RTL startup/resource limitation

Neither complete baseline nor candidate passed the actual RTL session gate, so full-product RTL8states/24hover and side effects remain untested. Candidate main-process PSS grew during Arabic startup to12.44GiB under nested Wayland and12.83GiB under X11, dominant in own aggregate12.51/12.91GiB. The QA bound (owned measured PSS12GiB or cgroup90%) stopped both before any new OOM event; event increments were0. This establishes measured own-process growth and a QA ceiling, not original-host inadequacy or root cause in PR/native/locale/profile/NewSession.

Wayland emergency CtrlQ failed to load libxdo under the KWin dependency path; no normal quit was delivered. X11 recovery fixed that loader path, recorded command exit0, but Floorp did not exit within8s. Both were then bounded own SIGTERM cleanup, actual child wait-15, normalQuitRequestedfalse, never normalPASS. The baseline resume direct wait-9 is preserved; first baseline RTL UNKNOWN was not retroactively filled in.

Original monitor requested2s samples, but synchronous PSS reads delayed completion (X11 maxgap62.41s); actual timestamps and executed monitor source are preserved. Future sampler separates slow PSS reads from fast cgroup/statusRSS. A QA-only test blocking PSS7s completed5 fast samples/maxgap2.116s; this is harness evidence, not additional product execution. Resource logs must not be described as guaranteed2s cadence.

## Exit and evidence semantics

Inside DBus, Python directly launches the actual Floorp ELF, records PID and /proc/PID/exe and saves direct Popen.wait receipts per phase. Separate measure.py probeExit, worker runnerExit, and outer dbusWrapperExit are recorded independently. Wrapper0 is never inferred as Floorp child0. LTR/install normal exits have explicit normalQuitRequested; interrupted/resource exits retain their actual negative statuses.

Prelaunch RTLresume30072 created no browser and its directory error is preserved. Other bounded attempts remain immutable. Scripts executed for Wayland/X11 and original monitor have snapshots. The later authorized500ms pair, passive diagnostic and horizontal-startup control are recorded below. All browser tests have now ended; no additional browser is permitted. Product archives/binaries are local and not published. Root owns Jira/QA branch updates; prior Jira10284 and PRcomment6663548952 remain context, not proof of these new results.

Main artifacts: matrix.json, baseline-summary.json, candidate-summary.json, candidate-x11-summary.json, x11-close-reopen-comparison.json, candidate-resources-summary.json, candidate-memory-curve.png, and each runs/* raw measurements/screenshots/ownership/child-exit/runner/hash manifests. Original distro/physical conditions and full-product RTL remain unmet. Source tests/diagnostic copies were not rerun.

## Final passive and horizontal-control limits

The other agent's passive-no-session run made zero TCP/RPC/NewSession connections (CLI --marionette retained). At54.66s it hit QA resource guard at ownedPSS12.273GiB/cgroup13.918GiB, OOM increments0. Actual child115562 exit-15, normal quit not established,46 payload entries unchanged. Strict live locale/RTL_UI and active renderer were not queried. The source was a copied stopped profile with prior resource-stop history, not a clean preRTL snapshot. Growth without NewSession is observed in that setup; strict root cause is not isolated. See passive-no-session/report.md and summary.json; separate runner/wrapper exit values are not supplied there and are not inferred.

The last authorized f43 route copied the already signedArabic profile independently; source prefs/user/extensions hashes remained unchanged. Startup native verticalTabs=false/designhorizontal was a non-target control, locale/protection/renderer unchanged. Native screenshot showed horizontalArabic chrome, but NewSession never passed strict live/locale gates, so neither validated horizontal success nor HubUI vertical transition occurred. Actual child117008 reached measured mainPSS12,583,471KiB/ownedPSS12,665,986KiB (about12.08GiB aggregate), cgroup14,572,691,456B, OOM increments0. QA guard fired; CtrlQ command0 did not cause exit within8s, own bounded SIGTERM followed. Direct child exit-15/normalQuitRequestedfalse, probe never started, worker1/DBus wrapper1, all payload bytes unchanged. See horizontal-control-summary.json and runs/candidate-f43-ar-horizontal-ui-30080/.

Final qualified full-product targetRTL states: **0**. Core LTR geometry total48hover and32tab selection/paint successes are retained; candidate initial close/reopen remains7/8, with later opposite input-occlusion outcomes preserved rather than rewritten. Original LTR CachyOS/Flatpak and physical-input conditions remain unproven. Niri belongs to another issue and is not a KAN29 original condition. Existing source CI reported248pass/1fail; it was not rerun here and does not prove full-product RTL success. No new source fix/Draft, merge/release/Done.

Final-process-audit.json found zero active references to all owned ended profiles. This worker deleted no profiles/products/build outputs; source/copy supply relationships remain preserved. The separate passive agent has its own cleanup receipt. All actual trials are finished and additional browser startup is prohibited. Final matrix.json contains9 distinct variants, child/probe/worker/wrapper statuses separately, and GitHub original issue URL.
