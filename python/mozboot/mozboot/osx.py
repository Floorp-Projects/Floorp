# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import platform
import subprocess
import sys
import tempfile

try:
    from urllib2 import urlopen
except ImportError:
    from urllib.request import urlopen

from distutils.version import StrictVersion

from mozboot.base import BaseBootstrapper
from mozfile import which

HOMEBREW_BOOTSTRAP = (
    "https://raw.githubusercontent.com/Homebrew/install/master/install.sh"
)

BREW_INSTALL = """
We will install the Homebrew package manager to install required packages.

You will be prompted to install Homebrew with its default settings. If you
would prefer to do this manually, hit CTRL+c, install Homebrew yourself, ensure
"brew" is in your $PATH, and relaunch bootstrap.
"""

BREW_PACKAGES = """
We are now installing all required packages via Homebrew. You will see a lot of
output as packages are built.
"""

NO_BREW_INSTALLED = "It seems you don't have Homebrew installed."

BAD_PATH_ORDER = """
Your environment's PATH variable lists a system path directory (%s)
before the path to your package manager's binaries (%s).
This means that the package manager's binaries likely won't be
detected properly.

Modify your shell's configuration (e.g. ~/.profile or
~/.bash_profile) to have %s appear in $PATH before %s. e.g.

    export PATH=%s:$PATH

Once this is done, start a new shell (likely Command+T) and run
this bootstrap again.
"""


