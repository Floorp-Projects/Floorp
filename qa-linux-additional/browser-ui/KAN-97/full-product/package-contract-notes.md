# Exact package and Runtime contract

Source: `d6bd2fc457774f55ab1f36a2b01c941f908f1ab0` (Draft3047). Observed main: `29a3eb23145da90706d8a94441373278521cc737`. Checked all existing head runs before dispatch; no package run existed. Dispatched existing package.yml once, run42103462848/attempt1, Linux-x64, stable-v2-validation, signing skipped. This created Actions artifacts only, not a public release.

The actual downloaded v2 manifest and lock agree on Runtime8965ffc346d426ccdf919bd18585e4d4a0c21654, BuildID20261103153232. Manifest artifact13274392012 and native Runtime artifact13272530069 were passed with their existing IDs/digests; no invented manifest or contract override. Source provenance, strict packaging, installer, DEB and MAR stages succeeded. See runtime-contract.json, runtime-build-manifest-v2.json, source-provenance/runtime-source-provenance.json, dispatch-inputs.json and package-run.json.

Final installer artifact13452071690 ZIP SHA256 `6c4a6d689c9138d8c29a3166274f5f811ec44f4cd486c3be4757ad55a68228d2`; final MAR artifact13451527153 ZIP SHA256 `da6021043540e6cf4ef8c2bdaa90f83799446c040d50407cd3614c1ae9ac06a0`. Both downloaded bytes match GitHub artifact digests. Extracted tar SHA256 `dcad8272354b639220d1e3e0677d2aa140ee3ed02c7b60888cfa4dc26266f7a5`.

The intermediate artifact-test-build-output was not executed as the final product. A fresh extraction of the final installer archive was executed without replacing its modules. Executable SHA256 `4f05c1330b38e50040440be6a3b6e0e7a5b52bc79edb5c5478d978c81f5a603d`, libxul.so `760ff8a689f52c9b1f3c89a45ea8813e59f187990caed747e4a7680020c8dbc7`. Complete before/after manifests record every payload entry, including browser/omni.ja.

## Packaging INI versus live identity

An acquisition assertion initially equated generated INI timestamps with compiled Runtime IDs and stopped before launching. This was a QA assertion error. The root application.ini explicitly says it is unused unless moved under browser/ and invoked with -app. application.ini/platform.ini record packaging timestamp20261105203105; these files and metadata were preserved unchanged. Live Services.appinfo.appBuildID and platformBuildID both report20261103153232, and version159.0.1, matching the actual manifest and MAR metadata. This follows the existing release-artifact verifier's live identity check. No metadata was rewritten to create a pass.

Native sandbox level6, normal window focus and unmodified product signature default (false, no user value) were recorded. No sandbox/CSP/TLS/signature protection was disabled. Synthetic XTest input, controlled HTTP fixture, and private Openbox/Xvfb are distinct from physical input, original Windows11 and HuggingFace conditions.
