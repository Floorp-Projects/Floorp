# Passive startup without Marionette NewSession

One bounded launch of exact PR2877 head f43 full package, copied previously signed Arabic profile, X11/Openbox display127 and port30078. `--marionette` remained; the script made **no socket/TCP connections or RPC calls**. Only the copied profile path and Marionette port changed.

At **54.66s from monitor baseline**, owned known PSS reached **12.273GiB** and cgroup memory **13.918GiB**, triggering the existing 12GiB PSS guard before the requested60s completed. Cgroup OOM/kill event deltas were zero. The15s screenshot visibly contains Arabic chrome labels; strict locale/RTL_UI APIs were **not evaluated**, no layout states or hover trials collected. **KAN29Pass=false**.

OS Ctrl+Q command exited0, but direct native child PID115562 did not exit within12s. Dedicated product group then received SIGTERM; direct child exit **-15**, so normal quit is **not established**. Full product **46 entries** remained unchanged. Executable/libxul hashes and ended-profile cleanup receipt retained.

This observation shows memory growth can occur without NewSession in this recorded setup. It does not establish a product root cause or strict causal independence: the source profile contains earlier resource-stop history and is not a clean pre-RTL snapshot. CLI automation flag remained. Renderer/protection preferences were not changed; active renderer identity was not queried under the no-RPC rule. No new source/build/lock changes or further launch.
