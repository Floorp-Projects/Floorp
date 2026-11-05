# KAN-29 Arabic startup memory investigation — 2026-11-06 JST

Cause remains unconfirmed; no product patch or new Draft PR. This investigation does not establish attribution to PR2877, nor qualify a full157 RTL layout state. Existing source/CI failures and original LTR CachyOS/Flatpak conditions remain unchanged.

## Evidence and isolation

Read existing startup logs, allowlisted profile differences and resource curves first. Independent source tree is clean at parent a6290b5811b8c5309b399ac340ec40b52f97b73c. Official Floorp12.20.0 parent and PR2877 f43b22d candidate share native157.0.1 Build20261007125927 and identical floorp/libxul hashes (Runtime c74fe1a545d275cdd710ab44701c1ee0dd6b0eae). Parent is an eligible same-Runtime control; installed Firefox159 is not. Stock Firefox157 was not downloaded/executed and its native identity is unverified. No new build was started.

Existing candidate first-Arabic attempts after normal signed-pack setup also grew; forced-stop profile reuse is not necessary. One additional short observation used the official parent, a fresh profile, signedState2/appDisabledfalse matching157 Arabic pack, normal English child exit0, and its first requested-ar startup. No migrated or previously forced-stop profile was used. A lock-copy helper failure happened before Arabic launch and was repaired without another bootstrap. Capture counts: English setup1, Arabic1, retries0.

## Observed resources and exits

At19.890s: aggregate RSS1.656GiB, main RSS1.381GiB, known PSS1.430GiB/main1.270GiB. Between14.328–19.890s main RSS grew185.6MiB/s, process CPU averaged1.005 cores; this is resident growth and whole-process CPU, not allocation rate or a GeckoMain stack. Target loop ended20.083s. During quit grace, cgroup6.040GiB guard triggered at24.278s (owned RSS2.483GiB, known PSS2.225GiB); own cleanup SIGTERM gave actual Floorp child -15. CtrlQ helper exit0 does not establish normal product exit. OOM/kill deltas0; all46 payload entries unchanged. Sampling median0.402s/maxgap6.653s means observed peaks are not continuous hard upper bounds.

## What remains unresolved

No profiler JSON or memory report was obtained. Feature `cpu` is invalid in native157 (correct name `processcpu`); parser retains other recognized features, so this warning does not prove all profiling was disabled or explain absent output. Registered SIGUSR2 was sent once; memory-report SIGRTMIN was not registered and was deliberately not sent. No JS heap/DOM/native allocation breakdown exists. Target live appLocale/RTL_UI was not queried: requested-ar is not qualified RTL conformance. Profiler overhead remains a confound.

Bootstrap had one remote non-system about:blank tab, but browser.startup.page=3, so SessionStore/window/UI restoration remains in scope. Main-process concentration does not exclude privileged about:newtab/home. Read-only code analysis weakens simple repeated language-pack-install or self-triggered Fluent DOM mutation loops but identifies no causal allocation path. Native157/159 DocumentL10n readiness/error-handling differences are hypotheses only, not a fix justification.

Default tool sandbox denied disposable-child ptrace; authorized native execution successfully attached/detached its own disposable child. Floorp was not attached. gdb/perf/eu-stack unavailable; libxul is stripped, GNU build-id6f03efb2d7e5d066ccadadc8d799831eeaa18229. Do not report blanket debugger permission absence.

## Disposition

No further browser execution, new build, PR2877 modification, CI/config/protection change, release, merge or Done transition. No baseline-failing causal regression/candidate effect exists, so no speculative Draft. Original10/Linux22 denominator and separate additional-KAN table are preserved. Private profiles/raw logs/raw results are excluded from this evidence branch; hashes, technical aggregates, historical scripts and public-source excerpts are included. Historical scripts are not validated profiler recipes; read capture/README.md before any future execution.

## Follow-up: English instrumentation established (2026-11-06 JST)

The earlier “No profiler JSON or memory report” statement describes the historical Arabic capture only. The subsequent [English preflight report](../instrumentation-preflight/report.md) and [summary](../instrumentation-preflight/summary.json) establish one fresh English PID119877: API1299 samples and registered-USR2 parent1408 samples with stacks, same-PID nonminimizing memory report1784 parent entries, actual native child exit0. Jira10490; immutable evidence commit782ebc039cac922c73ce3e10462b4c699fd8785a. This satisfies next-observations1 and English scope of2. It does not establish reporter initialization in a new Arabic process, Arabic output, allocation cause, or qualified RTL. Historical Arabic results remain unchanged.

## Subsequent single Arabic capture

See [early-capture report](../arabic-early-capture/report.md) and [summary](../arabic-early-capture/summary.json): first requested-ar PID120948 yielded1035 parent samples via registered SIGUSR2 at6.100s with valid startup features/cache. Registered nonminimizing SIGRTMIN at6.502s did not produce a completed memory report. RSS2GiB polling guard fired9.882s, actual child-15, not normal exit. ContextMenu startup catalog -> Localization.formatMessagesSync dominates599/612 post3s samples and matches existing merged PR2883; original missing key/candidate effect on157 not measured. Historical failed capture is retained. Full157RTL remains unqualified and PR2877 attribution unproven. No further Arabic run.
