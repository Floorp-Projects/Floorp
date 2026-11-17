# Debug artifact consumer: local implementation candidate

Status: NOT a workflow replacement yet. No new workflow has been enabled, dispatched or published. Existing exact-PR #3138 same-worker pilot is untouched. All code here is a Linux x86_64, opt-in/non-required foundation; no Marionette or release packaging changes.

## Implemented

* `artifact_transport.py`: default-branch workflow_dispatch-only trusted downloader. Its approved pin must be read from trusted base, never PR source. Authenticates exact Runtime Actions artifact ID/digest/name and source run/workflow/attempt; reads PR metadata and the Runtime lock at the exact candidate SHA. Existing Runtime credential only goes to api.github.com, never the signed archive storage redirect. PR reads use a separate GitHub read token. It does not execute candidate code.
* `artifact_preflight.py`: read-only ZIP/receipt preflight, outer and inner SHA256, fixed wrapper entries/type/size, exact normal producer context and compiled productProfile, ABI equality. Explicitly returns TRANSPORT_ONLY_NOT_NATIVE_EVIDENCE. Does not pretend to replace the producer's deep validate_producer/verify_support, source-cohort validation or ELF parsing.
* `artifact_assembly_recipe.py`: compiles 11 existing package.yml phases (bash syntax checked), preserving `--enable-artifact-builds`, adding Debug configure, binding the existing official `MOZ_ARTIFACT_FILE` path to authenticated bytes, and avoiding Native compile/install-tests. It reuses the actual separate `Build` phase, not just the frontend `Build Noraneko` phase. The final integrity prerequisite rejects changed floorp/libxul.so bytes and wrong packed browser.xhtml, but intentionally does not claim full chrome registration or native execution.
* Unit regression fixtures for identity, fork/stale head, lock mismatch, redirect credential boundary, corrupt/foreign/prequalified receipt, host mismatch, unchanged engine and wrong XHTML.

## Shared producer contract

Actual producer `runtime_native_debug.context_from_environment` and `validate_context` were executed with a hermetic normal workflow fixture. Consumer metadata context matches exactly: qa3-hosted-context-v1, runtime-native-debug-artifact-v1, Runtime repository/source, source-bound BuildID/run/attempt. Normal artifact namespace includes full source SHA/BuildID/run/attempt. Validation-only premerge producer profile/namespace is intentionally rejected by this consumer.

Normal compiled productProfile includes the original Floorp ea85c9274d6959e87d3bf3bfa9bc0d0c13971ef5 CSP patch hash 1d5c76d89966cefc4e51cd077e20c1a5cd8ca6094bc9ae8ae9635c13b4adba62. Approved pin must contain the exact full productProfile. Production must additionally verify that the candidate's required native patchset is compatible with that profile; matching a source SHA alone is insufficient.

## Smallest next executable trial

1. Keep old pilot; add a separate manual workflow on main only. Trusted download job checks out trusted default-branch source, consumes a reviewed approved pin, accepts only exact open same-repo PR/head. Give the existing read-only Runtime credential only to this job. Never pull_request_target or checkout candidate code in this job.
2. Transfer compact authenticated artifacts/metadata to a distinct no-cross-repo-secret execution job. Candidate frontend builds/tests run there with no inherited token or GitHub command-file channels. Recheck candidate head and digests at the execution boundary.
3. Invoke pinned producer `validate_producer`, source/host/build identity and safe archive extraction/verify_support. The packaged full common/mochitest/xpcshell assets are usable candidates; the embedded four-case QA3 canary plan is NOT the P0/P1/P2 schedule.
4. Stage exact primary bytes as `floorp-linux-x86_64-moz-artifact.tar.xz` for existing artifact-mode `Build`. Reuse normal source/branding/frontend/injection/preferences/package phases in a fresh assembly OBJDIR. This is a newly generated packaging OBJDIR, never a relocated native OBJDIR. Existing default release/download lookups must not run. Preserve the producer's build ID and verify final floorp/libxul.so SHA256 and build identity.
5. Verify actual packed PR frontend registration and resource contents. Resolve selected browser/xpcshell tests from authenticated packaged manifests, not config.status/all-tests.pkl from a fake native OBJDIR. Record exact selected tests, prefs and known differences. Require full native assertions + clean exit and pass current strict gate semantics. Fail-closed on unsupported native edits, host mismatch, missing scope or missing assembly evidence.

