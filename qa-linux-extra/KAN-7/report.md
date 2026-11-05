# KAN-7 / GitHub2943: Wayland startup hang, additional investigation

## Outcome

Original-condition startup failure remains **environment-blocked / not independently reproduced**. This follow-up performed read-only EGL/device/session capability measurements and pinned-source/version analysis, not another software-rendered browser launch. New product startup trials: **0**. No verified Floorp defect, external-dependency attribution or fixed-version claim; no patch/PR. Existing22-issue denominator and C's five-issue matrix are unchanged. This is a separate additional issue.

Current main is29a3eb23145da90706d8a94441373278521cc737; its only delta from the previous984509d2053be7bae9027133bda7259cd706b208 baseline is the OpenBot CI/documentation/test change, not runtime/product code. Runtime lock remains8965ffc346d426ccdf919bd18585e4d4a0c21654 (159.0.1). The source work reads immutable pinned versions; no existing product/profile is modified.

## Original evidence and prior work

GitHub2943 and all3 comments were read, along with Jira's5 prior comments10008/10009/10044/10253/10258. Reporter describes Floorp12.20.2@158.0.1, Fedora44/KDE6.7.5, no native-Wayland window, surviving process; XWayland works, fresh profile checked. Both archive and Flatpak reportedly fail. Active graphics in the *working XWayland* support section is Intel UHD TGL GT1, vendor8086/device9a60, mesa/iris26.2.4.0. RTX3070 Laptop exists, but its driver, native-Wayland active device, session kernel, monitor layout and KWin package/backports are not supplied. Do not infer NVIDIA rendering from presence of the discrete GPU.

The original terminal output is reported as **none**. There is no startup trace, blocked stack, crash or measured last successful initialization step. Therefore no original stopping function can honestly be identified. Existing WSLg observations reached compositor configure, native windowProtocol=wayland, llvmpipe and Marionette response on current Floorp; they are not physical-GPU reproductions. They were not rerun. The reporter's Firefox158.0 native-Wayland success is a reported control, not independently observed, and differs from Floorp Gecko158.0.1. Installation/build options of that Firefox remain unknown.

Exact2943 PR search returned0. Broader Wayland search found separate Draft3032/3033 with nested KWin success for PWA/split-view checks; these do not establish a KAN7 fix or matchingGPU coverage. No duplicate patch was made. See original-evidence.json and prior-jira-evidence.json.

## Current measured capability, not an emulated original machine

Debian13 Cloud, Linux6.18.57 x86_64. Actual protected execution context has **no /dev/dri, /sys/class/drm, renderD node, /dev/nvidia0, /dev/nvidiactl or /dev/dxg**. EGLQueryDevicesEXT succeeds and returns exactly1 device with EGL_MESA_device_software and empty render node. Mesa packages25.0.7, libdrm2.4.124, Wayland client1.23.1 differ from the original iris26.2.4. Installed iris/nouveau libraries do not constitute GPU hardware or driver access.

The default session currently has no DISPLAY/WAYLAND_DISPLAY/XDG_RUNTIME_DIR/session-DBus value; no system-bus socket. The old X121 pathname refuses connections (errno111), and no Wayland socket was found in the probed runtime directories. This is session state, **not inability to provision GUI/DBus**: earlier owned Xvfb/private DBus/real GTK portal tests on this same Cloud host succeeded, including the real Thunderbird portal path. Such X11/nested software compositors can be provisioned again but cannot supply the missing Intel/NVIDIA DRM hardware, reported KDE6.7.5 session/compositor policy or Fedora kernel. Installing another Mesa library or using Fedora container userspace cannot remedy that physical condition. No same-condition software startup or Fedora namespace-EPERM test was repeated; no kernel/GPU identity, renderer or protection was changed.

Small numeric/device results and the read-only reproducer are capabilities.json and probe-capabilities.py. Capture-startup.py is a fresh-owned-profile diagnostic for an actual available desktop; it was syntax-checked, **not executed here**, and never fabricates display/GPU. Its optional gdb attach targets only its own QA process, with debuginfod disabled; correct symbols must be obtained for the actual binary before interpreting frames. Full traces stay local for review.

