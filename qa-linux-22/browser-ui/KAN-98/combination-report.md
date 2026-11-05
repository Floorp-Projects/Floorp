# PR3031 + PR3034: unbalanced returning trail

## Current result and application order

For minDistance=20 and the exact relative trail `[(0,0),(0,-160),(0,16)]`, with both up-down and down-up configured, **apply PR3034 before PR3031, or apply them together**. The real-product comparison reproduces the wrong action when only PR3031 is applied. Neither PR was merged.

Official Floorp12.21.1 / Gecko159.0.1 / BuildID20261103153232, Debian13.6, Xvfb/X11, private DBus, fresh profiles. The copies change only the matching compiled functions: returning-distance gate for3031, and initial-direction fallback for3034. The combined copy is a diagnostic product copy, not a full production build of both commits. Browser security/TLS validation were not disabled. Input is synthetic, not physical hardware.

Actual mappings: up-down -> New Tab; down-up -> Close Tab. The separate `floorp.mousegesture.enabled` preference is true. The legacy nested config.enabled remains false and is not the enabled preference. The product service sets native `ui.context_menus.after_mouseup=true`, unlocked; the probe only reads it.

| Product/input | Actual result | Native menu |
|---|---|---|
| Original / W3C | No action, original tab retained (end displacement16 <20) | Opens on press |
| 3031 only / W3C | Wrong Close Tab once | Opens before trail |
| 3031+3034 / W3C | Correct New Tab once, no Close Tab | Opens before trail |
| 3031 only / OS XTest | Wrong Close Tab once | Closed, event prevented |
| 3031+3034 / OS XTest | Correct New Tab once; TabOpen=1, TabClose=0; original retained | popupShown=0, state=closed, release contextmenu prevented |

The W3C trials emit contextmenu on mousedown before any movement and fail the no-menu criterion. Those failures are preserved rather than reported as passes. OS XTest exercises the Linux native input route with measured release-time policy and suppresses its menu. The difference is input-path dependent; physical input/compositor behavior remains unverified.

The final fail-fast regression uses OS XTest (29510), not the W3C path. The received held points exactly preserve the requested offsets: `(450,500) -> (450,340) -> (450,516)`. It confirms correct action once and no contextmenu without manually hiding a popup or forcing the timing preference.

## Added source regression

Test-only commit `b21a81120d2354d90ea433410173cac208aabbf8` on PR3034 adds one case to the actual recognizer test module. It configures both opposite patterns, uses the exact three relative points and threshold20, and requires up-down. The focused case was executed inside actual Floorp chrome:

- Baseline recognizer: expected failure, actual down-up (`combination-source-baseline-29507.json`).
- Fixed recognizer: pass (`combination-source-patched-29506.json`).

No existing test suite or old CI run was manually rerun. Lint and diff checks pass; the added function's formatting passes. Whole-file formatting has pre-existing differences outside the added case. A direct typecheck first used the wrong project context, then the correct project command stopped because this independent worktree has no node_modules/fp-ts. Neither typecheck attempt is claimed as a pass. The real-browser focused test and product regression are the validation for this addition; the earlier full CI results apply to the earlier stated commits, not automatically to b21a8112.

## Evidence and reproduce

- `combination-29503.json`: baseline W3C, refined16px case.
- `combination-29504.json`, `combination-29505.json`:3031-only/combined W3C, including pre-trail menu observation.
- `combination-29508.json`, `combination-29509.json`:3031-only/combined XTest.
- `combination-29510.json`: final combined regression, enabled/timing policy readback.
- `combined-product-patch.json` plus `product-patch.json`: exact asset hash transitions.
- `combination-source/build.cjs`: focused export/bundle of the actual source test; no recognition boundary replacement.
- `../kan98-combination.py`, `../kan98-combination-run.py`: product test. The runner propagates child failure.

```sh
# Create copies directly from unchanged products/floorp in fresh destinations.
python3 KAN-98/patch-combined-product.py --variant activation-only
python3 KAN-98/patch-combined-product.py --variant combined
QA_COMBO_XTEST=1 QA_COMBO_EXPECT_PASS=1 PYTHONPATH=python \
  python3 kan98-combination-run.py floorp-combined-fixed 29510 combination
FLOORP_SOURCE=/path/to/PR3034 ESBUILD_MODULE=/path/to/esbuild/lib/main.js \
  node KAN-98/combination-source/build.cjs KAN-98/combination-source/patched.js
PYTHONPATH=python python3 kan98-combination-source-run.py floorp-combined-fixed 29506 source
```

The standalone patch recipe starts from unchanged official products/floorp and refuses to overwrite a destination. Its generated asset bytes were compared in memory with both actually tested copies and match exactly (combination-recipe-verification.json). Use fresh destinations before repeating the patch step. The earlier80px candidate was executed before the refined condition arrived; its29501/29502 evidence is retained separately and is not substituted for the16px result.
