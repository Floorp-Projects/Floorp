# Genuine XPT generation and strict packaging

**Both candidates now package successfully with unchanged strict checks.** The earlier missing-XPT failures remain historical logs, superseded by source-generated genuine material rather than a skipped check.

## Inputs and generation

Runtime source8965ffc346d426ccdf919bd18585e4d4a0c21654 plus the existing workflow upstream/common patches. PR3033 candidate e7dda31f uses unchanged artifact ConfigEnvironment (`COMPILE_ENVIRONMENT` unset/false); no option was changed to force compile traversal. BuildReader evaluates3178 contexts and selects **155 modules / 801 IDLs**. `inputs.json` records every selected module/input/SHA256. Parent separately confirms all1250 source IDL files match between isolated runtime trees (`source-idl-comparison.json`).

`generate.py` invokes the existing unmodified `python/mozbuild/mozbuild/action/xpidl-process.py` for every selected module. It generates actual linked JSON typelibs, headers, Rust bindings/macros and TypeScript declarations through the canonical implementation. No dummy module, subset, invented IDL, changed implementation or native C++/Rust compilation. All155 modules succeeded. `outputs.json` hashes every .xpt; `all-generated-hashes.json` records all2868 derivative files. `dependencies.json` hashes903 generator/IDL dependencies, including Bindings.conf and processor/Makefile. Binding-conf and source-generator bytes are preserved.

Combined candidate's own unchanged BuildReader config selects the same155 modules/801 IDLs; all903 source/generator dependency hashes match (`combined-module-comparison.json`). Therefore genuine XPT outputs were copied without regeneration into each isolated object's dist/xpt_artifacts. Both resulting XPT companion ZIPs contain exactly155 matching modules and identical SHA256 e6b1118254c2066921c207741090aa59a165591c5ac564ab239c221e88412889.

## Strict package results

Before each package, toolkit/mozapps/installer/packager.mk was asserted byte-identical to Runtime gitHEAD, including --error-if-empty. No empty-archive relaxation remains. No signing/security preference override was applied.

- PR3033: strict-package.log exit0, 1m43s. Package SHA2561230b658bb907482e2a411e0327062c0926a8b495f5609e180027771cdb6e9d2.
- PR3031+3034 candidate: combined-strict-package.log exit0, 1m53s. Package SHA2569fb23b445994c1ec1716609789b85e7b6ac7d20940ecee360e67b38c626ea1fe.

Exact local paths and companion hashes are in strict-results.json. Native code remains the verified prebuilt Runtime159.0.1/build20261103153232; no new high-cost native build, CI dispatch, upload or release.

## Relationship to completed product regression

PR3033 strict archive content/type matches all56 entries of the already tested optional-route extracted product. Raw archive modes differ from that extracted tree on27 files solely due to umask077 (644/755 become600/700); owner read/execute bits are preserved. Re-extracting strict archive with the same method yields **complete content/type/mode/hash identity across56 entries**, differences0 (`3033-extracted-full-tree-comparison.json`). Its existing X11/Wayland adoption tests without CSS injection therefore apply to the identical product payload; no repeated UI test was needed.

Combined strict package contains all1115 production assets matching the already tested assembled candidate byte-for-byte, and **all26 packaged ELF binaries** match (`combined-strict-product-audit.json`). Existing exact integration regression remains the test evidence; new full combined-suite/native-Wayland/physical-input coverage is not claimed.

## Provenance limits

These are genuine outputs regenerated from the matching patched source and the selected artifact configuration. The original Runtime CI XPT companion is unavailable; byte identity to XPT data embedded when the prebuilt native libxul was compiled is not proven. Full native compilation and official-release/default-byte identity are not established. The selected module list is complete for the actual unchanged artifact configuration, not an invented full-native-build configuration. These limits do not turn the strict package process into an official release.

## Reproduction

After existing product assembly, from the PR3033 Runtime checkout with its build Python environment:

```sh
MOZBUILD_STATE_PATH=/workspace/.floorp-cloud/mozbuild ./mach python --virtualenv build /workspace/qa-linux-22/browser-ui/full-product/xpt-generation/generate.py --generate
```

Verify outputs.json matches all155 generated/xpt/*.xpt; supply the entire set to the object's dist/xpt_artifacts. Keep canonical packager.mk unchanged and run ./mach package. For another candidate, first run compare-combined.py (adapt isolated paths) to assert every selected module/input and source dependency hash is identical, then supply the same full generated set. strict-results.json documents verification of every ZIP member. compare-tree.py/compare-directories.py/audit-combined-package.py preserve payload comparisons.

Publish small manifests/scripts/logs/report only. Exclude generated/ derivative bodies, 3033-strict-extracted/, object/runtime/source trees, binary archives, profiles and native product logs. Local candidate archives are not uploaded downloadable artifacts. No Jira status/Done/merge changes.
