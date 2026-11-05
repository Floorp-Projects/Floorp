# Environment evidence
Debian GNU/Linux 13.6 x86_64, uid1000. Base source 984509d2053be7bae9027133bda7259cd706b208 checked via live GitHub and fetch. Separate worktrees; original checkout unchanged.

Default tool execution restricts user namespace and Docker socket. The authorized outer execution path successfully runs `unshare -Ur true`, Docker 28.4.0 and private DBus. These are observed differences between execution paths, not product defects. No need to disable browser content sandbox. The stale `.floorp-cloud/env.sh` sets MOZ_DISABLE_CONTENT_SANDBOX, so it was deliberately not sourced; runners assert that variable is absent.

Xvfb 1440x1100 and OS XTest operate. KWin 6.3.6 with --x11-display :92 supplies nested Wayland. Firefox Troubleshoot reports windowProtocol=wayland and Software WebRender (visual-29295.json); X11 wheel run reports x11. KWin is not KDE Neon or a complete Plasma session. /dev/dri, /dev/input and /dev/uinput are absent: no physical input/GPU evidence.

45 Debian packages were downloaded into the task sysroot, including KWin, Weston, Flatpak1.16.6, xdg-desktop-portal and KDE backend. User-local package extraction does not install system DBus service definitions. Flatpak signature checking remained enabled; Flathub runtimes download, but Floorp application pull fails while querying parental-control details because system DBus is unavailable (flatpak-verbose.log). No Flatpak product run was claimed. Portal-backed product flow remains untested. Docker is available but original Fedora/CachyOS/Manjaro/Arch images were not run.

Floorp tarball SHA256 7dcedcbfef747819153b0b9c541a9c1f4f4965b0720f16d5d237d39b0c30d613 matches official release hashes.txt. Firefox archive SHA256 469ce17120018e1c95411d254cf3c5f8db2dd1768a7acde5a2a089b00072ac65 (computed locally; upstream hash not independently compared). TLS verification remained enabled. No secret environment values were printed or saved.
