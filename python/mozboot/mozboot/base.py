# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import hashlib
import os
import platform
import re
import subprocess
import sys

from distutils.version import LooseVersion
from mozboot import rust
from mozboot.util import (
    get_mach_virtualenv_binary,
    locate_java_bin_path,
    MINIMUM_RUST_VERSION,
)
from mozfile import which

# NOTE: This script is intended to be run with a vanilla Python install.  We
# have to rely on the standard library instead of Python 2+3 helpers like
# the six module.
if sys.version_info < (3,):
    from urllib2 import urlopen

    input = raw_input  # noqa
else:
    from urllib.request import urlopen


NO_MERCURIAL = """
Could not find Mercurial (hg) in the current shell's path. Try starting a new
shell and running the bootstrapper again.
"""

MERCURIAL_UNABLE_UPGRADE = """
You are currently running Mercurial %s. Running %s or newer is
recommended for performance and stability reasons.

Unfortunately, this bootstrapper currently does not know how to automatically
upgrade Mercurial on your machine.

You can usually install Mercurial through your package manager or by
downloading a package from http://mercurial.selenic.com/.
"""

MERCURIAL_UPGRADE_FAILED = """
We attempted to upgrade Mercurial to a modern version (%s or newer).
However, you appear to have version %s still.

It's possible your package manager doesn't support a modern version of
Mercurial. It's also possible Mercurial is not being installed in the search
path for this shell. Try creating a new shell and run this bootstrapper again.

If it continues to fail, consider installing Mercurial by following the
instructions at http://mercurial.selenic.com/.
"""

PYTHON_UNABLE_UPGRADE = """
You are currently running Python %s. Running %s or newer (but
not 3.x) is required.

Unfortunately, this bootstrapper does not currently know how to automatically
upgrade Python on your machine.

Please search the Internet for how to upgrade your Python and try running this
bootstrapper again to ensure your machine is up to date.
"""

RUST_INSTALL_COMPLETE = """
Rust installation complete. You should now have rustc and cargo
in %(cargo_bin)s

The installer tries to add these to your default shell PATH, so
restarting your shell and running this script again may work.
If it doesn't, you'll need to add the new command location
manually.

If restarting doesn't work, edit your shell initialization
script, which may be called ~/.bashrc or ~/.bash_profile or
~/.profile, and add the following line:

    %(cmd)s

Then restart your shell and run the bootstrap script again.
"""

RUST_NOT_IN_PATH = """
You have some rust files in %(cargo_bin)s
but they're not part of this shell's PATH.

To add these to the PATH, edit your shell initialization
script, which may be called ~/.bashrc or ~/.bash_profile or
~/.profile, and add the following line:

    %(cmd)s

Then restart your shell and run the bootstrap script again.
"""

RUSTUP_OLD = """
We found an executable called `rustup` which we normally use to install
and upgrade Rust programming language support, but we didn't understand
its output. It may be an old version, or not be the installer from
https://rustup.rs/

Please move it out of the way and run the bootstrap script again.
Or if you prefer and know how, use the current rustup to install
a compatible version of the Rust programming language yourself.
"""

RUST_UPGRADE_FAILED = """
We attempted to upgrade Rust to a modern version (%s or newer).
However, you appear to still have version %s.

It's possible rustup failed. It's also possible the new Rust is not being
installed in the search path for this shell. Try creating a new shell and
run this bootstrapper again.

If this continues to fail and you are sure you have a modern Rust on your
system, ensure it is on the $PATH and try again. If that fails, you'll need to
install Rust manually.

We recommend the installer from https://rustup.rs/ for installing Rust,
but you may be able to get a recent enough version from a software install
tool or package manager on your system, or directly from https://rust-lang.org/
"""

BROWSER_ARTIFACT_MODE_MOZCONFIG = """
# Automatically download and use compiled C++ components:
ac_add_options --enable-artifact-builds
""".strip()

