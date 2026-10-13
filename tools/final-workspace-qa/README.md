# Final package workspace pilot

This implements a nonpublishing Linux x64 adapter for the existing six
`nativeHiddenTabTests(true)` regressions. It prepares and launches the exact
Linux member of an authenticated final production bundle. It does not launch
downloaded Firefox, a development build, Vite, HMR or a shared profile.

Implementation and host contract success are separate from product execution.
No final product has been executed or qualified during this implementation.
`PASS_WORKSPACE_BEHAVIOR_ONLY` can describe only the six required behavior cases
and owned cleanup. `nativeProductQualification=UNVERIFIED`,
`compileProfileQualification=UNVERIFIED` and `publicationAuthorized=false`
remain explicit even when those cases pass. The legacy v2 release schema does
not prove the complete compile/source/profile cohort. Existing signing,
Fluent/Flasco tests, native verifiers, approval environments and publishers are
unchanged; this pilot is not a publisher gate.

## Source and final artifact binding

The trusted caller supplies `request.json` from authenticated API identities,
not declarations inside a candidate bundle. It binds the controller revision,
execution run/attempt, full Floorp and Runtime source revisions, original
production run/attempt, target and exact outer bundle artifact ID/name/digest.
The independently supplied `provider-snapshot.json` is compared with those
expectations, including repository, successful original attempt, expiry and
the artifact's actual producing run/head. The caller must authenticate this
API read; a snapshot inside the candidate ZIP is never used as its authority.
All attempts must be the original attempt 1. The archive is copied privately,
hashed against that expected digest, safely extracted within fixed limits and
validated with the unchanged `validateReleaseBundle(directory, true)`. That
verifier checks the exact 16-file production bundle, signed-package evidence,
manifest identity, metadata and every final member. It does not independently
authenticate a supplied API snapshot or prove actual OS signatures or PGO
profile qualification. Those remain separate evidence obligations.

The selected final member is extracted without traversal, special files,
external links or writes through links. Internal hardlinks become regular
copies of the same authenticated bytes. Native ELF architecture, executable
mode and `browser/buildid2` are checked. The actual runtime app/platform
BuildIDs and Firefox version are checked again inside the launched browser.
The complete extracted package and support inventory is sealed before launch
and rechecked afterward. Preparation is creation-only, root-issued and private;
the worker receives a read-only copy. A failed preparation is not repaired or
silently reused in place.

Worker dependencies use the controller's explicit `deno.json` and existing
frozen `deno.lock`. Both files must match the clean controller revision; their
hashes are retained and rechecked before execution.

Seven source materials are fetched from the product's exact Git revision.
The source fixture, shared harness and production bootstrap remain unchanged
in the repository. Generation redirects the fixture's two primary assertion
imports through a counting proxy that calls the original functions, and adds
a catch before its existing finally to preserve the original exception. The
before/after source hashes and transform identity are recorded. Cleanup errors
and earlier assertion failures remain visible; no later success can recover
them. The generated payload has no remote imports.

## Required cases and execution

The selection is fixed: extension, unattributed and collapsed-group hidden tabs
with `closeWithLastTab=false`, then the same three with `true`. Their minimum
observed positive assertion counts are 12, 11, 11, 11, 10 and 11. The fixture
names and order must match exactly. No filters, skip policies or test retries
are accepted. Failure stops later execution and records the remaining cases as
`NOT_RUN`; that result cannot pass.

Bootstrap requires a fresh production startup marker, disabled HTTP loader,
the real workspace API and a packaged `chrome://noraneko/content/core.js`
registration resolving inside the selected read-only product. It does not
reimport or reinvoke the production bootstrap to conceal its initial failure.
The test payload is loaded into the real browser window from a local protected
file. The fixture opens real windows and native tabs with workspaces enabled.

The dedicated Marionette client connects and creates one session. It bounds
UTF-8 frames, validates message IDs, writes complete packets and closes the
socket at a handshake or command deadline. The initial loopback TCP connection
has no independent deadline yet; the supervisor and service wall bounds remain
its backstop. It preserves original incoming/outgoing frame text.
Bounded startup observations precede test execution and are recorded separately;
transport, session and test failures are never retried.

## Linux ownership and limits

`run_linux.py` creates one transient systemd service with DynamicUser, fresh
private HOME/TMPDIR, private loopback network, protected home directories,
read-only input and no inherited credentials. It does not create a login
account, credential, signing configuration or permanent service. The unit has
four CPU cores, 8 GiB physical memory, no swap, 512 tasks and a 20-minute native
runtime cap. Address space is not capped because Gecko reserves large virtual
regions. The root controller rechecks effective limits and isolation.

