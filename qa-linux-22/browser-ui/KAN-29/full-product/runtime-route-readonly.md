# KAN-29 PR2877 exact-head package route (read-only)

Exact head `f43b22d34c0728e0ed800b8016d81b3212190954` locks daily-1142 Runtime `c74fe1a545d275cdd710ab44701c1ee0dd6b0eae`, Gecko **157.0.1**, BuildID **20261007125927**. Small official application.ini ZIP was downloaded and hash verified.

Matching successful Runtime run **37625027712** has v2 manifest artifact **11506774970** and Linux native bundle **11501872421**; API metadata says both unexpired until 2027-01-05. IDs, digests and sizes are in runtime-route-readonly.json. Child blob redirect returned HTTP403 of undetermined cause, not evidence of expiry or absence. Manifest JSON/native bundle content verification is parent-owned. No further child download attempts.

The exact-head workflow supports `stable-v2-validation`, requires caller/source SHA equality, validates manifest/run/artifact identity and source provenance. Runtime upstream pin exists; exact PR source patch hash matches its pin. This establishes metadata/source-contract compatibility, not completed packaging. Three existing exact-head runs contain no package run: source CI37798617165 failed its combined browser suite; its only artifact is test logs.

Official **v12.20.0** provides a strong same-Runtime baseline: hash-verified linux-meta.json identifies the same Runtime run/artifact/digest and BuildID, with full-version verification. Complete Linux tar asset620366462 is available. Release source `a6290b5811b8c5309b399ac340ec40b52f97b73c` is the exact PR parent; GitHub comparison shows only two flex-order implementation/test files changed. No binary download/execution in this audit.

No dispatch, build, push, source, lock, workflow or PR changes were performed. Original-condition qualification remains unchanged.
