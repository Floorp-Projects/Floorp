# KAN-30 / GH2907 — final Linux browser UI QA

**Current official release reproduces the hover/tab-switch defect. Canonical PR2934 fixes the tested Linux behavior; a complete current-main integration package passes the same real UI conditions.** No merge, release or Done transition.

## Sources and submission

Latest main29a3eb23145da90706d8a94441373278521cc737 was checked in a clean independent worktree. Canonical2934 remains author EldinBegano's fork head b7866aaa4905be4d86fb3dff1237d6a70c483f10. Author branch untouched: no ordinary fork push permission or agreed branch workflow. Owned immutable base mirror points exactly there. Dependent Draft [3043](https://github.com/Floorp-Projects/Floorp/pull/3043), headbfb8e5b583e1d8520e8e894e7480276f0250d0f2, adds only44 lines to one existing regression file. It is a test submission dependent on2934, not a replacement product fix.

Full package source is current main plus precisely2934's one-line blur→deactivate change: [1b8f3503df9e8cae6b08118ecbe2d85e2d5158e7](https://github.com/Floorp-Projects/Floorp/tree/1b8f3503df9e8cae6b08118ecbe2d85e2d5158e7). Old PR source's Runtime158 was not mixed with159 and called an exact-head build. CI tests the exact old PR-based test branch; full package tests the current159 integration. See full-product/report.md and provenance.json.

## Actual OS input and paint

Original report: Windows10 22H2 / Floorp12.20.1@157.0.1. Cloud: Debian13.6 / private Xvfb109 + Openbox EWMH + DBus, official Floorp12.21.1@159.0.1, native build20261103153232. Input is synthetic OS XTest/W3C, not physical. Normal design preferences select overlap+hover; controlled QA tabs are setup fixtures. No visibility/CSS/hover state overrides.

| Condition | Official baseline | Compiled-listener diagnostic candidate | Complete production package |
|---|---|---|---|
| Ctrl+Tab, hover only |29944 opacity0 despite hover |29945 opacity1 |29950 opacity1 |
| Ctrl+Tab, sidebar focused |29944 opacity0 |29945 opacity1 |29950 opacity1 |
| Normal signed Sidebery toolbar and actual row click |29946 QA A→QA B, opacity0 |29947 QA A→QA B, opacity1 |29950 QA A→QA B, opacity1 |
| Pointer leaves |29944 closes0 |29945 closes0 |29950 closes0 |
| Real native window deactivation |29944 trusted event, closes0 |29945 trusted event, closes0 |29950 focusTRUE→FALSE, trusted event, closes0 |

Normal Sidebery runs do not use showInitially or a forced panel-selection API. Signed5.6.1 setup uses normal AddonManager signature-verifying file installation, not a claim of human installation dialogs. Row text paint remains blank in Cloud; actual selection and cyan page paint independently prove the clicked row selected QA B. This separate limitation is recorded, not guessed to be a product defect. native-summary.json, full-product-ui-summary.json, raw probes and screenshots preserve evidence.

Marionette normally sets focusmanager.testmode=true, allowing background focus and invalidating native deactivation tests. Fresh qualified profiles set false to restore real native focus; no security protection disabled. The full package has signatureRequired=true/default, sandbox6; official baseline's false signature value was its existing default/userSet=false. Native events and _NET_ACTIVE_WINDOW were measured. Bare-Xvfb/twm no-event attempts remain unqualified historical observations; see native-followup.md/wm-control.json. Earlier report versions are available at6031f8ef and must not override these completed conditions.

## Strict full package and CI

All production targets rebuilt, standard configure/artifact build/after-mach passed; no native recompilation. 1115/1115 production assets match. XPT155 modules/801 IDLs/903 input dependencies match genuine regenerated inputs. Canonical packager/error-if-empty checks retained; mach package exit0. Archive SHA256388f2543413cc44783e8fcd114a4149081394793693ecb79924a3137604f5bb3. All57 archive members match standard staged payload type/content; this actual staged payload was run, not an independently extracted archive. Hashes and archive/stage identity remain unchanged after UI. Binaries/archive stay local; source, scripts, manifests and small proof are retrievable.

Draft3043 automatic CI42088237697 succeeded: smoke and all4 native platform jobs. Linux artifact ZIP SHA d08245e6267020626b1d60c165f5b46c76b2e07d5ad1b38016055f128d666c35 matches GitHub digest.261 test modules pass/0 fail; downloaded Firefox8 files/16 tasks pass. sidebarOverlay.test.ts passes16549ms. The new case is unconditional in runAllTests and the harness aggregates/throws failures; artifact logs module results rather than individual case lines. See ci-3043/summary.json and verified-excerpts.txt. No redundant CI rerun.

The isolated synthetic DOM regression earlier failed baseline29935 and passed candidate29936; the minimal integrated exact-head test now also passes standard CI, removing the prior QA config-adapter integration uncertainty. Real native/paint tests are separate from synthetic DOM regression.

## Remaining and reproduction

Original Windows10 22H2, original hardware and physical input remain untested. Historical Runtime158 full package was not generated; its exact PR-based branch passed native browser CI, while the complete current-main159 integration package is independently qualified. Current released/main baseline is still defective until canonical fix is reviewed/integrated; neither PR was merged.

See manual-repro.md; run.py/probe.py; design-fixture.json; dependency-setup.sh; download-sidebery.py. Full package reproduction/provenance is in full-product/. Runtime profiles, credentials, caches, binaries, archives, raw CI logs and signed URLs are not published. Additional matrix is separate from original Linux22. Infrastructure setup failures, approval rejections, bounded disk/cache pressure and valid results are explicitly distinguished; OOM/kill stayed0.
