# QA3 common source QA

Release and Alpha each call `qa3-source-required.yml` once. All four stable
package jobs and all three legacy Alpha package jobs require its success.
The job has no `continue-on-error`. Failed, cancelled, skipped, absent or
unsuccessfully uploaded evidence prevents dependent packages from starting.
The old shadow workflow remains available for explicitly requested diagnostics;
it no longer duplicates the required PR run. Signing, approval, the closed v2
bundle and publisher remain separate existing gates.

The fixed plan and coverage catalog retain their historical `stage: shadow`
schema label; this does not promote a shadow execution. Only `--required`
produces a `required-source` receipt, and that mode rejects local execution,
dirty checkout, a foreign repository and any attempt other than 1. The receipt
validator checks the same run/source, all 106 case identities, no failures or
skips, Python 3.12, Node 22, Deno 2.9.6 and the complete plan, runner,
configuration, lock and test-file hashes. Required outputs include the receipt
SHA-256 and immutable evidence artifact ID/digest. A receipt by itself is not a
provider attestation; consumers must also authenticate its original workflow
run and artifact. These host checks do not issue product qualification.

## Fixed plan and execution

`source-plan.json` pins 106 existing cases: 60 Deno runner/utils/smoke host
cases, 28 Node final-package intake cases, and 18 Python source/packaging
provenance cases. These are Floorp host tool tests, separate from Mozilla native
tests and the implementation kit's 114 tests. The executor uses a fixed command
registry and never calls `deno task test`, `test:integration`, browser startup,
signing or publication. Deno has no run/network/environment permission. It uses
`--no-config`, `--no-npm`, `--node-modules-dir=none` and a fixed
`DENO_NO_PACKAGE_JSON=1` environment with a host-only import map and frozen JSR
lock. Preflight requires that the four standard-library aliases and all JSR
specifiers/integrity records exactly match the root configuration/lock; those
root files are unchanged. The separate inputs avoid initializing unrelated
workspace npm binaries under the test file-size budget.

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
failure summary is printed to CI logs as well as retained in artifacts. JUnit
inputs must be UTF-8 (an optional UTF-8 BOM is accepted). DTD/entity
declarations, NUL bytes and declarations of a different encoding are rejected
before XML parsing, including UTF-16/32 encodings of a DTD.

On a disposable Linux x86_64 host with Git, Python 3.12, Node 22, Deno 2.9.6 and
`/usr/bin/prlimit` (provided by Ubuntu util-linux):

```sh
python3 -B -m unittest discover -s tools/qa3 -p 'test_*.py' -v
python3 -B tools/qa3/source.py run /tmp/qa3-new-attempt --expected-head FULL_GIT_SHA
```

The output directory must be new. Logs/reports are never overwritten to hide a
first failure. CI records actual host/tool versions, source HEAD, run/attempt,
plan/runner/root-config/root-lock/host-map/host-lock/test-file digests,
planned/executed IDs and each raw verdict. `--local` explicitly records
dirty/local execution during development; it does not issue qualification.
Preflight rejection also writes a FAIL receipt. CI preserves contract-test logs
and exit codes alongside the source receipt in `source/`. Workflow artifacts are
named with run and attempt and retained for 14 days. This receipt does not
establish complete product evaluation history across runs. QA3-08 still
requires a qualified claim backend and trusted controller before product
qualification or evidence reuse.

Child processes receive a small environment without credentials or runner option
injection. Each group has a 180-second timeout and a 16-MiB limit for regular
file writes (including cache entries and logs) applied before exec by prlimit,
runs in an owned process group, and is reaped/terminated on exit. This is a
Linux source host, not the future cross-platform GUI/Agent supervisor. An
isolated Python supervisor owns the command's descendant tree as a Linux
subreaper. It checks process birth identities and uses pidfds before signaling,
so detached descendants and forks during termination are included in bounded
TERM/KILL/reap cleanup. The calling process's subreaper state and unrelated
children are untouched. A bounded private control pipe reports completion;
missing results and incomplete cleanup fail the source run. This requires Linux
pidfd support, available on the Ubuntu 24.04 source worker. Cleanup syscall
errors are retained separately from the initial failure, and later
TERM/KILL/reap phases continue. A recorded cleanup error or supervisor
interruption prevents PASS, even if subsequent cleanup succeeds. Loss of the
supervisor through SIGKILL or a kernel stall requires external worker/job
teardown; an absent control result always fails the source run. Logical process
ownership is not a replacement for an external resource/teardown boundary.

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
stages. The initial Library materialization was blocked by HTTP 403. The kit was
subsequently reviewed from the user's immutable GitHub input, with its original
114 and separately repaired 130 host tests recorded in
[Floorp #2893](https://github.com/Floorp-Projects/Floorp/pull/2893). Those kit
tests are separate from the active source plan and do not qualify native builds.

Run the required workflow on a clean CI checkout and retain its original
attempt before accepting this change. Measure missing coverage, initial
failure/flake rate and resource use. Then
prove a Linux x64 Debug configure/support/harness with known-bad canaries and
repeat product evidence on Windows x64, Linux x64/ARM, Mac Intel/ARM. Keep Mac
universal bytes bound to both native executions. Use separate real MAR, DEB,
Portable and stub evidence. No such native execution is claimed by this change.

The future publisher must perform
`bundle -> final QA -> manual approval ->
fresh evidence revalidation -> publish`,
authenticate exact digests and retain the existing gates. The current manual
publication/approval flow is unchanged.
