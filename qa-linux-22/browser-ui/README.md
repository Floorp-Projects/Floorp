# Linux 22 / browser-ui assignment B

10 assigned issues were exercised in real Linux browser processes. The matrix records limited observations rather than declaring all original distribution/input conditions satisfied. KAN-54 remains outside the sprint. No issue was moved to Done and no merge/release was performed.

Baseline: Floorp-Projects/Floorp `984509d2053be7bae9027133bda7259cd706b208`. Original checkout was not edited. Official Floorp 12.21.1 / Gecko159.0.1, BuildID20261103153232; Firefox159.0.1 BuildID20261102144711. Product archive identity does not independently prove a byte-for-byte build from the source SHA.

## Reproduce

Use an isolated writable directory (the runner scripts resolve relative to their own directory). The recorded Cloud path is `/workspace/qa-linux-22/browser-ui`. Download/extract these unchanged official products to `products/floorp` and `products/firefox`:

- https://github.com/Floorp-Projects/Floorp/releases/download/v12.21.1/floorp-linux-x86_64.tar.xz
- https://github.com/Floorp-Projects/Floorp/releases/download/v12.21.1/hashes.txt
- https://archive.mozilla.org/pub/firefox/releases/159.0.1/linux-x86_64/en-US/firefox-159.0.1.tar.xz

Verify Floorp SHA256 against hashes.txt. Install `marionette_driver==3.7.1` into a local `python` directory with `python3 -m pip install --target python marionette_driver==3.7.1`. Browser GUI dependencies, DBus, Xvfb and optionally KWin/Flatpak must be available. The Cloud has a user-local sysroot; other Linux hosts may use their package manager. Do not source the old `.floorp-cloud/env.sh`, which disables a browser sandbox. Do not disable browser protection or TLS/signature validation.

Start Xvfb on `:92`, 1440x1100x24. Run from this directory, using an unused Marionette port:

```
PYTHONPATH=python python3 run.py floorp 29284 middle wheel
PYTHONPATH=python python3 run.py floorp 29285 search split detach desktop-drag
PYTHONPATH=python python3 run.py firefox 29286 search split
PYTHONPATH=python python3 gesture-run.py floorp 29311 gesture
```

`nested.sh`, `nested-firefox.sh`, and `nested-floorp-final.sh` show the exact KWin setup. Adjust sysroot/executable paths and ports for the destination. `MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1` enables the documented automation interface; content sandbox and validation remain enabled. OS XTest scripts need `xdotool`; they generate synthetic OS input and do not establish physical-device behavior. `run.py` terminates its entire owned process group. Early logs predate that cleanup correction; process-lifecycle failures were treated as infrastructure/harness errors.

KAN-3 and KAN-98 have separate source/diagnostic runners and reports. The controller test bundle documents its HMR/action-registration boundary replacements. These focused browser tests are not the complete repository CI or a production build.

## Evidence caveats

- `search-29284.json`: initial Shadow DOM input harness failure. Corrected final comparisons are 29291/29292. Invalid route attempts are retained under KAN-31; final `#connectionSecurity` is the correct pane.
- KAN-39 normal tabbar did receive the same XTest wheel events but remained at the end, including the reverse-direction follow-up. No reliable normal/stack speed equality conclusion was drawn.
- KAN-29 first layout attempt targeted a nonexistent `sidebar-main`; final 29289 targets `sidebar-container`. Only final samples support hover conclusions.
- `controls-29288.json` did not show the menu correctly. Final 29289 uses the native toolbar visibility helper and verifies rendered bounds.
- KAN-98 fixed-name screenshots were overwritten by the final diagnostic-copy run. Baseline JSON preserves the before result; current screenshots are after that local diagnostic patch.
- Flatpak runtimes were acquired with verification enabled, but the Floorp app pull failed during parental-control details lookup because system DBus is unavailable. No Flatpak product run was claimed.

See `matrix.json`, each `KAN-*/report.md`, and `environment/summary.md`. The dedicated QA branch contains only curated scripts, small evidence and screenshots, without browser profiles, user data, credentials, binaries, dependency caches or raw remote metadata.

Follow-up: actual signed Arabic localization (RTL_UI=true) is recorded separately in KAN-29/localized-report.md and localized-comparison.json. Independent existingPR2877 candidate improves the measured RTL defect; original LTR/CachyOS symptom remains unproven. Existing currentRuntime tests: candidate flex9/9 + panel8/8, original flex2fail/9. KAN-29/pr2877-root-note.md is prepared for the PR owner; PRhead is unchanged. KAN87 also has one normally installed signed image-theme control under qaLinuxKAN87/signed/.

Latest follow-up: PR2877 evidence was posted as top-level comment6663548952 after head/duplicate checks. Full production-asset candidates for3033and3031+3034were assembled using the existing verified Runtime artifact build route and passed limited regressions. Strict distributable packaging fails on genuine matching XPT auxiliary input; this failure remains distinct from product runtime success and3033's optionalaux archive. Candidate binaries stay local. See full-product/build-route.md.
