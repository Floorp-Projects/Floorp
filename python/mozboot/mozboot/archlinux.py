# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import sys
import tempfile
import subprocess

from mozboot.base import BaseBootstrapper
from mozboot.linux_common import LinuxBootstrapper

# NOTE: This script is intended to be run with a vanilla Python install.  We
# have to rely on the standard library instead of Python 2+3 helpers like
# the six module.
if sys.version_info < (3,):
    input = raw_input  # noqa


AUR_URL_TEMPLATE = "https://aur.archlinux.org/cgit/aur.git/snapshot/{}.tar.gz"


class ArchlinuxBootstrapper(LinuxBootstrapper, BaseBootstrapper):
    """Archlinux experimental bootstrapper."""

    SYSTEM_PACKAGES = ["base-devel", "unzip", "zip"]

    BROWSER_PACKAGES = [
        "alsa-lib",
        "dbus-glib",
        "gtk3",
        "libevent",
        "libvpx",
        "libxt",
        "mime-types",
        "startup-notification",
        "gst-plugins-base-libs",
        "libpulse",
        "xorg-server-xvfb",
        "gst-libav",
        "gst-plugins-good",
    ]

    BROWSER_AUR_PACKAGES = [
        "uuid",
    ]

    MOBILE_ANDROID_COMMON_PACKAGES = [
        # See comment about 32 bit binaries and multilib below.
        "multilib/lib32-ncurses",
        "multilib/lib32-readline",
        "multilib/lib32-zlib",
    ]

    def __init__(self, version, dist_id, **kwargs):
        print("Using an experimental bootstrapper for Archlinux.", file=sys.stderr)
        BaseBootstrapper.__init__(self, **kwargs)

    def install_system_packages(self):
        self.pacman_install(*self.SYSTEM_PACKAGES)

    def install_browser_packages(self, mozconfig_builder, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        self.aur_install(*self.BROWSER_AUR_PACKAGES)
        self.pacman_install(*self.BROWSER_PACKAGES)

    def install_browser_artifact_mode_packages(self, mozconfig_builder):
        self.install_browser_packages(mozconfig_builder, artifact_mode=True)

    def install_mobile_android_packages(self, mozconfig_builder, artifact_mode=False):
        # Multi-part process:
        # 1. System packages.
        # 2. Android SDK. Android NDK only if we are not in artifact mode. Android packages.

        # 1. This is hard to believe, but the Android SDK binaries are 32-bit
        # and that conflicts with 64-bit Arch installations out of the box.  The
        # solution is to add the multilibs repository; unfortunately, this
        # requires manual intervention.
        try:
            self.pacman_install(*self.MOBILE_ANDROID_COMMON_PACKAGES)
        except Exception as e:
            print(
                "Failed to install all packages.  The Android developer "
                "toolchain requires 32 bit binaries be enabled (see "
                "https://wiki.archlinux.org/index.php/Android).  You may need to "
                "manually enable the multilib repository following the instructions "
                "at https://wiki.archlinux.org/index.php/Multilib.",
                file=sys.stderr,
            )
            raise e

        # 2. Android pieces.
        super().install_mobile_android_packages(
            mozconfig_builder, artifact_mode=artifact_mode
        )

    def upgrade_mercurial(self, current):
        self.pacman_install("mercurial")

    def pacman_is_installed(self, package):
        command = ["pacman", "-Q", package]
        return (
            subprocess.run(
                command,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            ).returncode
            == 0
        )

    def pacman_install(self, *packages):
        command = ["pacman", "-S", "--needed"]
        if self.no_interactive:
            command.append("--noconfirm")

        command.extend(packages)

        self.run_as_root(command)

    def run(self, command, env=None):
        subprocess.check_call(command, stdin=sys.stdin, env=env)

    def download(self, uri):
        command = ["curl", "-L", "-O", uri]
        self.run(command)

    def unpack(self, path, name, ext):
        if ext == ".gz":
            compression = "-z"
        else:
            print(f"unsupported compression extension: {ext}", file=sys.stderr)
            sys.exit(1)

        name = os.path.join(path, name) + ".tar" + ext
        command = ["tar", "-x", compression, "-f", name, "-C", path]
        self.run(command)

    def makepkg(self, name):
        command = ["makepkg", "-sri"]
        if self.no_interactive:
            command.append("--noconfirm")
        makepkg_env = os.environ.copy()
        makepkg_env["PKGDEST"] = "."
        self.run(command, env=makepkg_env)

    def aur_install(self, *packages):
        needed = []

        for package in packages:
            if self.pacman_is_installed(package):
                print(
                    f"warning: AUR package {package} is installed -- skipping",
                    file=sys.stderr,
                )
            else:
                needed.append(package)

        # all required AUR packages are already installed!
        if not needed:
            return

        path = tempfile.mkdtemp(prefix="mozboot-")
        if not self.no_interactive:
            print(
                "WARNING! This script requires to install packages from the AUR "
                "This is potentially insecure so I recommend that you carefully "
                "read each package description and check the sources."
                f"These packages will be built in {path}: " + ", ".join(needed),
                file=sys.stderr,
            )
            choice = input("Do you want to continue? (yes/no) [no]")
            if choice != "yes":
                sys.exit(1)

        base_dir = os.getcwd()
        os.chdir(path)
        for name in needed:
            url = AUR_URL_TEMPLATE.format(package)
            ext = os.path.splitext(url)[-1]
            directory = os.path.join(path, name)
            self.download(url)
            self.unpack(path, name, ext)
            os.chdir(directory)
            self.makepkg(name)

        os.chdir(base_dir)
