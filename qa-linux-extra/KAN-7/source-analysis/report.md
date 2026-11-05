# KAN-7 pinned-source analysis

Read-only analysis; new product/GPU trials: **0**. Original report: Floorp12.20.2@158.0.1 hangs on Fedora44/KDE6.7.5 native Wayland in archive and Flatpak; XWayland works. No original stack, protocol trace or terminal output locates the stop. No root cause or fix is established.

## Provenance

Public v12.20.2 release published2026-10-17T02:43:43Z, not draft. linux-meta and release-manifest identify Floorp576bc4bbca058e4c24dc5bd2c7526d64cedc94cc and Runtime7dd619742b25567e479eeda97ae071d10542ad0b (daily1155), Gecko158.0.1/build20261014104253. Current requested Floorp29a3eb23145da90706d8a94441373278521cc737 pins Runtime8965ffc346d426ccdf919bd18585e4d4a0c21654 (daily1179), Gecko159.0.1/build20261103153232. Exact artifact hashes are retained in summary.json and locks. Effective running product metadata must still be measured by trial owner; this analysis did not execute products.

## Relevant bounded differences

The acquired Linux wrapper-build-linux workflow, linux64 common-opt/release mozconfigs, nsAppShell.cpp and mozwayland.c are byte-identical across the pinned Runtime commits. These files do not establish identical complete compiler/build configurations: wrapper calls common-build, which was not exhaustively traced. nsWaylandDisplay.cpp differs only by additional SHM-format logging listener in SetShm. The initialization/wait path cited below is unchanged. Floorp override.ini adds sponsored-newtab disabling and workspace-sync defaults; it contains no Wayland/EGL/WebRender override found by search. StaticPrefList full diff is saved; graphics changes include a remote-canvas DMABuf video-frame fast-path preference and WebRender changes; none establishes a startup-hang fix. Runtime comparison includes upstream158.0.2,159.0,159.0.1 synchronization; its300-file API result, like Floorp comparison, is capped and cannot rule out other relevant changes.

## Concrete candidates and falsification

Pinned old nsWaylandDisplay::Init (line1328) creates DMABufFormats, registers registry listener, calls wl_display_roundtrip (1338), then RequestAsyncRoundtrip/WaitForAsyncRoundtrips (1339-1340), followed by EnsureDMABufFormats (1341). WaitForAsyncRoundtrips (979) loops wl_display_dispatch until callbacks drain. This is a real native-Wayland synchronous path worth locating with a main-thread stack and protocol trace; it is **not evidence the reported hang occurred there**. If initialization completes before the stop, this candidate is rejected. GPU/buffer initialization is another measurable candidate, not a diagnosis. hypotheses.json records falsification conditions.

Firefox158.0 reported working versus Floorp Gecko158.0.1 leaves an engine-patch difference as an uncontrolled factor, alongside Floorp-specific startup/build integration. Original NVIDIA driver/version, active rendering device, compositor protocol sequence and stacks remain unknown. Parent capability measurement found only software EGL and no DRM/NVIDIA/dxg device; this source task cannot resolve physical-GPU conditions. No protection-disabling proposal or GPU-cause assertion is made. PR3032/3033 concern separate nested-Wayland issues and are not treated as canonical fixes for KAN-7.

Sources: retrieval-manifest.json, runtime-source-retrieval.json and runtime-source-extra-retrieval.json contain pinned public URLs and SHA256. Missing-file404s are recorded as acquisition misses only. No existing checkout/profile was modified, no network authentication was performed, and no external write/publication occurred.

## wl_fixes follow-up

Both pinned nsWaylandDisplay.cpp versions dynamically resolve wl_fixes_interface from libwayland-client, cap negotiated version by both library and compositor versions (old909-924/current932-947), and acknowledge registry global removal with wl_fixes_ack_global_remove when negotiated version supports it (old939-943/current962-966). This path is unchanged between these versions. No wl_registry_destroy call appears in this file; this does not exclude calls elsewhere. Parent supplied Debian1144505/KWin6.7.3 wl_fixes disconnect as an external candidate; the original6.7.5 silent-hang report lacks matching protocol error, so equivalence is unproven. A protocol trace showing wl_fixes negotiation, global removal, ack and compositor error is necessary to evaluate this candidate; absence of negotiation or successful acknowledgement without disconnect weakens it. Source excerpts saved in wl-fixes-excerpts.txt.