The worker reuses `createIsolatedBrowserLaunch` and `startIsolatedBrowser` with
an actual Linux controller. It verifies the native root's executable, PID
generation and membership in its otherwise empty service cgroup. All
descendants stay in that protected cgroup. Cleanup signals only its owned
service processes from the unprivileged worker, requires their absence and the
owned port's absence, and then removes the profile. The root controller stops
the complete unit, verifies an empty scope and retains bounded stdout/stderr,
wire transcript, original result and cleanup errors. A worker-written PASS
without successful root supervision cannot pass.

This OS path has been implemented and locally type-checked/bundled, not yet
qualified on a real native worker. Other OS/architectures need their own owned
process supervisors and final artifacts. Linux ARM production-opt and both Mac
architectures remain separate required production qualification work.

## Local host validation

```sh
deno fmt --check tools/final-workspace-qa/*.ts
deno lint --no-config tools/final-workspace-qa/*.ts
deno check --config=deno.json --lock=deno.lock --frozen-lockfile tools/final-workspace-qa/worker.ts
deno test --no-config --no-lock --allow-read --allow-write \
  --allow-net=127.0.0.1 tools/final-workspace-qa/contract.test.ts
python3 -B -m unittest discover -s tools/final-workspace-qa -p 'test_*.py' -v
python3 tools/final-workspace-qa/prepare.py build-support \
  --floorp-head FULL_PRODUCT_SOURCE_SHA --deno /absolute/path/to/deno \
  --output /absolute/private/new-support-directory
```

The PR workflow performs only these host contracts and support compilation on
a standard public GitHub runner. It never acquires or launches a product,
creates a privileged service or publishes. Original validation failures are
kept in the implementation handoff; host fixtures are not product evidence.

## Preparing a real limited run

After obtaining the authenticated original final bundle and freezing the
reviewed controller, the trusted root supervisor can prepare it with:

```sh
python3 tools/final-workspace-qa/prepare.py prepare \
  --request /trusted/request.json --archive /trusted/original-bundle.zip \
  --provider-snapshot /trusted/original-provider-snapshot.json \
  --deno /trusted/deno --output /private/new-preparation
python3 tools/final-workspace-qa/run_linux.py \
  --prepared /private/new-preparation --output /private/new-evidence
```

An authorized worker with systemd and cgroup v2 is required. Missing artifact
access is a blocker, not permission to bypass restrictions or substitute a
different product. Preserve the original artifact and authenticated caller
context separately from worker-authored receipts. Compare the original
production attempt with API evidence; do not infer it from the v2 Floorp fields,
which have no attempt member.

Before a paid run, reconcile the existing cumulative reservation. The existing
$1 phase cap includes the earlier $0.064 planning estimate, with a separate $2
reserve for unknown costs and a $10 total cap. It is not reset for this adapter.
Include acquisition, validation, support compilation, service and teardown in
the job cost bound; the 20-minute service cap is not a complete job estimate.
No new paid dispatch is part of this implementation or PR.

Final QA for publication still needs source-only QA once per package run,
qualified compile/profile evidence and product receipts for each unchanged
signed final member/native target. Preserve the sequence:
`bundle -> final QA -> manual approval -> fresh evidence revalidation -> publish`.

## One-use issuance and cancellation

Before spawning, the supervisor atomically consumes a protected root claim for
repository/controller revision/execution run/attempt/target. Failure or cancel
retains that claim and the original root-result reference. A new output or
preparation path cannot reuse the same original execution. These claims apply
to one supervisor host boot because `/run` is volatile; cross-worker issuance
and long-term original failure
retention still belong to the authenticated CI caller and evidence store.

The fixed trusted Deno worker requires `--allow-run` for `Deno.kill` of its owned
descendants: restricting it to the browser executable makes that API fail.
Process, credential and network isolation are enforced by the unprivileged
systemd/UID/cgroup/filesystem boundary, and the worker code is pinned and
read-only. That broader Deno capability is not a product security qualification.

SIGTERM and SIGINT retain the original cancellation and enter bounded unit
cleanup; later signals during cleanup do not overwrite the first failure.
SIGKILL or host loss cannot produce a successful root receipt. The service
runtime cap remains a backstop, and absent cleanup proof stays unverified.
