# Native157 capture limitations — readonly follow-up

Scope: exact native c74fe1a545d275cdd710ab44701c1ee0dd6b0eae code and the existing sole fresh-parent Arabic capture. No browser/test/build/profiler/signal or production change was run here. Raw capture logs remain private; only interpretation and source excerpts are saved. Underlying RAM cause, JS heap/DOM/native allocation category and busy stack remain unmeasured.

## What the attempt establishes

The log prints unrecognized feature `cpu` in BaseProfiler and GeckoProfiler help. That means this token is invalid, **not that the whole profiler was disabled**. Both parsers return0 for an unknown token and OR every remaining feature into the mask: Base platform2271–2301, Gecko platform5787–5817. `js`, `stackwalk`, `memory` are valid source names; `processcpu` is the correct CPU-utilization feature (BaseProfilerState212). Available features still depend on the actual build. Requested environment is not proof of an active recording or completed dump.

signal-events.json records SIGRTMIN34 unregistered at3s/7s, correctly not sent. It records SIGUSR2 registered and sent around10s, but no profiler JSON resulted. Capture inventory has only logs, not shutdown/profile/memory-report JSON. The final actual child exit was-15, so the normal profiler-shutdown hook was not demonstrated. Missing shutdown.json after forced cleanup does not prove no samples existed.

## Independent prerequisites source exposes

| Prerequisite | Exact c74 source | What is not proven by the previous attempt |
|---|---|---|
|Profiler active/configured|platform6378–6405 parses environment;6795–6805 returns without saving if ActivePS is absent|A registeredUSR2 handler does not establish ActivePS, enabled feature mask or usable samples. Unknowncpu alone cannot explain total inactivity.|
|Signal control pipe/thread working|platform5987–6001 writes stop byte only when pipeFD !=-1; constructor680–703 creates pipe/thread|Caught-signal bit proves handler registration, not successful pipe delivery/background-thread progress.|
|Cached output directory|platform7400–7440 caches MOZ_UPLOAD_DIR in CorePS. nsAppRunner6198–6211 calls lookup after prefs/directory initialization and before the following startup module work.|Environment variable presence and filesystem directory existence do not independently prove CorePS cache was populated in that process.|
|Actual dump path and write|platform6008–6058 needs cached directory and writes `profile_<processType>_<pid>.json`;6100–6167 saves parent first, then requests multi-process collection;6795–6808 can return if inactive|USR2 is not writing specifically MOZ_PROFILER_SHUTDOWN. No JSON means one of activation/control/cache/path/write/serialization prerequisites remains unresolved; blocked child collection alone is insufficient because parent save is attempted first.|
|Memory-report signal service initialized|nsMemoryReporterManager.cpp1803 calls nsMemoryInfoDumper::Initialize; dumper242–253 registersRT signals under MOZ_SUPPORTS_RT_SIGNALS|XPCOM/profiler startup alone does not guarantee memory-reporter manager initialization. An unregistered mask is real and must not be bypassed. Initializing service in the earlier English process would not register handlers in the new Arabic process.|
|Memory report main-thread progress|dumper131–140: signal→pipe→IO thread→main-thread task|Even a properly registered signal may not finish if the main thread is blocked. Missing report is not absence of a leak.|

Source excerpts/index preserve file/line/SHA evidence without changing native/source. SIGUSR2 and SIGRTMIN have different initialization and execution paths; neither missing output nor one parser warning identifies the RAM allocator.

## Precise next observation, only if root authorizes it

1. Preflight instrumentation on a bounded normal English control before relying on Arabic output: exact157 available feature names via nsIProfiler.GetFeatures, IsActive and active configuration (IDL160–185), known private writable dump path and profiler logging. Use `js,stackwalk,processcpu,memory`; avoid nativeallocations/jsallocations or tracing as an automatic low-resource substitute. Require an actual small parseable parent JSON with samples/stacks, not just a registered handler or env request. Preserve source/profile/native identity and code-recorded feature mask. This preflight is proposed, not executed.
2. If usingUSR2, inspect expected `profile_<type>_<pid>.json` plus profiler activation/path/control/write logs. Do not infer success from signal-send return. Native signal dump is designed to save parent before blocked multi-process collection. Bound observation time, file size and memory; preserve incomplete/absent artifacts as failures.
3. For a native memory-category report, early initialization must occur in the **same Arabic process**, through an explicitly approved instrumentation route before startup becomes unresponsive, then verify its actual caught-signal bit. Do not resolve startup guards artificially, enable unsupported signals or assume English-bootstrap initialization persists across normal restart. Non-minimizingSIGRTMIN only when registered; notSIGRTMIN+1.
4. If same-process service/active-profiler prerequisites cannot be established without changing the product path, prefer one bounded external own-main-child stack snapshot with verified ownership and exact ELF build IDs, subject to available ptrace capability. Keep raw stack/address data private; missing symbols are unresolved frames, not JS attribution. A `/proc/.../stack` kernel wait alone is not a userspace/JS stack. No new symbol/toolchain download or production edit is implied.

The existing root plan's fresh clean preRTL snapshot and direct child wait address earlier profile/exit confounds. The remaining prerequisite is **demonstrating that the chosen capture mechanism can actually produce the intended observation**, not adding more UI starts or increasing the resource ceiling. No existing validated fix overlap was found in the already-read I18n/newtab paths; those finite/error paths remain unproven memory causes.
