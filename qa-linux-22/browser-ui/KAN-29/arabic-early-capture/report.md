# KAN-29 first-Arabic early profiler capture — 2026-11-06 JST

A single bounded first requested-ar startup now produced a real early profiler JSON. Busy-path evidence strongly matches the already-merged context-menu localization fix PR2883. No duplicate Draft is warranted. This does not prove the exact missing message, allocator/DOM category, effect of applying2883 to this157 package, or original KAN29 LTR/CachyOS resolution. PR2877 attribution and qualified full157 RTL remain unproven.

## Product and preparation

Same official parent a6290b5811b8c5309b399ac340ec40b52f97b73c, native157.0.1 Build20261007125927, Runtime c74fe1a545d275cdd710ab44701c1ee0dd6b0eae. Product floorp/libxul/root/browser omni hashes matched before/after. Debian13/Xvfb131/Openbox/isolated DBus; synthetic automation, not physical input. Exactly1 fresh English setup and1 first requested-ar start. No profile migration/copy or prior Arabic/force-stop profile reuse. English PID120558 verified en-US/RTLfalse and normal Services.startup.quit actual child0 after ordinary matching signed Arabic pack install (signedState2/appDisabledfalse), requestedLocales ar and pref save. Sandbox6/langpack signaturestrue live checks; no verification disabled.

Startup profiler uses the English-validated js,stackwalk,processcpu,memory/GeckoMain/5ms, actual capacity16777216 entries (requested string8MiB is not8MiB total RAM). Read-only exact157 source shows profiler_init/start before later startup module work and output-directory cache populated from MOZ_UPLOAD_DIR. In Arabic PID120948, native startup log explicitly records all4 features, interval/capacity and cached output directory. Parsed target JSON confirms them, so validity is demonstrated in that process, not inferred from English alone. MOZ_LOG prof:4 was added for private diagnostic logging; profiler/log overhead remains a confound. Arabic made no Marionette NewSession/RPC connection.

## Early output and exits

English control SIGUSR2 at7.042s produced1156 parent samples/stacks. Arabic SIGUSR2 at6.100s, after caught-mask registration check, produced profile_0_120948.json:4051138 bytes, SHA2566edc45ba2a402bdd5d1140e16e76869001772652c9e608b320605bbe76807bba,1035 parent samples, timestamps2.012–6094.374ms, stacks present, matching4 active features. This establishes early parent output before waiting for large growth.

Arabic SIGRTMIN was registered in its own PID at6.502s and sent once. It is the nonminimizing signal; no SIGRTMIN+1/forcedGC/minimize. No completed memory-report file was found in the private output or own-PID tmp search before cleanup. Registration is not completion. Blocked main-thread service is plausible given synchronous localization samples, but the missing report cause is not directly proven. No JS heap/DOM/native memory-category breakdown was obtained.

Owned aggregate RSS reached2166124KiB (~2.066GiB) at9.882s; the2GiB polling threshold triggered own SIGTERM. Capture loop ended9.915s. Actual Arabic native child wait-15; normal exitfalse, no CtrlQ-normal claim. Guard was the RSS condition (cgroup5884895232B <6GiB), not a host OOM conclusion. English child0 and Arabic-15 remain separate. Samples/guard latencies are in resources/metrics; soft thresholds can overshoot. Arabic target15s hard-stop policy was not reached. No second Arabic run.

## New narrowing from real parent stacks

Of612 parent samples after3s,599 (97.9%) include Localization.formatMessagesSync; corresponding English post3s count0/728. Arabic chain: MozAfterPaint -> initializeModules -> ContextMenu.init/attach -> seedInitialCatalog -> catalog.seed/recordSurface/recordContainer -> getItemLabel/getLabelLocalizer -> Localization.formatMessagesSync. This is startup catalog population, rather than an observed repeated locale-install callback, restored content page loop, or direct PR2877 hover layout call. Actual argument/key and native inner frames are not resolved. Do not call the sampled inclusive percentage an allocation percentage.

Emitted malloc counter at3s/6s: Arabic293.1/946.6MiB versus English119.4/398.9MiB. These are cumulative emitted counters, not a JS-versus-native/DOM attribution or a controlled allocation-rate comparison. English is the first fresh startup with Marionette, Arabic the normally restored second startup without RPC, so phase/profile/cache differences remain.

## Existing fix overlap

Read-only GitHub lookup found already MERGED PR2883, headcfe5edbaa0391fbcf8f4d3e6fdb758742b5be6a6, merge6c3f11dfebe97fee2be25fad915b4e51cc1f1b4f (2026-10-08). Its body describes missing Fluent labels causing combinations of optional resource/source bundles and memory runaway. It searches one required candidate resource with branding at a time and adds a real-Fluent missing-key + subsequent valid label regression. Existing Windows Japanese baseline2GiB failure/candidate success and70 tests are the PR author's reported evidence, not tests performed by this task.

Exact parenta629 and PR2877f43 both contain the same vulnerable catalog blob e1cee1519cf61cd4c19b2d7a6f4a398590e49792 (all document localization resources optional, synchronous format). PR2883 merge and main984509d contain identical fixed blob a3f91ca4c5c6bc00ecb9854495c33fa31ed061aa. Public PR metadata/diff are included. This is strong existing-fix correspondence; this run did not execute a2883 candidate on157 or preserve the offending missing key. No new patch/Draft, no claim original KAN29 is fixed. PR2877 head/source untouched.

## Remaining evidence

A future approved candidate comparison should reuse the existing2883 correction/test, not invent a new fix. Exact first missing Fluent key/resource/source set and native inner allocation category remain unknown; retrieve without repeating unbounded startup. Full qualified Arabic RTL paint remains0; original LTR CachyOS/Flatpak/physical/compositor conditions remain. Original10/Linux22 denominator unchanged and additional KANs separate. Private profiles/raw profiler/memory/log contents are excluded; only aggregates/digests/public technical frames and selected native startup log are published. No build/CI/lock/security changes, merge/release/Done.
