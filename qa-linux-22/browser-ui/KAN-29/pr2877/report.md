# KAN-29 / existing Draft PR #2877: current Runtime validation

## Result

**Candidate: flexOrder9/9 and panelSidebarLayout8/8 passed. Baseline: flexOrder7/9 passed, with the exact two expected regression failures.** No test assertion was weakened or changed. Existing PR #2877 remains the only PR for this patch; its head was not modified and no new PR was created.

- Existing PR head fetched and verified: `f43b22d34c0728e0ed800b8016d81b3212190954`.
- Separate detached head checkout: `/workspace/floorp-pr2877-head`.
- Separate current baseline checkout: `/workspace/floorp-pr2877-current`, HEAD `984509d2053be7bae9027133bda7259cd706b208` with the PR's two-file patch applied locally.
- `git apply --check` and `git diff --check` pass. Both modified source files are byte-identical to the specified PR head. `panelSidebarLayout.test.ts` is also byte-identical between the PR head and current baseline (Git blob `7ec94801f5b58a9f865c6c55181ff5f61a9b94ab`; SHA256 `9cdfed700d4a05b4ec6a5c5160a0eea08d7386248d7514d49309f8bd7b11bb11`). See `source-identities.json`.

## Current-runtime browser evidence

Actual official Floorp12.21.1 / Gecko159.0.1 / BuildID20261103153232 on Debian13.6. The runner uses a private DBus and nested KWin socket per test port, through the established Xvfb92 display path. These are rendered browser tests against live native DOM and browser APIs, not Deno/Node mock tests.

| Run | Product logic | Suite | Result |
|---|---|---|---|
|29687|PR candidate|unchanged PR flexOrder tests|9/9 pass|
|29687|PR candidate|unchanged current panelSidebarLayout tests|8/8 pass|
|29688|baseline|same PR flexOrder tests|7/9 pass, 2 fail|

Baseline failures:

1. `boxless panels do not move the right hover anchor`: expected offset `0px`, actual `1366px`.
2. `hover anchor follows logical placement in LTR and RTL`: unstable anchor at `rtl`, Floorp end=false, Firefox end=false, `sidebar-launcher-expanded`.

The candidate's panelSidebarLayout module exercises the real embedded panel viewport, persisted resize, floating behavior, feature toggling, open/closed panel hover anchors, ResizeObserver/animation behavior, global-width sentinel, and independent docked/floating widths. Its8/8 result establishes compatibility of those tested paths with the current Runtime. Both `SidebarController.setPosition` and `toggleVerticalTabs` exist in that Runtime. No full repository suite or complete production build was run here.

The nine regression tests intentionally use root `dir` and native launcher attribute/CSS fixtures for directional/boxless/resize cases. They do not prove that a signed RTL locale is active. The parent's separate signed Arabic `RTL_UI=true` UI matrix supplies that distinct evidence; do not describe these fixtures as native Arabic UI operation or physical mouse input.

## Runtime bindings and protection boundaries

The test ESMs import the **actual product module singletons**, actual Solid runtime and actual panel data signals. Separate test copies add diagnostic exports for `gFlexOrder`, `PanelNavigator`, and the existing `setIsPanelSidebarEnabled` setter. They do not replace the functions, renderers, signals or native browser APIs. The unchanged test source is bundled with the normal assertion harness and imported through external `chrome://noraneko/content/qa-pr2877-{flex,panel}.js` modules.

CSP and browser protections remain enabled. Early inline/eval launchers did not work; eval was explicitly rejected by CSP. The final external-module loader uses the normal allowed chrome resource path, without changing CSP, signature or sandbox preferences. The source-test ESM/export additions exist only in the separate `*-tests` copies, not in the UI candidate used by the parent.

## Pure UI candidate copy and exact patch scope

`/workspace/qa-linux-22/browser-ui/products/floorp-pr2877` contains an untouched official executable and a locally patched archive for diagnostic UI comparison. The only changed archive entry is `chrome/noraneko/nora-content/assets/js/index10.js`, only within `updateHoverOffset`:

- derive physical right/left from logical end placement and the computed inline direction;
- ignore empty-width/height rectangles;
- measure occupied width from the resulting physical edge, preserving existing logical start/end output names.

This is the implementation in the specified PR, manually reflected into the already compiled asset; it is **not** a full production build of the PR. No other archive entry's contents changed. `patch-product.py` asserts every bounded replacement and verifies the single-entry difference. `pr-head.patch` is the exact original two-file source patch. `product-flex-module.js` and `candidate-flex-module.js` preserve the relevant compiled before/after assets.

| Artifact | SHA256 |
|---|---|
|Official executable, also used unchanged in candidate|`4f05c1330b38e50040440be6a3b6e0e7a5b52bc79edb5c5478d978c81f5a603d`|
|Original omni.ja|`159285f967e950e0cd1ac658d713b7770e7763d6d983fc88b6e48c16e4a1f345`|
|Pure UI candidate omni.ja|`bfdda188a926178da167a5edfa054f0b67505cfc81688573a4e407cc95abc27d`|
|Original index10.js|`24657cf8dbe5321bb88e7753eeb8ae3238f7028ebad81b9995a8ca7354aa5ae7`|
|Candidate index10.js|`c57f8d99002518d5502f549f19b62837879f48405b8dbc4dd4ac3b233617662b`|

`ui-candidate-hash-recheck.json` and `summary.json` verify that the pure UI candidate still has exactly the recorded archive and executable hashes after the test-copy work. The test-copy hashes are separately named in `test-copy-hashes.json` / `test-module-hashes.json`; do not confuse them with the UI candidate.

## Reproduction

1. Fetch and inspect the specified PR head; create a clean baseline984509d worktree; apply `pr-head.patch` with `git apply --check` first.
2. Start from the verified official Linux12.21.1 archive. Run `patch-product.py` to make the logic-only UI copy. It refuses to overwrite an existing target.
3. `prepare-test-copies.py` creates separate baseline/candidate copies with diagnostic exports only. `tests/bundle.cjs` bundles each unchanged test module, mapping imports to the real product modules. `prepare-test-modules.py` adds the two external test modules and auto-run result handling.
4. Run `dbus-run-session -- bash nested-tests.sh 29687 floorp-pr2877-candidate-tests flex panel` and `dbus-run-session -- bash nested-tests.sh 29688 floorp-pr2877-baseline-tests flex` with unused ports. The baseline command exits1 because its two regressions fail. Adapt absolute dependency/product/worktree paths on another host.

## Failed setup attempts, excluded from regression counts

- 29681: inline-module launcher produced no completion, eventually timing out. Its escaping was corrected, but inline execution remained unsuitable under the browser's policy.
- 29683/29684: private-display setup pointed to a nonexistent Xvfb path, leading to a compositor connection failure before a test ran.
- 29685: another inline launcher attempt did not provide a valid completed test result.
- 29686: explicit `EvalError: call to eval() blocked by CSP`; the policy was preserved and the launcher was replaced by the external-module approach.
- 29687/29688: final valid results as tabulated above.

These setup failures are not product regressions or failed test assertions. The raw logs are retained under `runtime/raw-logs` and excluded from publication along with fresh browser profiles. Published browser logs are bounded diagnostic excerpts, with original sizes/hashes in `log-curation.json`.

Original CachyOS/LTR report conditions are not automatically established as fixed by these regression fixtures. Keep #2877 Draft and KAN-29 open pending the remaining original-environment assessment; no merge/release/Done action was performed.
