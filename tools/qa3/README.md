# QA3 source shadow foundation

This is the first QA3-04/11 host foundation, not product qualification. The
approved initial mode is **shadow**. Release and Alpha each call the reusable
source job once, alongside their existing jobs. Its job-level
`continue-on-error` preserves the current required jobs, signing, approval,
bundle schema and publisher. The raw receipt retains FAIL/INCOMPLETE even when
the calling workflow continues. Do not make packages depend on this job yet.

## Fixed plan and execution

`source-plan.json` pins 106 existing cases: 60 Deno runner/utils/smoke host
cases, 28 Node final-package intake cases, and 18 Python source/packaging
provenance cases. These are Floorp host tool tests; they are not Mozilla native
tests or the unavailable implementation kit's 114 tests. The executor uses a
fixed command registry and never calls `deno task test`, `test:integration`,
browser startup, signing or publication. Deno has no run/network/environment
permission. It uses `--node-modules-dir=none` so a cold host does not initialize
unrelated workspace npm binaries under the test file-size budget.

The source runner validates the exact file/case IDs, origin, execution kind,
target, group order and fixed counts. It rejects missing/duplicate/unexpected
cases, skip, failure, recovered results, report/exit-code contradictions and
empty plans. A reduced suite requires an explicit policy/version review, not an
ignore/filter input. The selected Deno tests must use plain callback
definitions; per-test retry/ignore options are outside this plan. There are no
automatic retries. Test internals and trusted policy changes still require
normal code review; a candidate-controlled receipt is not an attestation.

Node 22 JUnit uses generic `classname="test"` without a file field. Its origin
is bound to the one fixed Node input file and exact named cases; explicit
foreign file fields are rejected. Deno report origins remain explicit. A bounded
failure summary is printed to CI logs as well as retained in artifacts.

On a disposable Linux x86_64 host with Git, Python 3.12, Node 22, Deno 2.9.7 and
`/usr/bin/prlimit` (provided by Ubuntu util-linux):

```sh
python3 -B -m unittest discover -s tools/qa3 -p 'test_*.py' -v
python3 -B tools/qa3/source.py run /tmp/qa3-new-attempt --expected-head FULL_GIT_SHA
```

The output directory must be new. Logs/reports are never overwritten to hide a
first failure. CI records actual host/tool versions, source HEAD, run/attempt,
plan/runner/lock/test-file digests, planned/executed IDs and each raw verdict.
`--local` explicitly records dirty/local execution during development; it does
not issue qualification. Preflight rejection also writes a FAIL receipt. CI
preserves contract-test logs and exit codes alongside the source receipt in
`source/`. Workflow artifacts are named with run and attempt and retained for 14
days. This initial receipt does not establish complete history across runs; that
requires the QA3-08 trusted controller before required promotion.

Child processes receive a small environment without credentials or runner option
injection. Each group has a 180-second timeout and a 16-MiB limit for regular
file writes (including logs) applied before exec by prlimit, runs in an owned
process group, and is reaped/terminated on exit. This is a Linux source host,
not the future cross-platform GUI/Agent supervisor.

## Coverage and remaining gates

`coverage.json` keeps installer, MAR, DEB/PPA, Portable and stub proof plans and
all five native targets visible. Their current verdict is **UNVERIFIED**.
Installer unit/intake checks cannot substitute for MAR application, DEB
installation/PPA bytes, Portable transformation/launcher or stub payload QA.
Source PASS leaves product/native/Runtime qualification unverified and always
sets `publicationAuthorized: false`.

The Runtime companion introduces explicit canonical Debug tests and verifies the
common source/profdata contracts. The current source relation and official
Firefox Git/Hg match remain unverified; the latter is advisory. Native Debug
test-support/harness, final signed artifact QA, real memory safety/GUI coverage,
trusted first-FAIL history, revocation/expiry and qualification are later
stages. The implementation kit remains unreviewed because both Library inputs
returned HTTP 403 during this initial work. Equal declared ZIP sizes do not
prove equal contents.

Before any required promotion, run the fixed plan on a clean CI checkout and
measure missing coverage, initial failure/flake rate and resource use. Then
prove a Linux x64 Debug configure/support/harness with known-bad canaries and
repeat product evidence on Windows x64, Linux x64/ARM, Mac Intel/ARM. Keep Mac
universal bytes bound to both native executions. Use separate real MAR, DEB,
Portable and stub evidence. No such native execution is claimed by this change.

The future publisher must perform
`bundle -> final QA -> manual approval ->
fresh evidence revalidation -> publish`,
authenticate exact digests and retain the existing gates. The current manual
publication/approval flow is unchanged.
