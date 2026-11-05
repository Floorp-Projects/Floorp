#!/usr/bin/env bash
# Run from this QA artifact directory on a Debian13 Cloud executor.
# Requires inherited HTTPS proxy/CA, cached Xvfb path, and user-extracted deps.
set -euo pipefail
qa90="$(cd "$(dirname "$0")" && pwd)"
# apt.conf is configured entirely under qa90/apt, not system apt state.
/usr/bin/apt-get -c "$qa90/apt/apt.conf" update
/usr/bin/apt-get -c "$qa90/apt/apt.conf" -o Debug::NoLocking=1 --download-only --no-install-recommends -y install flatpak thunderbird xdg-desktop-portal xdg-desktop-portal-gtk openbox
for qa90_deb in "$qa90"/apt/archives/*.deb; do dpkg-deb -x "$qa90_deb" "$qa90/sysroot"; done
"$qa90/flatpak-env.sh" flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
# The worker has no system bus; private DBus provides normal no-policy lookup.
# No parental-control configuration is changed. It is not Fedora's desktop bus.
"$qa90/flatpak-env.sh" dbus-run-session -- sh -c 'export DBUS_SYSTEM_BUS_ADDRESS="$DBUS_SESSION_BUS_ADDRESS"; flatpak install --user -y flathub one.ablaze.floorp'
"$qa90/flatpak-env.sh" dbus-run-session -- /workspace/qa-linux-22/C/web-tools/venv/bin/python -u "$qa90/run-integration.py"