class OSXBootstrapper(BaseBootstrapper):

    INSTALL_PYTHON_GUIDANCE = (
        "See https://firefox-source-docs.mozilla.org/setup/macos_build.html "
        "for guidance on how to prepare your system to build Firefox. Perhaps "
        "you need to update Xcode, or install Python using brew?"
    )

    def __init__(self, version, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.os_version = StrictVersion(version)

        if self.os_version < StrictVersion("10.6"):
            raise Exception("OS X 10.6 or above is required.")

        if platform.machine() == "arm64":
            print(
                "Bootstrap is not supported on Apple Silicon yet.\n"
                "Please see instructions at https://bit.ly/36bUmEx in the meanwhile"
            )
            sys.exit(1)

        self.minor_version = version.split(".")[1]

    def install_system_packages(self):
        self.ensure_command_line_tools()

        self.ensure_homebrew_installed()
        _, hg_modern, _ = self.is_mercurial_modern()
        if not hg_modern:
            print(
                "Mercurial wasn't found or is not sufficiently modern. "
                "It will be installed with brew"
            )

        packages = ["git", "gnu-tar", "terminal-notifier", "watchman"]
        if not hg_modern:
            packages.append("mercurial")
        self._ensure_homebrew_packages(packages)

    def install_browser_packages(self, mozconfig_builder):
        pass

    def install_browser_artifact_mode_packages(self, mozconfig_builder):
        pass

    def install_mobile_android_packages(self, mozconfig_builder):
        self.ensure_homebrew_mobile_android_packages(mozconfig_builder)

    def install_mobile_android_artifact_mode_packages(self, mozconfig_builder):
        self.ensure_homebrew_mobile_android_packages(
            mozconfig_builder, artifact_mode=True
        )

    def generate_mobile_android_mozconfig(self):
        return self._generate_mobile_android_mozconfig()

    def generate_mobile_android_artifact_mode_mozconfig(self):
        return self._generate_mobile_android_mozconfig(artifact_mode=True)

    def _generate_mobile_android_mozconfig(self, artifact_mode=False):
        from mozboot import android

        return android.generate_mozconfig("macosx", artifact_mode=artifact_mode)

    def ensure_command_line_tools(self):
        # We need either the command line tools or Xcode (one is sufficient).
        # Python 3, required to run this code, is not installed by default on macos
        # as of writing (macos <= 11.x).
        # There are at least 5 different ways to obtain it:
        # - macports
        # - homebrew
        # - command line tools
        # - Xcode
        # - python.org
        # The first two require to install the command line tools.
        # So only in the last case we may not have command line tools or xcode
        # available.
        # When the command line tools are installed, `xcode-select --print-path`
        # prints their path.
        # When Xcode is installed, `xcode-select --print-path` prints its path.
        # When neither is installed, `xcode-select --print-path` prints an error
        # to stderr and nothing to stdout.
        # So in the rare case where we detect neither the command line tools or
        # Xcode is installed, we trigger an intall of the command line tools
        # (via `xcode-select --install`).
        proc = subprocess.run(
            ["xcode-select", "--print-path"],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
        if not proc.stdout:
            subprocess.run(["xcode-select", "--install"], check=True)
            # xcode-select --install triggers a separate process to be started by
            # launchd, and tracking its successful outcome would require something
            # like figuring its pid and using kqueue to get a notification when it
            # finishes. Considering how unlikely it is that someone would end up
            # here in the first place, we just bail out.
            print("Please follow the command line tools installer instructions")
            print("and rerun `./mach bootstrap` when it's finished.")
            sys.exit(1)

    def _ensure_homebrew_found(self):
        self.brew = which("brew")

        return self.brew is not None

    def _ensure_homebrew_packages(self, packages, is_for_cask=False):
        package_type_flag = "--cask" if is_for_cask else "--formula"
        self.ensure_homebrew_installed()

        def create_homebrew_cmd(*parameters):
            base_cmd = [self.brew]
            base_cmd.extend(parameters)
            return base_cmd + [package_type_flag]

        installed = set(
            subprocess.check_output(
                create_homebrew_cmd("list"), universal_newlines=True
            ).split()
        )
        outdated = set(
            subprocess.check_output(
                create_homebrew_cmd("outdated", "--quiet"), universal_newlines=True
            ).split()
        )

        to_install = set(package for package in packages if package not in installed)
        to_upgrade = set(package for package in packages if package in outdated)

        if to_install or to_upgrade:
            print(BREW_PACKAGES)
        if to_install:
            subprocess.check_call(create_homebrew_cmd("install") + list(to_install))
        if to_upgrade:
            subprocess.check_call(create_homebrew_cmd("upgrade") + list(to_upgrade))

    def _ensure_homebrew_casks(self, casks):
        self._ensure_homebrew_found()

        known_taps = subprocess.check_output([self.brew, "tap"])

        # Ensure that we can access old versions of packages.
        if b"homebrew/cask-versions" not in known_taps:
            subprocess.check_output([self.brew, "tap", "homebrew/cask-versions"])

        # "caskroom/versions" has been renamed to "homebrew/cask-versions", so
        # it is safe to remove the old tap. Removing the old tap is necessary
        # to avoid the error "Cask [name of cask] exists in multiple taps".
        # See https://bugzilla.mozilla.org/show_bug.cgi?id=1544981
        if b"caskroom/versions" in known_taps:
            subprocess.check_output([self.brew, "untap", "caskroom/versions"])

        self._ensure_homebrew_packages(casks, is_for_cask=True)

    def ensure_homebrew_browser_packages(self):
        # TODO: Figure out what not to install for artifact mode
        packages = ["yasm"]
        self._ensure_homebrew_packages(packages)

    def ensure_homebrew_mobile_android_packages(
        self, mozconfig_builder, artifact_mode=False
    ):
        # Multi-part process:
        # 1. System packages.
        # 2. Android SDK. Android NDK only if we are not in artifact mode. Android packages.

        # 1. System packages.
        packages = ["wget"]
        self._ensure_homebrew_packages(packages)

        casks = ["adoptopenjdk8"]
        self._ensure_homebrew_casks(casks)

        is_64bits = sys.maxsize > 2 ** 32
        if not is_64bits:
            raise Exception(
                "You need a 64-bit version of Mac OS X to build "
                "GeckoView/Firefox for Android."
            )

        # 2. Android pieces.
        java_path = self.ensure_java(mozconfig_builder)
        # Prefer our validated java binary by putting it on the path first.
        os.environ["PATH"] = "{}{}{}".format(java_path, os.pathsep, os.environ["PATH"])
        from mozboot import android

        android.ensure_android(
            "macosx", artifact_mode=artifact_mode, no_interactive=self.no_interactive
        )

    def ensure_homebrew_installed(self):
        """
        Search for Homebrew in sys.path, if not found, prompt the user to install it.
        Then assert our PATH ordering is correct.
        """
        homebrew_found = self._ensure_homebrew_found()
        if not homebrew_found:
            self.install_homebrew()

        # Check for correct $PATH ordering.
        brew_dir = os.path.dirname(self.brew)
        for path in os.environ["PATH"].split(os.pathsep):
            if path == brew_dir:
                break

            for check in ("/bin", "/usr/bin"):
                if path == check:
                    print(BAD_PATH_ORDER % (check, brew_dir, brew_dir, check, brew_dir))
                    sys.exit(1)

    def ensure_clang_static_analysis_package(self, state_dir, checkout_root):
        from mozboot import static_analysis

        self.install_toolchain_static_analysis(
            state_dir, checkout_root, static_analysis.MACOS_CLANG_TIDY
        )

    def ensure_sccache_packages(self, state_dir, checkout_root):
        from mozboot import sccache

        self.install_toolchain_artifact(state_dir, checkout_root, sccache.MACOS_SCCACHE)
        self.install_toolchain_artifact(
            state_dir, checkout_root, sccache.RUSTC_DIST_TOOLCHAIN, no_unpack=True
        )
        self.install_toolchain_artifact(
            state_dir, checkout_root, sccache.CLANG_DIST_TOOLCHAIN, no_unpack=True
        )

    def ensure_fix_stacks_packages(self, state_dir, checkout_root):
        from mozboot import fix_stacks

        self.install_toolchain_artifact(
            state_dir, checkout_root, fix_stacks.MACOS_FIX_STACKS
        )

    def ensure_stylo_packages(self, state_dir, checkout_root):
        from mozboot import stylo

        self.install_toolchain_artifact(state_dir, checkout_root, stylo.MACOS_CLANG)
        self.install_toolchain_artifact(state_dir, checkout_root, stylo.MACOS_CBINDGEN)

    def ensure_nasm_packages(self, state_dir, checkout_root):
        from mozboot import nasm

        self.install_toolchain_artifact(state_dir, checkout_root, nasm.MACOS_NASM)

    def ensure_node_packages(self, state_dir, checkout_root):
        # XXX from necessary?
        from mozboot import node

        self.install_toolchain_artifact(state_dir, checkout_root, node.OSX)

    def ensure_minidump_stackwalk_packages(self, state_dir, checkout_root):
        from mozboot import minidump_stackwalk

        self.install_toolchain_artifact(
            state_dir, checkout_root, minidump_stackwalk.MACOS_MINIDUMP_STACKWALK
        )

    def ensure_dump_syms_packages(self, state_dir, checkout_root):
        from mozboot import dump_syms

        self.install_toolchain_artifact(
            state_dir, checkout_root, dump_syms.MACOS_DUMP_SYMS
        )

    def install_homebrew(self):
        print(BREW_INSTALL)
        bootstrap = urlopen(url=HOMEBREW_BOOTSTRAP, timeout=20).read()
        with tempfile.NamedTemporaryFile() as tf:
            tf.write(bootstrap)
            tf.flush()

            subprocess.check_call(["bash", tf.name])

        homebrew_found = self._ensure_homebrew_found()
        if not homebrew_found:
            print(
                "Homebrew was just installed but can't be found on PATH. "
                "Please file a bug."
            )
            sys.exit(1)

    def _update_package_manager(self):
        subprocess.check_call([self.brew, "-v", "update"])

    def _upgrade_package(self, package):
        self._ensure_homebrew_installed()

        try:
            subprocess.check_output(
                [self.brew, "-v", "upgrade", package], stderr=subprocess.STDOUT
            )
        except subprocess.CalledProcessError as e:
            if b"already installed" not in e.output:
                raise

    def upgrade_mercurial(self, current):
        self._upgrade_package("mercurial")
