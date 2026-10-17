# Original Mozilla Debug evidence intake

This QA3-08 collector connects Floorp to the existing Runtime xpcshell execution
and exported results. It does not implement another native runner or Mozilla
parser. It does not complete QA3-06 packaged Floorp tests.

The actual execution entry is Runtime's
[`qa3_xpcshell_entry.py`](https://github.com/Floorp-Projects/Floorp-Runtime/blob/7dd619742b25567e479eeda97ae071d10542ad0b/.github/workflows/scripts/qa3_xpcshell_entry.py),
called by its existing cold consumer with an explicit same-build xpcshell
helper, installed test/manifest, sequential execution and retry disabled. The
collector retains the original `commands[].argv`; it does not invent a
replacement command or treat the Runtime's creation-only hosted workflow as a
generic Floorp job.

The supported scope is Linux x64's installed
`xpcom/tests/unit/test_nsIMutableArray.js` and its assertion canary. The
producer and consumer must name the pinned Runtime revision and original
run/attempt 1. The original provider run/job/steps must succeed.
Acquisition/root supervision, the complete export inventory, process receipts,
stdout/stderr and raw mozlog digests must agree. The collector checks the
recorded one-pass argv and replays both complete raw logs with the existing
pinned `qa3_mozlog.verify_mozlog`. Every input stays available under a fresh
output directory, including a private copy of the original ZIP and the pinned
parser source. A later call cannot overwrite the first observation.
Failed/cancelled provider work remains FAIL; missing/skipped/foreign/unsupported
evidence blocks.

Obtain the original consumer artifact and producer receipt through the normal
authorized provider connection. Do not substitute a production Runtime manifest
or another run. `provider.json` contains original REST `run`, original-attempt
`job` (including steps), consumer `artifact`, and the SHA256 of the original
producer receipt as `producerReceiptSha256`. An offline JSON snapshot does not
authenticate itself; the caller must verify the workflow revision and original
provider/artifact provenance. The output keeps
`externalAuthentication:REQUIRED`.

```sh
python3 -B tools/mozilla-debug-qa/collect.py \
  --provider original-provider.json --producer producer.json \
  --archive qa3-consumer-evidence-RUN-1.zip \
  --runtime-git /existing/public-runtime-checkout \
  --product-record native-verification.json --output /tmp/new-debug-observation
```

`--product-record` is optional descriptive product context. Debug and product
compile subjects have separate BuildIDs; their equality is not required. Equal
Runtime HEADs alone do not prove common patches, compiler inputs, diagnostic
differences, profile validity, expiry or revocation. `productApplicability` and
`nativeProductQualification` always remain UNVERIFIED.
`PASS_DEBUG_EVIDENCE_ONLY` means that these two original raw-log records passed
the scoped replay. It does not issue Runtime qualification, native
fixture/support authentication, complete worker isolation or publication rights.
`publicationAuthorized` is always false.

`runtime-recipe.json` pins the actual Runtime Git revision, plan and verifier
dependencies. The collector reads only those local Git objects, with lazy fetch
disabled, and verifies their sizes/SHA256 before importing the parser in an
isolated, supervised host subprocess. It does not import the mutable Runtime
working tree, download dependencies, launch xpcshell/Firefox or access
credentials. Raw ZIP processing reuses the bounded Linux intake reader; no
archive path is extracted. Native execution and provider/host-loss durability
remain separate. No dedicated VM or persistent shared claim is required to
collect these files.

The integration tests execute this CLI and the real pinned parser on synthetic
export files, using owned Python/Git children. They are not native test results:

```sh
QA3_RUNTIME_RECIPE=/existing/public-runtime-checkout \
  python3 -B -m unittest discover -s tools/mozilla-debug-qa -p 'test_*.py' -v
```

The ordinary PR workflow obtains only public pinned Runtime source and runs
these host tests. It never starts a Runtime build, native consumer or paid mode.
Existing product probes, source QA, profile generation, signing and publishing
gates remain separate and unchanged.
