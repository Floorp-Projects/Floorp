#!/bin/bash
set -euo pipefail
dnf -y --setopt=install_weak_deps=False --setopt=sslcacert=/run/host-ca.pem install gtk3 dbus-x11 libXtst libX11 libX11-xcb libXt dbus-glib mesa-libEGL mesa-libGL alsa-lib python3-pip util-linux shadow-utils
SSL_CERT_FILE=/run/host-ca.pem PIP_CERT=/run/host-ca.pem python3 -m pip install --target /task/fedora-python marionette_driver==3.7.1
useradd -u 1000 -d /task/runtime/fedora-home qa
install -d -o 1000 -g 1000 /task/runtime/fedora-home
cat /etc/os-release
runuser -u qa -- unshare -Ur true || printf "namespace probe failed; attempting product with default protections\n"
runuser -u qa -- env QA_PYTHONPATH=/task/fedora-python PYTHONPATH=/task/fedora-python python3 -u /task/run.py floorp 29410 search middle wheel