## Remaining blockers (must be resolved before publication/merge as replacement)

* No real normal producer artifact pin yet; no download or cold native test performed.
* The official artifact-mode assembly recipe is compiled/static-tested, NOT executed against the Debug primary. Setup dependencies and build identity propagation require real-worker proof. Current final checker is only prerequisite integrity, not a complete product registration verifier.
* Current local_cohort/resolve_native require same-worker native OBJDIR/config.status/all-tests.pkl and cannot authenticate this cold support directly. Need packaged-manifest schedule and adapter integration, preserving missing/all-skip/failure checks. Do not manufacture an OBJDIR receipt.
* Must verify compiled native patch compatibility for exact PR UI, not just the CSP patch receipt.
* No GitHub workflow YAML is added until the real executable consumer can return meaningful native evidence. A permanently-red placeholder or green preflight-only job is not a replacement.

No permission broadening, secrets additions, release changes, publication, PR or merge performed. Unit tests and synthetic ELF fixtures are host contract evidence only.


## Phase 2 local extension (still no native execution)

This patch is explicitly STACKED ON #3138 `ea85c9274d6959e87d3bf3bfa9bc0d0c13971ef5`, not a standalone main change. `build_recipe`, `resolve_native`, `ci-adapter` and `gate` remain dependencies of that pilot. Integration must bring the reviewed dependency set or remain stacked.

Implemented `cold_schedule.py`: maps the seven exact Firefox browser/xpcshell manifest paths into authenticated packaged support; emits native harness commands; translates actual --dump-tests records into the existing hash-bound gate plan. It rejects replacement canary paths, omitted/duplicate groups, missing tests, all-disabled groups and foreign/mutable-changed dump paths. Native manifest conditions remain in the original TOML. xpcshell requires the existing appdir adaptation validated by packaged manifestparser and uses assembled Floorp `--app-binary`, never the support shell. Browser uses a mutable verified mochitest copy with `cold_browser_entry.py`, no retry/repeat/filter; source support remains frozen. The command runner must set COLD_MOCHITEST_ROOT and the already-qualified offline Python/module environment. No pretend OBJDIR or config.status is created.

Implemented `cold_adapter.py`: exact plan/result/exit/raw-log binding, current strict gate aggregation and actual per-test xpcshell argv audit are reused. It refuses incomplete executions, cleanup uncertainty or product/support mutation. It does not generate synthetic PASS or run host contracts as product checks. A controller still has to run/own the native processes and authenticate before/after product/support inventories.

Implemented `packed_registration.py`: consumes all eleven generated production jar.mn mounts, follows only reachable packed chrome manifest registrations, verifies complete generated file hashes and generated overrides across both omni archives, rejects missing/shadowed/remote/stale registration. Unsupported packaging grammar fails closed. This is static packed-product integrity, not browser startup proof. Combine it with the previous unchanged floorp/libxul.so and actual injected packed browser.xhtml checks.

Separate Debug source qualification is now explicit: trusted pin may carry schema floorp-debug-qualification-v1, scope isolated-browser-xpcshell-qa, exact production_runtime_sha and debug_runtime_sha, and production_equivalence=false. This deliberately does NOT rewrite production floorp-runtime.lock.json or claim a newer Debug engine equals the production engine. No record means a mismatched engine is rejected. The event/workflow allowlist now permits schedule only with daily-build, matching producer rules.

First real required-check candidate should prioritize actual Firefox tabs + session/tab restore behavior (tab retention, order, selection) before broadening containers. No branch rule was changed. A focused subset needs its own explicit approved scope and cannot pass this seven-group full pilot gate.

## Precisely blocked real proof

