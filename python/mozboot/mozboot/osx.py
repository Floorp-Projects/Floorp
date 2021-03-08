# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import platform
import re
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

HOMEBREW_BOOTSTRAP = "https://raw.githubusercontent.com/Homebrew/install/master/install"
XCODE_APP_STORE = "macappstore://itunes.apple.com/app/id497799835?mt=12"
XCODE_LEGACY = (
    "https://developer.apple.com/downloads/download.action?path=Developer_Tools/"
    "xcode_3.2.6_and_ios_sdk_4.3__final/xcode_3.2.6_and_ios_sdk_4.3.dmg"
)

RE_CLANG_VERSION = re.compile("Apple (?:clang|LLVM) version (\d+\.\d+)")

APPLE_CLANG_MINIMUM_VERSION = StrictVersion("4.2")

XCODE_REQUIRED = """
Xcode is required to build Firefox. Please complete the install of Xcode
through the App Store.

It's possible Xcode is already installed on this machine but it isn't being
detected. This is possible with developer preview releases of Xcode, for
example. To correct this problem, run:

  `xcode-select --switch /path/to/Xcode.app`.

e.g. `sudo xcode-select --switch /Applications/Xcode.app`.
"""

XCODE_REQUIRED_LEGACY = """
You will need to download and install Xcode to build Firefox.

Please complete the Xcode download and then relaunch this script.
"""

XCODE_NO_DEVELOPER_DIRECTORY = """
xcode-select says you don't have a developer directory configured. We think
this is due to you not having Xcode installed (properly). We're going to
attempt to install Xcode through the App Store. If the App Store thinks you
have Xcode installed, please run xcode-select by hand until it stops
complaining and then re-run this script.
"""

XCODE_COMMAND_LINE_TOOLS_MISSING = """
The Xcode command line tools are required to build Firefox.
"""

INSTALL_XCODE_COMMAND_LINE_TOOLS_STEPS = """
Perform the following steps to install the Xcode command line tools:

    1) Open Xcode.app
    2) Click through any first-run prompts
    3) From the main Xcode menu, select Preferences (Command ,)
    4) Go to the Download tab (near the right)
    5) Install the "Command Line Tools"

When that has finished installing, please relaunch this script.
"""

UPGRADE_XCODE_COMMAND_LINE_TOOLS = """
An old version of the Xcode command line tools is installed. You will need to
install a newer version in order to compile Firefox. If Xcode itself is old,
its command line tools may be too old even if it claims there are no updates
available, so if you are seeing this message multiple times, please update
Xcode first.
"""

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
        self.ensure_xcode()

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

    def ensure_xcode(self):
        if self.os_version < StrictVersion("10.7"):
            if not os.path.exists("/Developer/Applications/Xcode.app"):
                print(XCODE_REQUIRED_LEGACY)

                subprocess.check_call(["open", XCODE_LEGACY])
                sys.exit(1)

        # OS X 10.7 have Xcode come from the app store. However, users can
        # still install Xcode into any arbitrary location. We honor the
        # location of Xcode as set by xcode-select. This should also pick up
        # developer preview releases of Xcode, which can be installed into
        # paths like /Applications/Xcode5-DP6.app.
        elif self.os_version >= StrictVersion("10.7"):
            select = which("xcode-select")
            try:
                output = subprocess.check_output(
                    [select, "--print-path"], stderr=subprocess.STDOUT
                )
            except subprocess.CalledProcessError as e:
                # This seems to appear on fresh OS X machines before any Xcode
                # has been installed. It may only occur on OS X 10.9 and later.
                if b"unable to get active developer directory" in e.output:
                    print(XCODE_NO_DEVELOPER_DIRECTORY)
                    self._install_xcode_app_store()
                    assert False  # Above should exit.

                output = e.output

            # This isn't the most robust check in the world. It relies on the
            # default value not being in an application bundle, which seems to
            # hold on at least Mavericks.
            if b".app/" not in output:
                print(XCODE_REQUIRED)
                self._install_xcode_app_store()
                assert False  # Above should exit.

        # Once Xcode is installed, you need to agree to the license before you can
        # use it.
        try:
            output = subprocess.check_output(
                ["/usr/bin/xcrun", "clang"], stderr=subprocess.STDOUT
            )
        except subprocess.CalledProcessError as e:
            if b"license" in e.output:
                xcodebuild = which("xcodebuild")
                try:
                    subprocess.check_output(
                        [xcodebuild, "-license"], stderr=subprocess.STDOUT
                    )
                except subprocess.CalledProcessError as e:
                    if b"requires admin privileges" in e.output:
                        self.run_as_root([xcodebuild, "-license"])

        # Even then we're not done! We need to install the Xcode command line tools.
        # As of Mountain Lion, apparently the only way to do this is to go through a
        # menu dialog inside Xcode itself. We're not making this up.
        if self.os_version >= StrictVersion("10.7"):
            if not os.path.exists("/usr/bin/clang"):
                print(XCODE_COMMAND_LINE_TOOLS_MISSING)
                print(INSTALL_XCODE_COMMAND_LINE_TOOLS_STEPS)
                sys.exit(1)

            output = subprocess.check_output(
                ["/usr/bin/clang", "--version"], universal_newlines=True
            )
            match = RE_CLANG_VERSION.search(output)
            if match is None:
                raise Exception("Could not determine Clang version.")

            version = StrictVersion(match.group(1))

            if version < APPLE_CLANG_MINIMUM_VERSION:
                print(UPGRADE_XCODE_COMMAND_LINE_TOOLS)
                print(INSTALL_XCODE_COMMAND_LINE_TOOLS_STEPS)
                sys.exit(1)

    def _install_xcode_app_store(self):
        subprocess.check_call(["open", XCODE_APP_STORE])
        print("Once the install has finished, please relaunch this script.")
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

            subprocess.check_call(["ruby", tf.name])

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