# Upgrade Mercurial older than this.
# This should match the OLDEST_NON_LEGACY_VERSION in
# version-control-tools/hgext/configwizard/__init__.py.
MODERN_MERCURIAL_VERSION = LooseVersion("4.9")

MODERN_PYTHON2_VERSION = LooseVersion("2.7.3")
MODERN_PYTHON3_VERSION = LooseVersion("3.6.0")

# Upgrade rust older than this.
MODERN_RUST_VERSION = LooseVersion(MINIMUM_RUST_VERSION)

# Upgrade nasm older than this.
MODERN_NASM_VERSION = LooseVersion("2.14")


class BaseBootstrapper(object):
    """Base class for system bootstrappers."""

    INSTALL_PYTHON_GUIDANCE = (
        "We do not have specific instructions for your platform on how to "
        "install Python. You may find Pyenv (https://github.com/pyenv/pyenv) "
        "helpful, if your system package manager does not provide a way to "
        "install a recent enough Python 3 and 2."
    )

    def __init__(self, no_interactive=False, no_system_changes=False):
        self.package_manager_updated = False
        self.no_interactive = no_interactive
        self.no_system_changes = no_system_changes
        self.state_dir = None

    def validate_environment(self, srcdir):
        """
        Called once the current firefox checkout has been detected.
        Platform-specific implementations should check the environment and offer advice/warnings
        to the user, if necessary.
        """

    def suggest_install_distutils(self):
        """Called if distutils.{sysconfig,spawn} can't be imported."""
        print(
            "Does your distro require installing another package for distutils?",
            file=sys.stderr,
        )

    def suggest_install_pip3(self):
        """Called if pip3 can't be found."""
        print(
            "Try installing pip3 with your system's package manager.", file=sys.stderr
        )

    def install_system_packages(self):
        """
        Install packages shared by all applications. These are usually
        packages required by the development (like mercurial) or the
        build system (like autoconf).
        """
        raise NotImplementedError(
            "%s must implement install_system_packages()" % __name__
        )

    def install_browser_packages(self, mozconfig_builder):
        """
        Install packages required to build Firefox for Desktop (application
        'browser').
        """
        raise NotImplementedError(
            "Cannot bootstrap Firefox for Desktop: "
            "%s does not yet implement install_browser_packages()" % __name__
        )

    def generate_browser_mozconfig(self):
        """
        Print a message to the console detailing what the user's mozconfig
        should contain.

        Firefox for Desktop can in simple cases determine its build environment
        entirely from configure.
        """
        pass

    def install_browser_artifact_mode_packages(self, mozconfig_builder):
        """
        Install packages required to build Firefox for Desktop (application
        'browser') in Artifact Mode.
        """
        raise NotImplementedError(
            "Cannot bootstrap Firefox for Desktop Artifact Mode: "
            "%s does not yet implement install_browser_artifact_mode_packages()"
            % __name__
        )

    def generate_browser_artifact_mode_mozconfig(self):
        """
        Print a message to the console detailing what the user's mozconfig
        should contain.

        Firefox for Desktop Artifact Mode needs to enable artifact builds and
        a path where the build artifacts will be written to.
        """
        return BROWSER_ARTIFACT_MODE_MOZCONFIG

    def install_mobile_android_packages(self, mozconfig_builder):
        """
        Install packages required to build Firefox for Android (application
        'mobile/android', also known as Fennec).
        """
        raise NotImplementedError(
            "Cannot bootstrap GeckoView/Firefox for Android: "
            "%s does not yet implement install_mobile_android_packages()" % __name__
        )

    def generate_mobile_android_mozconfig(self):
        """
        Print a message to the console detailing what the user's mozconfig
        should contain.

        GeckoView/Firefox for Android needs an application and an ABI set, and it needs
        paths to the Android SDK and NDK.
        """
        raise NotImplementedError(
            "%s does not yet implement generate_mobile_android_mozconfig()" % __name__
        )

    def install_mobile_android_artifact_mode_packages(self, mozconfig_builder):
        """
        Install packages required to build GeckoView/Firefox for Android (application
        'mobile/android', also known as Fennec) in Artifact Mode.
        """
        raise NotImplementedError(
            "Cannot bootstrap GeckoView/Firefox for Android Artifact Mode: "
            "%s does not yet implement install_mobile_android_artifact_mode_packages()"
            % __name__
        )

    def generate_mobile_android_artifact_mode_mozconfig(self):
        """
        Print a message to the console detailing what the user's mozconfig
        should contain.

        GeckoView/Firefox for Android Artifact Mode needs an application and an ABI set,
        and it needs paths to the Android SDK.
        """
        raise NotImplementedError(
            "%s does not yet implement generate_mobile_android_artifact_mode_mozconfig()"
            % __name__
        )

    def ensure_mach_environment(self, checkout_root):
        mach_binary = os.path.abspath(os.path.join(checkout_root, "mach"))
        if not os.path.exists(mach_binary):
            raise ValueError("mach not found at %s" % mach_binary)
        cmd = [sys.executable, mach_binary, "create-mach-environment"]
        subprocess.check_call(cmd, cwd=checkout_root)

    def ensure_clang_static_analysis_package(self, state_dir, checkout_root):
        """
        Install the clang static analysis package
        """
        raise NotImplementedError(
            "%s does not yet implement ensure_clang_static_analysis_package()"
            % __name__
        )

    def ensure_stylo_packages(self, state_dir, checkout_root):
        """
        Install any necessary packages needed for Stylo development.
        """
        raise NotImplementedError(
            "%s does not yet implement ensure_stylo_packages()" % __name__
        )

    def ensure_nasm_packages(self, state_dir, checkout_root):
        """
        Install nasm.
        """
        raise NotImplementedError(
            "%s does not yet implement ensure_nasm_packages()" % __name__
        )

    def ensure_sccache_packages(self, state_dir, checkout_root):
        """
        Install sccache.
        """
        pass

    def ensure_lucetc_packages(self, state_dir, checkout_root):
        """
        Install lucetc.
        """
        pass

    def ensure_wasi_sysroot_packages(self, state_dir, checkout_root):
        """
        Install the wasi sysroot.
        """
        pass

    def ensure_node_packages(self, state_dir, checkout_root):
        """
        Install any necessary packages needed to supply NodeJS"""
        raise NotImplementedError(
            "%s does not yet implement ensure_node_packages()" % __name__
        )

    def ensure_dump_syms_packages(self, state_dir, checkout_root):
        """
        Install dump_syms.
        """
        pass

    def ensure_fix_stacks_packages(self, state_dir, checkout_root):
        """
        Install fix-stacks.
        """
        pass

    def ensure_minidump_stackwalk_packages(self, state_dir, checkout_root):
        """
        Install minidump_stackwalk.
        """
        pass

    def install_toolchain_static_analysis(
        self, state_dir, checkout_root, toolchain_job
    ):
        clang_tools_path = os.path.join(state_dir, "clang-tools")
        if not os.path.exists(clang_tools_path):
            os.mkdir(clang_tools_path)
        self.install_toolchain_artifact(clang_tools_path, checkout_root, toolchain_job)

    def install_toolchain_artifact(
        self, state_dir, checkout_root, toolchain_job, no_unpack=False
    ):
        mach_binary = os.path.join(checkout_root, "mach")
        mach_binary = os.path.abspath(mach_binary)
        if not os.path.exists(mach_binary):
            raise ValueError("mach not found at %s" % mach_binary)

        # NOTE: Use self.state_dir over the passed-in state_dir, which might be
        # a subdirectory of the actual state directory.
        if not self.state_dir:
            raise ValueError(
                "Need a state directory (e.g. ~/.mozbuild) to download " "artifacts"
            )
        python_location = get_mach_virtualenv_binary(state_dir=self.state_dir)
        if not os.path.exists(python_location):
            raise ValueError("python not found at %s" % python_location)

        cmd = [
            python_location,
            mach_binary,
            "artifact",
            "toolchain",
            "--bootstrap",
            "--from-build",
            toolchain_job,
        ]

        if no_unpack:
            cmd += ["--no-unpack"]

        subprocess.check_call(cmd, cwd=state_dir)

    def run_as_root(self, command):
        if os.geteuid() != 0:
            if which("sudo"):
                command.insert(0, "sudo")
            else:
                command = ["su", "root", "-c", " ".join(command)]

        print("Executing as root:", subprocess.list2cmdline(command))

        subprocess.check_call(command, stdin=sys.stdin)

    def dnf_install(self, *packages):
        if which("dnf"):
            command = ["dnf", "install"]
        else:
            command = ["yum", "install"]

        if self.no_interactive:
            command.append("-y")
        command.extend(packages)

        self.run_as_root(command)

    def dnf_groupinstall(self, *packages):
        if which("dnf"):
            command = ["dnf", "groupinstall"]
        else:
            command = ["yum", "groupinstall"]

        if self.no_interactive:
            command.append("-y")
        command.extend(packages)

        self.run_as_root(command)

    def dnf_update(self, *packages):
        if which("dnf"):
            command = ["dnf", "update"]
        else:
            command = ["yum", "update"]

        if self.no_interactive:
            command.append("-y")
        command.extend(packages)

        self.run_as_root(command)

    def apt_install(self, *packages):
        command = ["apt-get", "install"]
        if self.no_interactive:
            command.append("-y")
        command.extend(packages)

        self.run_as_root(command)

    def apt_update(self):
        command = ["apt-get", "update"]
        if self.no_interactive:
            command.append("-y")

        self.run_as_root(command)

    def apt_add_architecture(self, arch):
        command = ["dpkg", "--add-architecture"]
        command.extend(arch)

        self.run_as_root(command)

    def prompt_int(self, prompt, low, high, default=None):
        """Prompts the user with prompt and requires an integer between low and high.

        If the user doesn't select an option and a default isn't provided, then
        the lowest option is used. This is because some option must be implicitly
        selected if mach is invoked with "--no-interactive"
        """
        if default is not None:
            assert isinstance(default, int)
            assert low <= default <= high
        else:
            default = low

        if self.no_interactive:
            print(prompt)
            print('Selecting "{}" because context is not interactive.'.format(default))
            return default

        while True:
            choice = input(prompt)
            if choice == "" and default is not None:
                return default
            try:
                choice = int(choice)
                if low <= choice <= high:
                    return choice
            except ValueError:
                pass
            print("ERROR! Please enter a valid option!")

    def prompt_yesno(self, prompt):
        """ Prompts the user with prompt and requires a yes/no answer."""
        if self.no_interactive:
            print(prompt)
            print('Selecting "Y" because context is not interactive.')
            return True

        while True:
            choice = input(prompt + " (Yn): ").strip().lower()[:1]
            if choice == "":
                return True
            elif choice in ("y", "n"):
                return choice == "y"

            print("ERROR! Please enter y or n!")

    def _ensure_package_manager_updated(self):
        if self.package_manager_updated:
            return

        self._update_package_manager()
        self.package_manager_updated = True

    def _update_package_manager(self):
        """Updates the package manager's manifests/package list.

        This should be defined in child classes.
        """

    def _parse_version_impl(self, path, name, env, version_param):
        """Execute the given path, returning the version.

        Invokes the path argument with the --version switch
        and returns a LooseVersion representing the output
        if successful. If not, returns None.

        An optional name argument gives the expected program
        name returned as part of the version string, if it's
        different from the basename of the executable.

        An optional env argument allows modifying environment
        variable during the invocation to set options, PATH,
        etc.
        """
        if not name:
            name = os.path.basename(path)
        if name.lower().endswith(".exe"):
            name = name[:-4]

        process = subprocess.run(
            [path, version_param],
            env=env,
            universal_newlines=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        if process.returncode != 0:
            # This can happen e.g. if the user has an inactive pyenv shim in
            # their path. Just silently treat this as a failure to parse the
            # path and move on.
            return None

        match = re.search(name + " ([a-z0-9\.]+)", process.stdout)
        if not match:
            print("ERROR! Unable to identify %s version." % name)
            return None

        return LooseVersion(match.group(1))

    def _parse_version(self, path, name=None, env=None):
        return self._parse_version_impl(path, name, env, "--version")

    def _parse_version_short(self, path, name=None, env=None):
        return self._parse_version_impl(path, name, env, "-v")

    def _hg_cleanenv(self, load_hgrc=False):
        """Returns a copy of the current environment updated with the HGPLAIN
        and HGRCPATH environment variables.

        HGPLAIN prevents Mercurial from applying locale variations to the output
        making it suitable for use in scripts.

        HGRCPATH controls the loading of hgrc files. Setting it to the empty
        string forces that no user or system hgrc file is used.
        """
        env = os.environ.copy()
        env["HGPLAIN"] = "1"
        if not load_hgrc:
            env["HGRCPATH"] = ""

        return env

    def is_mercurial_modern(self):
        hg = which("hg")
        if not hg:
            print(NO_MERCURIAL)
            return False, False, None

        our = self._parse_version(hg, "version", self._hg_cleanenv())
        if not our:
            return True, False, None

        return True, our >= MODERN_MERCURIAL_VERSION, our

    def ensure_mercurial_modern(self):
        installed, modern, version = self.is_mercurial_modern()

        if modern:
            print("Your version of Mercurial (%s) is sufficiently modern." % version)
            return installed, modern

        self._ensure_package_manager_updated()

        if installed:
            print("Your version of Mercurial (%s) is not modern enough." % version)
            print(
                "(Older versions of Mercurial have known security vulnerabilities. "
                "Unless you are running a patched Mercurial version, you may be "
                "vulnerable."
            )
        else:
            print("You do not have Mercurial installed")

        if self.upgrade_mercurial(version) is False:
            return installed, modern

        installed, modern, after = self.is_mercurial_modern()

        if installed and not modern:
            print(MERCURIAL_UPGRADE_FAILED % (MODERN_MERCURIAL_VERSION, after))

        return installed, modern

    def upgrade_mercurial(self, current):
        """Upgrade Mercurial.

        Child classes should reimplement this.

        Return False to not perform a version check after the upgrade is
        performed.
        """
        print(MERCURIAL_UNABLE_UPGRADE % (current, MODERN_MERCURIAL_VERSION))

    def is_python_modern(self, major):
        assert major in (2, 3)

        our = None

        if major == 3:
            our = LooseVersion(platform.python_version())
        else:
            for test in ("python2.7", "python"):
                python = which(test)
                if python:
                    candidate_version = self._parse_version(python, "Python")
                    if candidate_version and candidate_version.version[0] == major:
                        our = candidate_version
                        break

        if our is None:
            return False, None

        modern = {
            2: MODERN_PYTHON2_VERSION,
            3: MODERN_PYTHON3_VERSION,
        }
        return our >= modern[major], our

    def ensure_python_modern(self):
        modern, version = self.is_python_modern(3)
        if modern:
            print("Your version of Python 3 (%s) is new enough." % version)
        else:
            print(
                "ERROR: Your version of Python 3 (%s) is not new enough. You "
                "must have Python >= %s to build Firefox."
                % (version, MODERN_PYTHON3_VERSION)
            )
            print(self.INSTALL_PYTHON_GUIDANCE)
            sys.exit(1)
        modern, version = self.is_python_modern(2)
        if modern:
            print("Your version of Python 2 (%s) is new enough." % version)
        else:
            print(
                "WARNING: Your version of Python 2 (%s) is not new enough. "
                "You must have Python >= %s to build Firefox. Python 2 is "
                "not required to build, so we will proceed. However, Python "
                "2 is required for other development tasks, like running "
                "tests; you may like to have Python 2 installed for that "
                "reason." % (version, MODERN_PYTHON2_VERSION)
            )
            print(self.INSTALL_PYTHON_GUIDANCE)

    def warn_if_pythonpath_is_set(self):
        if "PYTHONPATH" in os.environ:
            print(
                "WARNING: Your PYTHONPATH environment variable is set. This can "
                "cause flaky installations of the requirements, and other unexpected "
                "issues with mach. It is recommended to unset this variable."
            )

    def is_nasm_modern(self):
        nasm = which("nasm")
        if not nasm:
            return False

        our = self._parse_version_short(nasm, "version")
        if not our:
            return False

        return our >= MODERN_NASM_VERSION

    def is_rust_modern(self, cargo_bin):
        rustc = which("rustc", extra_search_dirs=[cargo_bin])
        if not rustc:
            print("Could not find a Rust compiler.")
            return False, None

        our = self._parse_version(rustc)
        if not our:
            return False, None

        return our >= MODERN_RUST_VERSION, our

    def cargo_home(self):
        cargo_home = os.environ.get(
            "CARGO_HOME", os.path.expanduser(os.path.join("~", ".cargo"))
        )
        cargo_bin = os.path.join(cargo_home, "bin")
        return cargo_home, cargo_bin

    def win_to_msys_path(self, path):
        """Convert a windows-style path to msys style."""
        drive, path = os.path.splitdrive(path)
        path = "/".join(path.split("\\"))
        if drive:
            if path[0] == "/":
                path = path[1:]
            path = "/%s/%s" % (drive[:-1], path)
        return path

    def print_rust_path_advice(self, template, cargo_home, cargo_bin):
        # Suggest ~/.cargo/env if it exists.
        if os.path.exists(os.path.join(cargo_home, "env")):
            cmd = "source %s/env" % cargo_home
        else:
            # On Windows rustup doesn't write out ~/.cargo/env
            # so fall back to a manual PATH update. Bootstrap
            # only runs under msys, so a unix-style shell command
            # is appropriate there.
            cargo_bin = self.win_to_msys_path(cargo_bin)
            cmd = "export PATH=%s:$PATH" % cargo_bin
        print(
            template
            % {
                "cargo_bin": cargo_bin,
                "cmd": cmd,
            }
        )

    def ensure_rust_modern(self):
        cargo_home, cargo_bin = self.cargo_home()
        modern, version = self.is_rust_modern(cargo_bin)

        if modern:
            print("Your version of Rust (%s) is new enough." % version)
            rustup = which("rustup", extra_search_dirs=[cargo_bin])
            if rustup:
                self.ensure_rust_targets(rustup, version)
            return

        if version:
            print("Your version of Rust (%s) is too old." % version)

        rustup = which("rustup", extra_search_dirs=[cargo_bin])
        if rustup:
            rustup_version = self._parse_version(rustup)
            if not rustup_version:
                print(RUSTUP_OLD)
                sys.exit(1)
            print("Found rustup. Will try to upgrade.")
            self.upgrade_rust(rustup)

            modern, after = self.is_rust_modern(cargo_bin)
            if not modern:
                print(RUST_UPGRADE_FAILED % (MODERN_RUST_VERSION, after))
                sys.exit(1)
        else:
            # No rustup. Download and run the installer.
            print("Will try to install Rust.")
            self.install_rust()

    def ensure_rust_targets(self, rustup, rust_version):
        """Make sure appropriate cross target libraries are installed."""
        target_list = subprocess.check_output(
            [rustup, "target", "list"], universal_newlines=True
        )
        targets = [
            line.split()[0]
            for line in target_list.splitlines()
            if "installed" in line or "default" in line
        ]
        print("Rust supports %s targets." % ", ".join(targets))

        # Support 32-bit Windows on 64-bit Windows.
        win32 = "i686-pc-windows-msvc"
        win64 = "x86_64-pc-windows-msvc"
        if rust.platform() == win64 and win32 not in targets:
            subprocess.check_call([rustup, "target", "add", win32])

        if "mobile_android" in self.application:
            # Let's add the most common targets.
            if rust_version < LooseVersion("1.33"):
                arm_target = "armv7-linux-androideabi"
            else:
                arm_target = "thumbv7neon-linux-androideabi"
            android_targets = (
                arm_target,
                "aarch64-linux-android",
                "i686-linux-android",
                "x86_64-linux-android",
            )
            for target in android_targets:
                if target not in targets:
                    subprocess.check_call([rustup, "target", "add", target])

    def upgrade_rust(self, rustup):
        """Upgrade Rust.

        Invoke rustup from the given path to update the rust install."""
        subprocess.check_call([rustup, "update"])
        # This installs rustfmt when not already installed, or nothing
        # otherwise, while the update above would have taken care of upgrading
        # it.
        subprocess.check_call([rustup, "component", "add", "rustfmt"])

    def install_rust(self):
        """Download and run the rustup installer."""
        import errno
        import stat
        import tempfile

        platform = rust.platform()
        url = rust.rustup_url(platform)
        checksum = rust.rustup_hash(platform)
        if not url or not checksum:
            print("ERROR: Could not download installer.")
            sys.exit(1)
        print("Downloading rustup-init... ", end="")
        fd, rustup_init = tempfile.mkstemp(prefix=os.path.basename(url))
        os.close(fd)
        try:
            self.http_download_and_save(url, rustup_init, checksum)
            mode = os.stat(rustup_init).st_mode
            os.chmod(rustup_init, mode | stat.S_IRWXU)
            print("Ok")
            print("Running rustup-init...")
            subprocess.check_call(
                [
                    rustup_init,
                    "-y",
                    "--default-toolchain",
                    "stable",
                    "--default-host",
                    platform,
                    "--component",
                    "rustfmt",
                ]
            )
            cargo_home, cargo_bin = self.cargo_home()
            self.print_rust_path_advice(RUST_INSTALL_COMPLETE, cargo_home, cargo_bin)
        finally:
            try:
                os.remove(rustup_init)
            except OSError as e:
                if e.errno != errno.ENOENT:
                    raise

    def http_download_and_save(self, url, dest, hexhash, digest="sha256"):
        """Download the given url and save it to dest.  hexhash is a checksum
        that will be used to validate the downloaded file using the given
        digest algorithm.  The value of digest can be any value accepted by
        hashlib.new.  The default digest used is 'sha256'."""
        f = urlopen(url)
        h = hashlib.new(digest)
        with open(dest, "wb") as out:
            while True:
                data = f.read(4096)
                if data:
                    out.write(data)
                    h.update(data)
                else:
                    break
        if h.hexdigest() != hexhash:
            os.remove(dest)
            raise ValueError("Hash of downloaded file does not match expected hash")

    def ensure_java(self, mozconfig_builder):
        """Verify the presence of java.

        Finds a valid Java (throwing an error if not possible) and encodes it to the mozconfig.
        """

        bin_dir = locate_java_bin_path()
        mozconfig_builder.append(
            """
        # Use the same Java binary that was specified in bootstrap. This way, if the default system
        # Java is different than what Firefox needs, users should just need to override it (with
        # $JAVA_HOME) when running bootstrap, rather than when interacting with the build.
        ac_add_options --with-java-bin-path={}
        """.format(
                bin_dir
            )
        )

        print("Your version of Java ({}) is 1.8.".format(bin_dir))
        return bin_dir
