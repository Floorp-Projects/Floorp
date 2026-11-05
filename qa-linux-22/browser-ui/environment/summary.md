# Environment evidence
Debian GNU/Linux 13.6 x86_64, uid1000. Base source 984509d2053be7bae9027133bda7259cd706b208 checked via live GitHub and fetch. Separate worktrees; original checkout unchanged.

Default tool execution restricts user namespace and Docker socket. The authorized outer execution path successfully runs `unshare -Ur true`, Docker 28.4.0 and private DBus. These are observed differences between execution paths, not product defects. No need to disable browser content sandbox. The stale `.floorp-cloud/env.sh` sets MOZ_DISABLE_CONTENT_SANDBOX, so it was deliberately not sourced; runners assert that variable is absent.

Xvfb 1440x1100 and OS XTest operate. KWin 6.3.6 with --x11-display :92 supplies nested Wayland. Firefox Troubleshoot reports windowProtocol=wayland and Software WebRender (visual-29295.json); X11 wheel run reports x11. KWin is not KDE Neon or a complete Plasma session. /dev/dri, /dev/input and /dev/uinput are absent: no physical input/GPU evidence.

45 Debian packages were downloaded into the task sysroot, including KWin, Weston, Flatpak1.16.6, xdg-desktop-portal and KDE backend. User-local package extraction does not install system DBus service definitions. Flatpak signature checking remained enabled; Flathub runtimes download, but Floorp application pull fails while querying parental-control details because system DBus is unavailable (flatpak-verbose.log). No Flatpak product run was claimed. Portal-backed product flow remains untested. Docker runs an actual Fedora44 Container Image userland using the host Xvfb and shared host kernel. Floorp search/middle/wheel and split/detach were exercised there. This is not the reporter's full Fedora desktop or Flatpak. CachyOS/Manjaro/Arch images were not run.

Floorp tarball SHA256 7dcedcbfef747819153b0b9c541a9c1f4f4965b0720f16d5d237d39b0c30d613 matches official release hashes.txt. Firefox archive SHA256 469ce17120018e1c95411d254cf3c5f8db2dd1768a7acde5a2a089b00072ac65 (computed locally; upstream hash not independently compared). TLS verification remained enabled. No secret environment values were printed or saved.

Fedora44: initial missing libX11-xcb caused XPCOM load exit255; installing libX11-xcb/libXt/dbus-glib/Mesa resolved it. User namespace probe inside Docker returns EPERM, while the product starts with default protection settings and no sandbox-disabling environment. Precondition failure and actual successful product executions are retained separately. Repro material: fedora44.sh plus environment/fedora44-second.log and fedora44-more.log.

## Follow-up: genuine localized RTL and normal signed theme

Official Gecko159.0.1 signed Arabic pack installed through normal AddonManager path, normal app quit exit0 before restart. Final runs29665/29666: appLocale ar, isAppLocaleRTL true, RTL_UI true, root computed direction rtl, actual native Wayland. Root-dir fixture is prohibited for these two runs. Candidate is an independent official-product copy with only existingPR2877's offset computation transplanted; exact ZIP/nativebinary hashes are in KAN-29/pr2877/product-patch.json. Earlier failed localization/connection preconditions remain separate.

KAN87 extra normal signed Galaxy1.0 AMO theme (signedState2, temporaryfalse) ran on separate nativeWayland socket29701. No browser signature/security preference changes. See qaLinuxKAN87/signed/report.md.

## Full production candidate assembly follow-up

PR3033 and combined3031+3034 use independent full production asset builds plus Runtime8965/daily-1179 nativeartifact159.0.1, verified digestcb9b..., matching officialRuntime build20261103153232. Existing before-mach/configure/artifactbuild/after-mach succeeded. This is not native Gecko compilation. Signature-disabling preference override omitted; actual candidates require signatures, combined content sandbox6. Existing shared checkout content/branches remain unchanged. Product binaries are not published.

Strict machpackage remains failed: matching XPT auxiliary input unavailable in both the release inventory and verified same-source CIartifact13272530069. No strict pass claimed. Full production assembled product regressions pass;3033also has separately labelled optionalaux archive product evidence. See full-product/build-route.md and per-candidate reports.
