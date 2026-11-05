# KAN-71 / GitHub #2535: xdg-open focus

Source main `984509d2053be7bae9027133bda7259cd706b208`; independent branch `qa/linux-c-integration`. Latest issue and comments read. Issue-number and xdg-open PR searches found no directly established fix; #2852 concerns PWA desktop launchers, not this ordinary-window focus contract.

## Executed product tests

Official Floorp12.21.1 / Gecko159.0.1 build20261103153232; official Firefox159.0.1 build20261102144711. Debian13.6, isolated HOME/XDG/profile, real session DBus, Xvfb X11. XFWM4.20.0 and standalone Muffin6.4.1 were actually launched. Muffin dependencies and schemas were installed in a user-space sysroot, including the missing Cinnamon settings schema identified by an initial startup failure. This is not Linux Mint or a complete Cinnamon session, and no KDE/Wayland claim is made.

Command: `dbus-run-session -- /workspace/qa-linux-22/C/web-tools/venv/bin/python /workspace/qa-linux-22/C/KAN-71/run_focus.py` in the authorized direct execution context. Do not source the old saved env.sh. Dependencies: official products, marionette_driver, Xvfb/xdotool, xterm, GIO, XFWM/Muffin; the script names their isolated paths explicitly.

Both products were already running. An isolated desktop entry and MIME association dispatched `xdg-open` to that exact profile. The source terminal was activated before each test. Two modes per product/WM: shell invocation and a command typed into the terminal followed by a synthetic Return. The local fixture server recorded each unique HTTP marker, the original browser process stayed alive, and actual `_NET_ACTIVE_WINDOW` plus browser window IDs were measured after dispatch. The screenshots show the source terminal still in front of the successfully loaded browser tab.

**8/8 URL dispatches reached the HTTP fixture; 0/8 brought the browser in front.** Both Floorp and Firefox behaved alike on both window managers. No front-focus success is claimed from URL receipt. `results.json` contains the per-trial observations; per-product directories contain window properties, screenshots and logs. Original browser sandbox level remained6. TLS checks and product protection preferences were not disabled. Marionette's required system-inspection opt-in was used.

## Interpretation and remaining conditions

Classification: symptom reproduced in a substitute environment; **Floorp-specific defect remains unconfirmed**. The original claim that Firefox raises while Floorp does not has not been reproduced. The launcher recorded neither inherited DESKTOP_STARTUP_ID nor XDG_ACTIVATION_TOKEN. These were X11 tests; no Wayland activation-token implementation was exercised. Muffin logs include timestamp0 activation warnings from synthetic activation of the source terminal. Synthetic input is not physical input and is insufficient to establish the original trusted-user activation path.

Remaining: Linux Mint/Cinnamon X11 with an actual initiating application's startup timestamp/context; a matching Firefox control demonstrating the reported difference; physical-input path if that changes focus policy. Retain the unresolved environment condition, narrowed from generic Linux absence to `needs-cinnamon-x11-activation`. Do not call this fixed, ordinary unreproduced, or an upstream bug solely from this comparison. No code patch/Draft PR is justified yet.

Earlier harness runs (missing inspection opt-in, desktop helper dispatch, missing Muffin schema) are retained locally as setup evidence. Their results are not product tests and are excluded from the final 8-trial count.
