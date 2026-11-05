# KAN77 / GH2263 — Linux complete-product acceptance of Draft PR3049

2026-11-05 UTC / 2026-11-06 JST. Source owned by coordinator; independent worktree qa/kan77-linux-acceptance exact f04ff611c3634515064776e2d58e76648f87269b, base29a3eb23145da90706d8a94441373278521cc737. No product-source changes or duplicate PR. Original issue, ten GitHub comments, Jira10057/10163/10183/10440 were read before work. Later coordinator shared Windows complete-package success/Jira10451, not rerun here.

## Immutable complete product

Existing genuine Runtime schema2 manifest ZIP digest2b6a1b20190dc7016957ca25f72f8e83f267de51458b515153fd209bb90d9138, manifest artifact13274392012, Runtime success run41664957250/attempt1/head8965ffc346d426ccdf919bd18585e4d4a0c21654. Linux-x86_64 native artifact13272530069, API/manifest digestsha256:ed5a4f97ee7bf42c817f3146d5a843e095710dcc3be25fae0af0bf47274b739a, expected nativeBuildID20261103153232. Runtime source-provenance ZIP13453773323 digest2b69a1e0031844358499b0dded97e4f5c64143dff795e58b3be01baee931191f also checked. This is the existing compiled Runtime, not a native rebuild claim.

Checked exact-head runs before dispatch: only existing Windows package42111951173, no Linux package. Single new Linux package-only stable-v2-validation run42112677415 succeeded, platformLinux-x64, skip_signing=true, signed_validation=false; no release/publication/merge. Existing standard CI42111775032 succeeded; no rerun. Windows package not duplicated.

Installer artifact13455518201,108210887bytes, API/actual ZIP SHA256 cbc7b3e62a4704e8ed6463b0c2e3ae774d687f74958ae9b0dd496e585bd97e3f. Inner official workflow-generated tar SHA256 e7ff715c12ba56550fea37d0fdbd3c515d3cb3a69fccb62bc79bba16ad058a9b. Safe extraction; actor top/current-document guard and actual split-bundle progress/observer guards inspected. Shipped index19.js.map sourcesContent for SsbPageAction.tsx and ssbManager.ts exactly match immutable PRhead. The complete extracted product was run without JS/source replacement, mock service or diagnostic injection. Local inspected artifact copy is outside product prefix.

Official12.21.1 baseline source984509d2053be7bae9027133bda7259cd706b208; these three target PWA source files are identical at official source and PRbase (recorded hashes). Whole products are different builds, not claimed identical except patch; coordinator's same-source Windows comparison provides separate causal evidence. Both native products report Gecko159.0.1/build20261103153232 and Floorp display12.21.1@159.0.1.

## Natural HTTP and actual UI acceptance

Actual loopback HTTP fixture serves HTML/manifest/SVG with Cache-Control:no-store. Dedicated fresh profiles; PWA remains enabled. addTab/loadURI/reload/OpenBrowserWindow and real DOM-created iframes exercise native events. No observer notification is fabricated and no product handler replaced. Three-second action settle windows, raw request-log indices/time/path/counts and chrome UI states permit recomputation. Requests are low-volume to owned fixture; Archidekt/Allegro were not load-tested. This does not establish causality for original429.

| Natural condition / manifest counted | Official baseline | Complete candidate |
|---|---:|---:|
| Initial selected A | 6 | 6 |
| background-B (/manifest/A.json) | 6 | 0 |
| subframes-four (/manifest/A.json) | 18 | 0 |
| other-window-C (/manifest/A.json) | 4 | 0 |
| two-window-background (/manifest/A.json + /manifest/C.json) | 9 | 0 |
| select-B (/manifest/B.json) | 2 | 2 |
| reload-B (/manifest/B.json) | 6 | 6 |

All four unrelated-owner conditions improve to0; selected fetches retained. Both products hide action on all3 negative controls and restore it on A. Candidate panel title KAN77 A / origin127.0.0.1 / native label Install Application. Candidate16 phase measurements plus baseline13 and independent baseline-ui/candidate-ui controls; all4 product sessions normalQuit/exit0. Complete candidate46 file/symlink entries unchanged before/after; baseline46 match original prefix after tests. This is full product acceptance of scoped behavior, not original-site bug closure.

Initial A and selected B/reload are necessary fetches, not required to be exactlyone. Same-document6-fetch duplication remains outside PRscope. Negative controls top-without-manifest, top-with-child-only-manifest, and about:blank must remove the action; return to eligibleA restores it. PWA panel is opened with a real WebDriver click and shows fixtureA name/icon, localhost origin and Install Application control. No app installation/desktop-entry/launcher claim from this UI check. Baseline panel has its own fresh UI-control session; candidate UI check is after the main natural sequence, plus a matched fresh single-window candidate-ui control. The multiwindow screenshot has windowC foreground behind the windowA-owned popup because synthetic background-window clicking does not ensure window activation; the single-window screenshot removes this harness presentation ambiguity.

## Limits and harness failures

Debian13(trixie)/kernel6.18.57 x86_64, dedicated Xvfb/X11/Openbox/private DBus, no /dev/dri,/dev/nvidia0,/dev/dxg. Virtual GUI with synthetic browser control/X11 input; physical GUI/input and original CachyOS not tested. Proxy/CA retained, localhost excepted normally; no TLS bypass or security-pref decrease. Both stock content sandbox6, sandbox user overridefalse; signatures.required stockfalse/user overridefalse, unchanged. No third-party extensions installed and no personal profiles/accounts/cookies exported.

First baseline controller stored references on nonpersistent marionette globalThis; stopped before Cnavigation with harness assertion. Native windows/tabs identified, correct persistent window references used and execution resumed; initial/background/fourframe not repeated. Corrected script used for candidate. Package inspector initially assumed PWA in core.js then too-small snippet context; corrected to actual splitbundle and verified exact source maps **before** candidate launch. These are QA setup errors, not product defects or failed packageCI. Unused partial extraction copies stayed isolated.

Remaining original-site429 causality/networkinitiators, original CachyOS/browser version/user conditions, physical GUI validation, same-document duplicates and sameURL-document-generation/out-of-order races are unqualified. No proof of general request-loop/429 resolution. Linux acceptance does not expand PR3049 scope. Original Linux22/C-five denominator and KAN50/KAN86 evidence unchanged; KAN102 product work still on hold. No merge/release/Done.

Console boundary: both baseline and candidate natural suites contain two SsbPageAction AbortError lines (ManifestMessages actor destroyed before pending manifest query resolved). These are retained in raw pwa-error-lines.json, not suppressed or counted as a clean-console claim. All observed settled UI states and normal exits remained correct; no new candidate-specific regression identified. Exact failing document/teardown timing was not traced, and document-generation races remain outside acceptance.