No qualified normal Debug primary/support artifact is available in this lane. Consequently the compiled recipe has NOT assembled a real PR UI onto that primary, no real generated jar.mn/packed registration was inspected, no actual packaged native dump ran, and no cold Firefox test ran. Tests use clearly synthetic archive/ELF/dump/mozlog fixtures solely to validate rejection/control flow. The next bounded real operation needs approved normal artifact ID/digest/source, trusted QA qualification pin, matching Ubuntu24.04 ABI/offline environment, actual artifact-mode assembly, then native tabs/session smoke with clean exit. Keep old pilot and Release intact until this exact proof succeeds.


## Phase 3: bounded tabs/session qualification (not required)

`focused_qualification.py` fixes two original Firefox test selections:
- tabs/browser.toml: browser_open_newtab_start_observer_notification.js
- sessionstore/test/browser_oldformat.toml: browser_906076_lazy_tabs.js

The second has historical PASS and identical source blob at 741c/b72ff4d,
but current Debug-cohort membership and native execution remain mandatory.
It covers lazy-tab restoration count and selected/second URI, NOT arbitrary
full order/no-loss safety or real restart/workspace retention. The prior
restoreLastActionCorrectOrder candidate is excluded: historical TIMEOUT and
count-only assertions. Marionette restart coverage remains a separate lane.

The command builder and focused entry point retain original manifest predicates
and select exactly one fixed test per invocation. Native dump resolution binds
the original manifest, test bytes, and native dump. Finalization reuses strict
raw mozlog checking, requires two exact tests with exit 0, no skips, missing,
zero or unknown outcomes, and frozen material/owned cleanup attestations.
This is an adapter contract, NOT an authenticated execution controller.

### Workflow/controller hold points
No workflow YAML is added: a runnable consumer cannot honestly be completed
until the normal producer artifact exists and the following boundaries are
integrated and exercised. Do not convert these into a preflight-only green job.
1. Default-ref-only orchestration downloads by controlled reviewed Debug pin,
   with existing scoped Runtime token in download-only job; no fork path.
2. Secretless execution independently validates outer and deep receipt/support,
   then uses exact reviewed Runtime qa3_native_io extraction and run_owned.
   Do not vendor an unpinned helper or replace cleanup with process-name kills.
3. Demonstrate primary MOZ_ARTIFACT_FILE compatibility and unchanged ELF bytes
   after official artifact-mode production UI assembly; inspect real generated
   registration using packed_registration.py.
4. Construct offline Python environment and verified mutable native harness;
   run focused native dump and test commands using controller-owned argv,
   clean environment, bounded logs and owned-process cleanup. Record inventories
   before and after; never trust PR-authored boolean attestations.
5. Only the strict focused native result, not these host fixtures, can qualify
   the candidate. Missing artifact/assembly/native evidence is BLOCKED, not PASS.

Production lock, release workflows, existing pilot and branch rules untouched.


### Primary equivalence resolved; material-stage controller added
Reviewer verified producer bf6a254 copies the original primary tar unchanged
as runtime.tar.xz; primarySha256 binds these exact bytes. MOZ_ARTIFACT_FILE
accepts that local tar irrespective of filename. Keep Apply upstream patches,
including artifacts.py.patch product=floorp mapping, before artifact install.
This resolves format provenance, not an actual Debug assembly execution.

qualification_materials.py now performs secretless, non-root staging with
exact Runtime-helper git HEAD and byte-for-byte committed .github validation
(including detection of assume-unchanged changes), normal-profile-only receipt,
inner archive hashes, existing bounded safe extract_archive, deep primary
identity, recipe metadata and full support verification. It uses the exact
Runtime helper tree rather than copying those implementations. No arbitrary
unverified helper is imported. Caller must first authenticate API/outer wrapper
and controlled pin; this is not a substitute for artifact_transport/preflight.
CLI deliberately exits 2 after materials verification: assembly/native runner
is not connected, so scheduling it cannot issue a green qualification result.

Remaining exact integration obstacle is controlled artifact-mode assembly
layout/toolchain environment and separate worker ownership: package scripts
expect normal checkout/build paths and frontend toolchain, while qa3 run_owned
expects bounded native process ownership. Need a reviewed layout adapter plus
actual pinned-source fixture before wiring execution jobs. No unreviewed shell
recipe or PR-authored attestation is accepted as completed native execution.


