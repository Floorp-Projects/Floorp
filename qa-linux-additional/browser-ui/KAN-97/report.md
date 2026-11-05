# KAN-97 / GH2692 — scroll-action targeting

**A distinct focused-frame routing defect was reproduced and fixed in the diagnostic candidate. The original Windows/HuggingFace report is not declared reproduced or resolved.** Draft [3047](https://github.com/Floorp-Projects/Floorp/pull/3047) is limited to the confirmed defect.

## Prior evidence and duplicate check

Read latest KAN-97 description and comment10093, GH2692 (Windows11 / Floorp12.17.0@154.0; no comments), and related GH2244 comment5394341020 reporting a correct label but no scrolling while Home/End still worked. The original public MP4 was inspected privately: it shows a HuggingFace model-card page, not proof of this controlled iframe condition. Original video/frames and user tab contents are not included in publishable QA; only attachment metadata/hash is saved.

Existing merged2678 concerns web-process scroll actor availability. Current source already has validated directions, allFrames and the existing safeForUntrustedWebProcess opt-in. The current root-page successes are not attributed to2678 without an old-version comparison. Searches for2692, scroll gesture, NRMouseGestureScroll, gesture iframe, focused iframe and scroll target found2678/2467/2447 as related history, no duplicate focused-frame routing fix. These searches are not a proof that no related work exists anywhere.

Source baseline: latest main `29a3eb23145da90706d8a94441373278521cc737`. Independent source branch [qa/kan97-scroll-target](https://github.com/Floorp-Projects/Floorp/tree/qa/kan97-scroll-target), commit `d6bd2fc457774f55ab1f36a2b01c941f908f1ab0`;3 files,113 insertions/1 deletion. Parent reviewed the diff before Draft creation. KAN-30 source/PR3043 and canonical products were not modified.

## Actual products, input and focus

Debian13.6, owned Xvfb117 plus Openbox, one browser at a time. Normal focus behavior is explicit (`focusmanager.testmode=false`), browser document focus was true. Inputs are OS XTest synthetic right-button gestures and Ctrl+Home/End, not physical hardware. The settings enable control was toggled through actual W3C UI; persisted/default up and down mappings were verified as gecko-scroll-to-top and gecko-scroll-to-bottom. No action callback or renderer was registered to force a pass.

Products: official Floorp12.21.1/Gecko159.0.1 build20261103153232; existing full strict PR3031+3034 combined candidate source0cf1474c (3031dbafd,3034b21a); Firefox159.0.1 build20261102144711 native keyboard control; and the explicitly labeled diagnostic candidate below. Firefox has no Floorp gesture implementation, so its keyboard results are a scroll-target control, not a Firefox gesture pass.

Controlled HTTP fixtures (not data URLs, which the actor does not match) contain a long root document, a focused overflow element, a same-origin iframe and a cross-origin localhost/127.0.0.1 iframe. All start at1800px with clear room in both directions. Real focus, selected top/child browsing-context IDs, labels, target and non-target scroll positions, trusted events and context-menu counts were recorded. No original external page fetch or iframe DOM-access workaround is involved.

| Condition | Official gesture | Existing3031+3034 gesture | Firefox Ctrl+Home/End | Patched diagnostic gesture |
|---|---:|---:|---:|---:|
| Root top/bottom | 2/2 | 2/2 | 2/2 | 2/2 |
| Focused overflow top/bottom | 2/2 | 2/2 | 2/2 | 2/2 |
| Focused same-origin frame top/bottom | 0/2 | 0/2 | 2/2 | 2/2 |
| Focused cross-origin frame top/bottom | 0/2 | 0/2 | 2/2 | 2/2 |

All32 keyboard controls across the four products passed. For the failing frame gestures, the correct Scroll to Top/Bottom label appeared; frame position stayed1800 while the unrelated root moved to0/7083. The frame's own endpoints were0/6650. This identifies a target-routing defect, not a direction/distance activation failure;3031/3034 do not fix it. After the change all16 candidate gesture+keyboard cases pass, and every non-target root/overflow/frame position remains unchanged. All24 gesture context-menu counts are0. Each Floorp run logged exactly8 scroll dispatches for its8 gesture inputs.

## Minimal fix and verification

Scroll actions now use Services.focus.focusedContentBrowsingContext **only when its top is the selected tab's browsing context**. Missing focus, another tab or another window falls back to the selected context. All existing scroll directions share this route. No actor whitelist, direction recognition, distance logic or protection was changed.

Seven colocated browser policy cases cover null/undefined/root focus, a focused frame, nested descendant and foreign root/descendant. The browser before-control is an explicit old-routing adapter (selected context unconditionally), checked against the exact old production expression. It is **not literal old-module execution**:5 cases pass, the2 frame/nested routing assertions fail; the actual new helper passes7/7. No missing-import or compile failure is counted as the before-failure. Actual official and combined OS-input frame failures are the primary before evidence. Limited strict Deno checks for the new helper/test, formatting and diff checks passed. No broad local typecheck was run.

The after product is a **compiled-module diagnostic copy, not a full production build/package**. Native launchers and omni were copied using the established QA recipe; static native resources are reused. Only `chrome/noraneko/nora-content/assets/js/gestures.js` changed, inserting the compiled source helper and the corresponding routing call. All other archive entries match. `candidate-patch.json` contains hashes. Actual GRE resolves to `/workspace/kan97-runtime/candidate`, and the module resolves to that copy's omni (`candidate-runtime-metadata.json`). The real product controller/labels/actor received actual OS gestures; there was no DOM scroll substitute during the tested action.

Official/candidate signatureRequired=false is the existing default (userSet=false), not a testing change; the combined strict candidate and Firefox retain true defaults. Content sandbox level is6. No signing, CSP, sandbox, actor registration or validation was disabled. Canonical omni, executable and libxul hashes were audited unchanged.

## Attempt and evidence limits

An initial bare-Xvfb attempt was stopped and preserved separately before focused comparisons; it is not the accepted focus evidence and is not a product crash. The corrected clean runs use Openbox and normal focus. A later GRE metadata check reused the QA browser-log filename; its log was separated and only the candidate matrix was replayed to retain the raw8-dispatch log. The prior verified summary is preserved as a receipt. No source change accompanied that replay.

Raw per-product JSON, settings/gesture screenshots, source patch, before/after unit results, scripts and hashes are adjacent. Binary copy, profiles and original MP4 remain outside publishable QA under `/workspace/kan97-runtime`; no binary is published. Parent handles Jira, Draft3047/CI and small-evidence publication. Remaining conditions are original Windows11/12.17.0, the original website/profile/focus structure and physical input. Do not mark the whole issue Done or claim the original root-page symptom was fixed.

Publication note: full startup browser logs remain private; dispatch-excerpts.json preserves the exact8 command lines per Floorp run with raw-log SHA256. Source CI42096334025 is tracked by the parent. The replay was evidence recovery, excluded from the56 primary observation count.

Final source CI: Draft3047 run42096334025 succeeded, including smoke/all4 nativeplatform checks,282 browser test modules/0fail and16 downloaded Firefox tasks. scrollTarget.test.ts passed7ms. Artifact ZIP SHA f42b0a5134fb0545ab498ce9b22e12a57980335e3f758c0ae2f9845752d1bef1 matches GitHub digest. See ci-3047/summary.json and verified-excerpts.txt. Jira10412/10414 records local and CI results; original Windows/fullpackage limits remain. No source/CI rerun.