## Narrowed hypotheses and evidence that would distinguish them

Pinned old Runtime7dd619742b25567e479eeda97ae071d10542ad0b and current8965ffc346d426ccdf919bd18585e4d4a0c21654 have the same inspected native-Wayland initialization path. Init creates DMABufFormats, performs wl_display_roundtrip, requests asynchronous roundtrips, waits by wl_display_dispatch until callbacks drain, then ensures DMABuf formats. This gives a **concrete potential wait to locate**, not proof the original process stopped there. A main-thread stack plus the last protocol request/reply distinguishes registry synchronization from later buffer/device initialization and Floorp custom startup. Source excerpts preserve original line numbers, pinned URLs and full-file hashes.

The inspected wrapper Linux workflow, linux64 common-opt/release mozconfigs, nsAppShell and mozwayland implementation are byte-identical old/current; nsWaylandDisplay only adds SHM logging in the acquired diff. Floorp override.ini's observed additions concern sponsored-newtab/workspace sync, with no Wayland/EGL/WebRender entry found. These bounded comparisons **do not prove complete build/source equivalence**: Runtime/API comparisons are capped at300 files and include broad upstream updates. Source-analysis/report.md and hypotheses.json document limits and explicit falsification conditions.

External candidates were screened against primary sources, without promoting a symptom match into diagnosis:

- Debian1144505 documents KWin wl_fixes registry errors/client disconnect and a package fix; KAN7 lacks that protocol error and describes6.7.5 process survival. Both pinned Runtime versions negotiate wl_fixes library/compositor versions and acknowledge global removal in the same inspected path. Only a matching negotiation/global-removal/error trace can connect this candidate.
- Mozilla2076558 is wrong-monitor window placement, documented in158.0.1 notes. A mapped window outside expected coordinates must be distinguished from initialization deadlock; no original map/coordinate trace exists. It is not a confirmed KAN7 fix.
- Mesa26.2.6's Meteor Lake benchmark hang and Mozilla2076178's NVIDIA HDR/video dmabuf crash have different GPU/workload signatures. They cannot establish an iris TGL startup-hang cause or justify a forced driver change.

Archive+Flatpak failure weakens a single-package-only explanation but leaves their common Floorp startup/Runtime and host compositor/GPU paths. Reported Firefox success weakens a browser-independent total desktop failure but leaves version/package/build differences. **Floorp-specific versus external remains unresolved.** No protection-disabling workaround or speculative source change is proposed. Primary links and exclusions are in upstream-screening.json.

## Precise remaining conditions

A protected Fedora44 KDE6.7.5 real Wayland session with Intel TGL9a60/iris26.2.4 and actual hybrid GPU routing; record exact kernel/KWin package and NVIDIA driver without changing them. Obtain an owned fresh-profile stalled-process main/child stack, last Wayland request/reply and mapping state, plus the matched-engine Firefox158.0.1 control and reported158.0/current controls as separate versions/packages. GPU process/device-open evidence is needed before assigning a driver. A desktop portal wait requires its own matching DBus/stack observation; KAN90's successful OpenURI route does not test Wayland startup.

needs-linux is no longer an accurate generic capability gap given the actual Linux/Wayland observations already recorded. Replace generic needs-gpu with needs-intel-tgl-iris-26-2-4 and needs-hybrid-gpu-driver-info, and add needs-startup-stack / needs-matched-firefox while retaining env-blocked, needs-fedora-kde, needs-repro and P1. No Done transition. No new request is sent to the original reporter; the next required evidence is specified for the coordinator's hardware-capable work.

QA evidence published on qa/linux-c-integration; Jira KAN-7 comment10347 posted and precise labels updated. Raw source/download comparisons remain local where large; pinned retrieval URLs/full-file hashes, selected line-numbered excerpts and bounded diffs are exported for review. No source/build equivalence claim beyond the inspected files.