## Phase 4: clean official assembly layout
`assembly_layout.py` is an executable staging CLI, not a build or gate.
It exports exact pinned local git commits using independent no-hardlink clones,
removes source remote configuration, leaves dirty/untracked original files alone,
stages the authenticated primary into the official HOME/artifacts filename,
and compiles the trusted digest-bound normal package workflow into 11 frozen
scripts. Fixtures execute real git clone/checkout and bash syntax checking,
not an assumed layout. Git subprocesses have bounded 180-second timeouts and
a clean environment; no network remote, hooks, credentials or candidate script
execution is involved. Production runtime lock is never rewritten.

Staging is not a security boundary by chmod alone: protected() requires input
and every ancestor to be owned by a different UID and not group/world writable.
The staging CLI deliberately returns 2/BLOCKED until a controller establishes
that protected worker boundary and readiness. It does not chown, sudo or alter
permissions outside the newly created scratch. No arbitrary command runner
is exposed as a substitute for reviewed owned-process orchestration.

Concrete missing official prerequisite: package.yml `Setup` separately runs
apt installs and `mach bootstrap --application-choice browser_artifact_mode`.
Those are intentionally not in the eleven compiled phase scripts. A controlled
preprovisioned toolchain/bootstrap receipt must precede build execution; staging
alone cannot assert dependencies are present. No installations were performed.

The two-case smoke only qualifies assembly first. After real assembly/registration
and smoke pass, the shared batch-runner lane must schedule full original P0 tabs
and SessionStore manifests; do not treat the two cases as bulk coverage, nor
replace their native tests with these host fixture results.


## Phase 5: callable owned assembly execution
`assembly_executor.execute` now drives the official phase scripts through the
pinned qa3_native_io.run_owned interface, with explicit 1200-second artifact
bootstrap and 1800-second per-phase limits, 14 GiB memory, 16 MiB logs and
10 GiB free-space reservation. It checks source revisions/clean tracked state,
exact digest-derived phase sequence, protected script/deno/workflow ownership,
primary digest, credential-free worker environment and producer BuildID.
After injection/package it checks actual browser.xhtml and unchanged primary
floorp/libxul ELF bytes via the existing verifier. It returns assembled-but-not-
qualified; registration and actual native batch execution remain separate.

Official Setup is validated byte-for-byte from trusted package.yml and split:
apt update/install msitools jq belongs ONLY to prior trusted image provisioning;
mach browser_artifact_mode bootstrap runs unprivileged between Setup Noraneko
and Write Versions, preserving official order. This interface never invokes
sudo/apt or offers an arbitrary command callback in a privileged worker.
Host fixtures use fake owned execution, verifying all 12 command positions,
timeouts/environment/BuildID/MOZCONFIG and downstream integrity boundary.
They do not claim bootstrap or native build success.

One actionable next integration: supply a disposable provisioned CI worker and
controller-owned protected recipe/primary with writable candidate clones, then
invoke this adapter with the exact reviewed Runtime io module and actual normal
artifact. The controller must establish this UID/layout boundary; do not add
an enabled workflow claiming safety merely from chmod or a PR-supplied record.
No workflow is activated until that integration is independently reviewed.


### Deno executable review fix
Executor builds a controlled PATH beginning with the protected exact Deno parent,
rejects non-absolute/colon/newline paths and verifies executable regular-file
identity plus PATH lookup before every owned invocation. Deno bytes are frozen
by digest across phases. Compiler normalizes only the exact SHA256-reviewed
legacy Build Noraneko probing prefix, replacing hardcoded candidate selection
with command-v equality against DENO_EXECUTABLE_PATH and readonly binding.
All other official phase bodies remain unchanged. A real bash subprocess fixture
runs the normalized build phase against a benign executable and confirms its
exact path; malformed PATH, mismatched resolution, and wrong declared binary
fail. No Deno install or production frontend build was performed.
