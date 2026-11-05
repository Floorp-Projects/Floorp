> Current result: strict packaging subsequently succeeded with all155 genuine Python-generated XPT modules. See ../xpt-generation/report.md, strict-results.json and strict-package.log. The previous missing-XPT/optional-route paragraphs below preserve the earlier phase and are superseded for current package status. Current standard-path strict archive SHA2561230b658bb907482e2a411e0327062c0926a8b495f5609e180027771cdb6e9d2. The historical c10f... optional archive is retained at the sibling `.optional-aux.tar.xz` path, not the current standard path.

# PR3033 full production assembly and limited regression

PR head `e7dda31f660bb865c2ea8a505ece44875796970f` confirmed. Independent main checkout `/workspace/product3033/noraneko`, matching Runtime checkout `/workspace/product3033/runtime` at `8965ffc346d426ccdf919bd18585e4d4a0c21654`. Shared branches/checkouts unchanged. No release, upload, CI dispatch, merge or Done transition.

## Completed stages

1. Private dependency install; existing `feles-build build --phase before-mach` built all production chrome/startup/modules/pages. Not a single CSS replacement.
2. Runtime upstream/common/packaging patches from existing workflow, version generation (12.21.1@159.0.1), matching source configure, existing `MOZ_ARTIFACT_FILE ./mach build` succeeded. Native code was taken from the exact prebuilt artifact, not compiled locally. Native artifact SHA256 `cb9b01283d5ce3167f4ba046ee7b39ae9d98a692e1beba73bd9f85f460157772`, version159.0.1/build20261103153232, verified against the published asset digest and application metadata. This matching source/artifact supersedes the earlier environment snapshot's 157 baseline.
3. Existing after-mach production XHTML injection and built-file patches completed. Native libraries remain prebuilt. The repository preference override step is omitted because it disables addon signatures. Both signature-required preferences are read as true/default true/no user value in the actual candidate. Official release/default byte identity is not claimed.
4. **Strict package failed**: missing matching XPT auxiliary input (`No files added ...xpt_artifacts.zip`). `package.log` preserves failure. Matching runtime CI/release assets inspected by parent also contain no XPT companion. No fabricated XPT or ignored integrity mismatch supplied.
5. A **separate existing-workflow optional-aux package** completed with the workflow's empty auxiliary archive handling; `package-final.log`. This is not strict package success. Strict packager checks were restored immediately afterward. Archive SHA256 `c10f8375652dd200e4136c6bb1ebb0f444a6f4d552c97a8a57aa2c6d0571f5c8`.

## Product tests without CSS fixtures

- Assembled full-production dist/bin on X11: `candidate-29831.json` passed.
- Separate optional-aux package extracted and run with native Wayland client on dedicated nested KWin: `candidate-29832.json` passed.
- Both use the product's own built CSS; no style/source CSS injection or replacement in the test. `cssFixture=false`. Two native split groups, gBrowser replaceGroupWithWindow/adoptTabGroup round trip. Both moved documents retain titles in source, new window and returned window. API fixture is not menu/physical drag.
- Actual noncurrent panes compute absolute/hidden. Both addon/langpack signature preferences true/default true/user false. Browser sandbox protections not disabled.
- Packaged omni.ja `chrome/noraneko/nora-content/assets/js/layout.js` SHA256 `5fb2b654485eaf50fa42b05c81903f278cbb8dba573efc3a2ec9dd97df994296` matches the full production build output exactly. Archive/asset hashes in package-hashes.json.

## Limits and evidence

The initial missing-XPT strict-package failure was resolved by the canonical source-generation follow-up below. Full native compilation, official release/default byte identity, physical input and original Fedora full desktop are not established. The initial failure remains historical evidence; current strict-package success is independently recorded below.

Small evidence: build-progress.json, package-hashes.json, pr-head.json, before-mach.log, upstream-packaging.log, configure-final.log, build-final.log, after-mach.log, built-patches.log, package.log (strict failure), package-final.log (optional route), candidate-29831.json, candidate-29832.json and screenshots. `reproduce.sh` follows the canonical local path and retains strict checks, stopping at the same missing-XPT failure. `run-candidate.py`/`candidate-probe.py`/`nested-candidate.sh` repeat limited product tests. Large native input, extracted package, node_modules, runtime profiles and product logs should not be committed.

Local optional-route archive: `/workspace/product3033/obj-artifact-build-output/dist/floorp-12.21.1@159.0.1.en-US.linux-x86_64.tar.xz`. This local file alone is not an independently uploaded/downloadable artifact. Parent owns QA publication and Jira/matrix updates.
