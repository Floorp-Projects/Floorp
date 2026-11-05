# One fresh first-Arabic capture: aggregate findings

Exactly **one fresh English bootstrap** (nativePID118661, normal child exit0) and **one Arabic capture** (nativePID119072) were performed. Official Floorp parenta629, Runtimec74/native157.0.1 build20261007125927, detaches the observation from PR2877 attribution. The bootstrap copy helper followed a dangling lock after successful normal quit; root repaired lock/symlink copying and resumed **the same normally-ended fresh profile**, with no second English or Arabic launch.

Bootstrap live proof: English locale, one non-system remote `about:blank` tab, resumeOncefalse (startupPage3). Normally installed Arabic157 language pack signedState2/appDisabledfalse; preRTL copied prefs requested`ar`, addon active/signed2. Clean snapshot has72entries, digest in analysis-summary.json. Fresh profile and normal ending distinguish this from prior recovery-state copies. Snapshot/profiles remain private. Bootstrap observed default signaturesRequiredfalse/userSetfalse, **langpackSignaturesRequiredtrue/userSetfalse**, sandbox6, focus testmodefalse. No protection settings changed.

## Measured growth and CPU

Capture stopped its target loop at **20.083s**. Last sampled point within that interval (19.890s): aggregateRSS **1.656GiB**, mainRSS **1.381GiB**; knownPSS **1.430GiB**, mainPSS **1.270GiB**, children knownPSS **164.2MiB**. Five children observed. MainPSS cache age0.211s.

From14.328–19.890s, endpoint resident growth was **199.5MiB/s aggregate**, **185.6MiB/s main**; process CPU tick deltas correspond to **1.005 CPU cores main** and **0.270 children**. These are resident-growth/CPU averages, not allocation rates or heap attribution.

CtrlQ command returned0 after target stop, but native quit was not established. During its grace wait the **cgroup6GiB** guard interrupted; ownedRSS was still below3GiB. Last observed peak at24.278s: aggregateRSS **2.483GiB**, knownPSS **2.225GiB**, cgroup **6.040GiB**. Native child was terminated with SIGTERM, direct exit**-15**. Full46-entry product payload unchanged; all cgroupOOM/kill/event deltas0. Runner119057 and outerDBus wrapper are separate; their status is not inferred from child-15.

## Capture failure and measurement limits

SIGRTMIN requests at **3.427s / 7.081s** found no caught bit, so **neither was sent**. SIGUSR2 at **10.288s** was registered and sent; no profilerJSON file appeared. No memoryreport file appeared. Startup logged an unrecognized`cpu`feature warning; this does **not** establish that valid other features were inactive or explain the absent profile. JS heap, DOM counts, JS/native stack attribution and strictRTL_UI remain **unmeasured/unresolved**. No KAN29PASS.

Fast polling requested0.2s but did not achieve that bound: observed owned-sample gaps min0.365s/median0.402s/**max6.653s**, largest gap3.880–10.533s; first owned sample3.880s. One sampler call itself took6.266s. Separate slowPSS maximum observed read duration1.603s, maximum cacheage1.041s. Thus observed peaks are sampled values, **not continuous hard memory upper bounds**; unobserved excursions cannot be excluded. Capture/profiler overhead and process startup timing also limit comparison to earlier runs.

Only curated numeric curves, technical proof and hashes are publishable. Raw captures/profiles/logs/results remain private. No further browser, download, source, PR, CI, lock or build action was performed during analysis.

Interpretation refinement: startupPage3 leaves normal SessionStore/window/UI restoration in scope even though the only clean bootstrap page was a new remote about:blank. This excludes imported user pages and prior forced-stop/migration as necessary conditions, not restoration itself. Target live appLocale/RTL_UI was not queried. CPU measurements are process totals, not a sampled GeckoMain-thread stack. The `cpu`feature warning is an instrumentation defect, but does not by itself explain absent output or disable all valid features. No second Arabic capture is authorized/performed.
