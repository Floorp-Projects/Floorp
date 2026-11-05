# KAN-98: separate sparse initial-direction fix

Baseline `984509d2053be7bae9027133bda7259cd706b208`. Independent worktree `/workspace/floorp-kan98-sparse`, branch `fix/kan98-sparse-initial-direction`, commit `e6f2dbe7b34e73950ddbedd13378f04fa38c5155`.

## Defect and scope

The initial-direction sampler uses points 10–50px from the trail start. A sparse first move can jump beyond that interval while preserving a genuine right-angle corner in the recorded trail. The fallback currently searches backwards from the final point, misclassifying a right-down trail's initial direction as downRight. When the direction-sequence path is also under-sampled, the shape recognizer's direction-variant filter rejects the otherwise matching L shape.

The minimal production change searches forwards for the first point at least15px from the start. It preserves the existing sampling average, thresholds, shape database and confidence checks. No new direction/shape is guessed when a corner was never recorded.

This is independent of the returning-distance controller fix in Draft #3031, which remains unchanged. A separate Draft is justified: different runtime function, independent reproduction and no dependency on #3031. Neither change establishes all KAN-98 conditions resolved.

## Actual browser source tests

The complete `mouseGestureRecognizer.test.ts` module is bundled from actual source and executed in official Floorp chrome. No HMR/config/action boundaries or recognition functions are replaced in this suite.

- Original recognizer + two new test cases: **2 failures, 61 other cases pass** (`sparse-tests-original-29332.json`). Both fail on the sparse first-leg assertion.
- Revised recognizer: **63 cases pass** (`sparse-tests-patched-29336.json`). Coverage includes both previously captured failed trails, rotated L variants with initial sub-threshold jitter, and a two-point diagonal whose missing corner must not be guessed as an L.
- `deno lint` on both changed files and `git diff --check` pass. Newly added test block is formatted; existing unrelated formatting in the recognizer/test files is preserved. Full suite/production build remain unrun locally.

## Actual product causal comparison

Debian13.6, Xvfb :92, DBus, new isolated profiles. Official Floorp12.21.1 / Gecko159.0.1 / BuildID20261103153232. Dedicated `floorp.mousegesture.enabled=true`; captured config explicitly maps right-down to New Tab. Browser sandbox/security verification stay enabled. W3C input, not physical input.

Two recorded failure paths were sent as explicit zero-duration W3C moves, preventing interpolation from silently filling the gaps. Each condition was repeated three times:

| Input | Official original | Separate copy, fallback-only change |
|---|---:|---:|
| Recorded three-point L | 0/3 actions, blank label | 3/3 actions, New Tab label |
| Recorded five-point L | 0/3 actions, blank label | 3/3 actions, New Tab label |
| Explicit dense10px L control | 3/3 actions | 3/3 actions |

Raw results `sparse-product-29333.json` and `sparse-product-29335.json` include exact requested points, received events, trusted flags, configuration, labels and before/after tab counts. Screenshots use unique `sparse-product-29333-*` / `sparse-product-29335-*` filenames. The source-test comparison and modified-product comparison independently agree. This is a diagnostic copy, not a complete rebuilt release from the commit; the original product is untouched.

The official archive hash was verified earlier against release hashes.txt. `sparse-product-patch.json` identifies the exact changed JS asset hashes in the dedicated copy. The first copy-patch attempt rejected a whitespace mismatch before completing the archive; its launch then failed because the executable symlink was not created. That is a harness failure, not a product regression. The corrected bounded matcher produced the valid copy used for29335.

## Recoverability limit

The recorded paths retain the right-angle corner, so their initial leg is recoverable. A trail containing only the start and diagonal endpoint has lost that corner; the new test requires it to remain unrecognized when only right-down is configured. This change does not reconstruct unobserved motion. Physical input, Arch/Sway/Wayland, other sampling/acceleration/DPI combinations and standard CI remain unverified.

## Reproduction assets

- `../kan98-sparse-product.py`, `../kan98-sparse-product-run.py`: actual input comparison. Run with `floorp` or `floorp-sparse-fixed`, unique port, scenario `sparse`.
- `patch-sparse-product.py`: apply the single-loop change to an isolated copy of the official product. It requires that copy to exist at `../products/floorp-sparse-fixed`.
- `../kan98-sparse-tests.py`, `../kan98-sparse-tests-run.py`: source test execution in real browser chrome.
- `sparse-source-test/bundle.cjs`: bundle test module; `QA_SPARSE_ORIGINAL=1` substitutes only the saved original-recognizer source for the before comparison.
- `sparse-source-test/original-recognizer.ts`: original pinned source snapshot.
- Runtime/Python and product paths are centralized under browser-ui in these scripts and must be adapted on another machine. No profile data or credentials are required or intended for publication.

## Native CI follow-up

Run42048615534 on e6f2dbe7 completed successfully. Downloaded artifact13428075583 matches SHA256 d719bd8a501e433027dd6a83786ff143c94990f0aa4057274688dc16976094d0. Explicit recognizer module success, 281 modules/0 failed and16 Firefox tasks passed were read from its logs. Physical/original-compositor conditions remain open.
