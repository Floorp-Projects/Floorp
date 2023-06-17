# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import platform
import re
import shutil
import stat
import subprocess
import sys
import time
from collections import OrderedDict
from pathlib import Path
from typing import Optional

# Use distro package to retrieve linux platform information
import distro
from mach.site import MachSiteManager
from mach.telemetry import initialize_telemetry_setting
from mach.util import (
    UserError,
    get_state_dir,
    to_optional_path,
    to_optional_str,
    win_to_msys_path,
)
from mozbuild.base import MozbuildObject
from mozfile import which
from packaging.version import Version

from mozboot.archlinux import ArchlinuxBootstrapper
from mozboot.base import MODERN_RUST_VERSION
from mozboot.centosfedora import CentOSFedoraBootstrapper
from mozboot.debian import DebianBootstrapper
from mozboot.freebsd import FreeBSDBootstrapper
from mozboot.gentoo import GentooBootstrapper
from mozboot.mozconfig import MozconfigBuilder
from mozboot.mozillabuild import MozillaBuildBootstrapper
from mozboot.openbsd import OpenBSDBootstrapper
from mozboot.opensuse import OpenSUSEBootstrapper
from mozboot.osx import OSXBootstrapper, OSXBootstrapperLight
from mozboot.solus import SolusBootstrapper
from mozboot.void import VoidBootstrapper
from mozboot.windows import WindowsBootstrapper

APPLICATION_CHOICE = """
Note on Artifact Mode:

Artifact builds download prebuilt C++ components rather than building
them locally. Artifact builds are faster!

Artifact builds are recommended for people working on Firefox or
Firefox for Android frontends, or the GeckoView Java API. They are unsuitable
for those working on C++ code. For more information see:
https://firefox-source-docs.mozilla.org/contributing/build/artifact_builds.html.

Please choose the version of Firefox you want to build (see note above):
%s
Your choice: """

APPLICATIONS = OrderedDict(
    [
        ("Floorp for Desktop", "browser"),
    ]
)

FINISHED = """
Your system should be ready to build %s!
"""

MOZCONFIG_SUGGESTION_TEMPLATE = """
Paste the lines between the chevrons (>>> and <<<) into
%s:

>>>
%s
<<<
"""

MOZCONFIG_MISMATCH_WARNING_TEMPLATE = """
WARNING! Mismatch detected between the selected build target and the
mozconfig file %s:

Current config
>>>
%s
<<<

Expected config
>>>
%s
<<<
"""

CONFIGURE_MERCURIAL = """
Mozilla recommends a number of changes to Mercurial to enhance your
experience with it.

Would you like to run a configuration wizard to ensure Mercurial is
optimally configured? (This will also ensure 'version-control-tools' is up-to-date)"""

CONFIGURE_GIT = """
Mozilla recommends using git-cinnabar to work with mozilla-central (or
mozilla-unified).

Would you like to run a few configuration steps to ensure Git is
optimally configured?"""

DEBIAN_DISTROS = (
    "debian",
    "ubuntu",
    "linuxmint",
    "elementary",
    "neon",
    "pop",
    "kali",
    "devuan",
    "pureos",
    "deepin",
    "tuxedo",
)

FEDORA_DISTROS = (
    "centos",
    "fedora",
    "rocky",
    "oracle",
)

ADD_GIT_CINNABAR_PATH = """
To add git-cinnabar to the PATH, edit your shell initialization script, which
may be called {prefix}/.bash_profile or {prefix}/.profile, and add the following
lines:

    export PATH="{cinnabar_dir}:$PATH"

Then restart your shell.
"""


OLD_REVISION_WARNING = """
WARNING! You appear to be running `mach bootstrap` from an old revision.
bootstrap is meant primarily for getting developer environments up-to-date to
build the latest version of tree. Running bootstrap on old revisions may fail
and is not guaranteed to bring your machine to any working state in particular.
Proceed at your own peril.
"""


# Version 2.24 changes the "core.commitGraph" setting to be "True" by default.
MINIMUM_RECOMMENDED_GIT_VERSION = Version("2.24")
OLD_GIT_WARNING = """
You are running an older version of git ("{old_version}").
We recommend upgrading to at least version "{minimum_recommended_version}" to improve
performance.
""".strip()


class Bootstrapper(object):
    """Main class that performs system bootstrap."""

    def __init__(
        self,
        choice=None,
        no_interactive=False,
        hg_configure=False,
        no_system_changes=False,
        exclude=[],
        mach_context=None,
    ):
        self.instance = None
        self.choice = choice
        self.hg_configure = hg_configure
        self.no_system_changes = no_system_changes
        self.exclude = exclude
        self.mach_context = mach_context
        cls = None
        args = {
            "no_interactive": no_interactive,
            "no_system_changes": no_system_changes,
        }

        if sys.platform.startswith("linux"):
            # distro package provides reliable ids for popular distributions so
            # we use those instead of the full distribution name
            dist_id, version, codename = distro.linux_distribution(
                full_distribution_name=False
            )

            if dist_id in FEDORA_DISTROS:
                cls = CentOSFedoraBootstrapper
                args["distro"] = dist_id
            elif dist_id in DEBIAN_DISTROS:
                cls = DebianBootstrapper
                args["distro"] = dist_id
                args["codename"] = codename
            elif dist_id in ("gentoo", "funtoo"):
                cls = GentooBootstrapper
            elif dist_id in ("solus"):
                cls = SolusBootstrapper
            elif dist_id in ("arch") or Path("/etc/arch-release").exists():
                cls = ArchlinuxBootstrapper
            elif dist_id in ("void"):
                cls = VoidBootstrapper
            elif dist_id in (
                "opensuse",
                "opensuse-leap",
                "opensuse-tumbleweed",
                "suse",
            ):
                cls = OpenSUSEBootstrapper
            else:
                raise NotImplementedError(
                    "Bootstrap support for this Linux "
                    "distro not yet available: " + dist_id
                )

            args["version"] = version
            args["dist_id"] = dist_id

        elif sys.platform.startswith("darwin"):
            # TODO Support Darwin platforms that aren't OS X.
            osx_version = platform.mac_ver()[0]
            if platform.machine() == "arm64" or _macos_is_running_under_rosetta():
                cls = OSXBootstrapperLight
            else:
                cls = OSXBootstrapper
            args["version"] = osx_version

        elif sys.platform.startswith("openbsd"):
            cls = OpenBSDBootstrapper
            args["version"] = platform.uname()[2]

        elif sys.platform.startswith(("dragonfly", "freebsd", "netbsd")):
            cls = FreeBSDBootstrapper
            args["version"] = platform.release()
            args["flavor"] = platform.system()

        elif sys.platform.startswith("win32") or sys.platform.startswith("msys"):
            if "MOZILLABUILD" in os.environ:
                cls = MozillaBuildBootstrapper
            else:
                cls = WindowsBootstrapper
        if cls is None:
            raise NotImplementedError(
                "Bootstrap support is not yet available " "for your OS."
            )

        self.instance = cls(**args)

    def maybe_install_private_packages_or_exit(self, application, checkout_type):
        # Install the clang packages needed for building the style system, as
        # well as the version of NodeJS that we currently support.
        # Also install the clang static-analysis package by default
        # The best place to install our packages is in the state directory
        # we have.  We should have created one above in non-interactive mode.
        self.instance.auto_bootstrap(application, self.exclude)
        self.instance.install_toolchain_artifact("fix-stacks")
        self.instance.install_toolchain_artifact("minidump-stackwalk")
        if not self.instance.artifact_mode:
            self.instance.install_toolchain_artifact("clang-tools/clang-tidy")
            self.instance.ensure_sccache_packages()
        # Like 'ensure_browser_packages' or 'ensure_mobile_android_packages'
        getattr(self.instance, "ensure_%s_packages" % application)()

    def check_code_submission(self, checkout_root: Path):
        if self.instance.no_interactive or which("moz-phab"):
            return

        # Skip moz-phab install until bug 1696357 is fixed and makes it to a moz-phab
        # release.
        if sys.platform.startswith("darwin") and platform.machine() == "arm64":
            return

        if not self.instance.prompt_yesno("Will you be submitting commits to Mozilla?"):
            return

        mach_binary = checkout_root / "mach"
        subprocess.check_call((sys.executable, str(mach_binary), "install-moz-phab"))

    def bootstrap(self, settings):
        if self.choice is None:
            applications = APPLICATIONS
            # Like ['1. Firefox for Desktop', '2. Firefox for Android Artifact Mode', ...].
            labels = [
                "%s. %s" % (i, name) for i, name in enumerate(applications.keys(), 1)
            ]
            choices = ["  {} [default]".format(labels[0])]
            choices += ["  {}".format(label) for label in labels[1:]]
            prompt = APPLICATION_CHOICE % "\n".join(choices)
            prompt_choice = self.instance.prompt_int(
                prompt=prompt, low=1, high=len(applications)
            )
            name, application = list(applications.items())[prompt_choice - 1]
        elif self.choice in APPLICATIONS.keys():
            name, application = self.choice, APPLICATIONS[self.choice]
        elif self.choice in APPLICATIONS.values():
            name, application = next(
                (k, v) for k, v in APPLICATIONS.items() if v == self.choice
            )
        else:
            raise Exception(
                "Please pick a valid application choice: (%s)"
                % "/".join(APPLICATIONS.keys())
            )

        mozconfig_builder = MozconfigBuilder()
        self.instance.application = application
        self.instance.artifact_mode = "artifact_mode" in application

        self.instance.warn_if_pythonpath_is_set()

        if sys.platform.startswith("darwin") and not os.environ.get(
            "MACH_I_DO_WANT_TO_USE_ROSETTA"
        ):
            # If running on arm64 mac, check whether we're running under
            # Rosetta and advise against it.
            if _macos_is_running_under_rosetta():
                print(
                    "Python is being emulated under Rosetta. Please use a native "
                    "Python instead. If you still really want to go ahead, set "
                    "the MACH_I_DO_WANT_TO_USE_ROSETTA environment variable.",
                    file=sys.stderr,
                )
                return 1

        state_dir = Path(get_state_dir())
        self.instance.state_dir = state_dir

        hg = to_optional_path(which("hg"))

        # We need to enable the loading of hgrc in case extensions are
        # required to open the repo.
        (checkout_type, checkout_root) = current_firefox_checkout(
            env=self.instance._hg_cleanenv(load_hgrc=True),
            hg=hg,
        )
        self.instance.srcdir = checkout_root
        self.instance.validate_environment()
        self._validate_python_environment(checkout_root)

        if self.instance.no_system_changes:
            self.maybe_install_private_packages_or_exit(application, checkout_type)
            self._output_mozconfig(application, mozconfig_builder)
            sys.exit(0)

        self.instance.install_system_packages()

        # Like 'install_browser_packages' or 'install_mobile_android_packages'.
        getattr(self.instance, "install_%s_packages" % application)(mozconfig_builder)

        if not self.instance.artifact_mode:
            self.instance.ensure_rust_modern()

        git = to_optional_path(which("git"))

        # Possibly configure Mercurial, but not if the current checkout or repo
        # type is Git.
        hg_installed = bool(hg)
        if checkout_type == "hg":
            hg_installed, hg_modern = self.instance.ensure_mercurial_modern()

        if hg_installed and checkout_type == "hg":
            if not self.instance.no_interactive:
                configure_hg = self.instance.prompt_yesno(prompt=CONFIGURE_MERCURIAL)
            else:
                configure_hg = self.hg_configure

            if configure_hg:
                configure_mercurial(hg, state_dir)

        # Offer to configure Git, if the current checkout or repo type is Git.
        elif git and checkout_type == "git":
            should_configure_git = False
            if not self.instance.no_interactive:
                should_configure_git = self.instance.prompt_yesno(prompt=CONFIGURE_GIT)
            else:
                # Assuming default configuration setting applies to all VCS.
                should_configure_git = self.hg_configure

            if should_configure_git:
                configure_git(
                    git,
                    to_optional_path(which("git-cinnabar")),
                    state_dir,
                    checkout_root,
                )

        self.maybe_install_private_packages_or_exit(application, checkout_type)
        self.check_code_submission(checkout_root)
        # Wait until after moz-phab setup to check telemetry so that employees
        # will be automatically opted-in.
        if not self.instance.no_interactive and not settings.mach_telemetry.is_set_up:
            initialize_telemetry_setting(settings, str(checkout_root), str(state_dir))

        self._output_mozconfig(application, mozconfig_builder)

        print(FINISHED % name)
        if not (
            which("rustc")
            and self.instance._parse_version(Path("rustc")) >= MODERN_RUST_VERSION
        ):
            print(
                "To build %s, please restart the shell (Start a new terminal window)"
                % name
            )

    def _default_mozconfig_path(self):
        return Path(self.mach_context.topdir) / "mozconfig"

    def _read_default_mozconfig(self):
        path = self._default_mozconfig_path()
        with open(path, "r") as mozconfig_file:
            return mozconfig_file.read()

    def _write_default_mozconfig(self, raw_mozconfig):
        path = self._default_mozconfig_path()
        with open(path, "w") as mozconfig_file:
            mozconfig_file.write(raw_mozconfig)
            print(f'Your requested configuration has been written to "{path}".')

    def _show_mozconfig_suggestion(self, raw_mozconfig):
        suggestion = MOZCONFIG_SUGGESTION_TEMPLATE % (
            self._default_mozconfig_path(),
            raw_mozconfig,
        )
        print(suggestion, end="")

    def _check_default_mozconfig_mismatch(
        self, current_mozconfig_info, expected_application, expected_raw_mozconfig
    ):
        current_raw_mozconfig = self._read_default_mozconfig()
        current_application = current_mozconfig_info["project"][0].replace("/", "_")
        if current_mozconfig_info["artifact-builds"]:
            current_application += "_artifact_mode"

        if expected_application == current_application:
            if expected_raw_mozconfig == current_raw_mozconfig:
                return

            # There's minor difference, show the suggestion.
            self._show_mozconfig_suggestion(expected_raw_mozconfig)
            return

        warning = MOZCONFIG_MISMATCH_WARNING_TEMPLATE % (
            self._default_mozconfig_path(),
            current_raw_mozconfig,
            expected_raw_mozconfig,
        )
        print(warning)

        if not self.instance.prompt_yesno("Do you want to overwrite the config?"):
            return

        self._write_default_mozconfig(expected_raw_mozconfig)

    def _output_mozconfig(self, application, mozconfig_builder):
        # Like 'generate_browser_mozconfig' or 'generate_mobile_android_mozconfig'.
        additional_mozconfig = getattr(
            self.instance, "generate_%s_mozconfig" % application
        )()
        if additional_mozconfig:
            mozconfig_builder.append(additional_mozconfig)
        raw_mozconfig = mozconfig_builder.generate()

        current_mozconfig_info = MozbuildObject.get_base_mozconfig_info(
            self.mach_context.topdir, None, ""
        )
        current_mozconfig_path = current_mozconfig_info["mozconfig"]["path"]

        if current_mozconfig_path:
            # mozconfig file exists
            if self._default_mozconfig_path().exists() and Path.samefile(
                Path(current_mozconfig_path), self._default_mozconfig_path()
            ):
                # This mozconfig file may be created by bootstrap.
                self._check_default_mozconfig_mismatch(
                    current_mozconfig_info, application, raw_mozconfig
                )
            elif raw_mozconfig:
                # The mozconfig file is created by user.
                self._show_mozconfig_suggestion(raw_mozconfig)
        elif raw_mozconfig:
            # No mozconfig file exists yet
            self._write_default_mozconfig(raw_mozconfig)

    def _validate_python_environment(self, topsrcdir):
        valid = True
        try:
            # distutils is singled out here because some distros (namely Ubuntu)
            # include it in a separate package outside of the main Python
            # installation.
            import distutils.spawn
            import distutils.sysconfig

            assert distutils.sysconfig is not None and distutils.spawn is not None
        except ImportError as e:
            print("ERROR: Could not import package %s" % e.name, file=sys.stderr)
            self.instance.suggest_install_distutils()
            valid = False
        except AssertionError:
            print("ERROR: distutils is not behaving as expected.", file=sys.stderr)
            self.instance.suggest_install_distutils()
            valid = False
        pip3 = to_optional_path(which("pip3"))
        if not pip3:
            print("ERROR: Could not find pip3.", file=sys.stderr)
            self.instance.suggest_install_pip3()
            valid = False
        if not valid:
            print(
                "ERROR: Your Python installation will not be able to run "
                "`mach bootstrap`. `mach bootstrap` cannot maintain your "
                "Python environment for you; fix the errors shown here, and "
                "then re-run `mach bootstrap`.",
                file=sys.stderr,
            )
            sys.exit(1)

        mach_site = MachSiteManager.from_environment(
            topsrcdir,
            lambda: os.path.normpath(get_state_dir(True, topsrcdir=topsrcdir)),
        )
        mach_site.attempt_populate_optional_packages()


def update_vct(hg: Path, root_state_dir: Path):
    """Ensure version-control-tools in the state directory is up to date."""
    vct_dir = root_state_dir / "version-control-tools"

    # Ensure the latest revision of version-control-tools is present.
    update_mercurial_repo(
        hg, "https://hg.mozilla.org/hgcustom/version-control-tools", vct_dir, "@"
    )

    return vct_dir


def configure_mercurial(hg: Optional[Path], root_state_dir: Path):
    """Run the Mercurial configuration wizard."""
    vct_dir = update_vct(hg, root_state_dir)

    hg = to_optional_str(hg)

    # Run the config wizard from v-c-t.
    args = [
        hg,
        "--config",
        f"extensions.configwizard={vct_dir}/hgext/configwizard",
        "configwizard",
    ]
    subprocess.call(args)


def update_mercurial_repo(hg: Path, url, dest: Path, revision):
    """Perform a clone/pull + update of a Mercurial repository."""
    # Disable common extensions whose older versions may cause `hg`
    # invocations to abort.
    pull_args = [str(hg)]
    if dest.exists():
        pull_args.extend(["pull", url])
        cwd = dest
    else:
        pull_args.extend(["clone", "--noupdate", url, str(dest)])
        cwd = "/"

    update_args = [str(hg), "update", "-r", revision]

    print("=" * 80)
    print(f"Ensuring {url} is up to date at {dest}")

    env = os.environ.copy()
    env.update({"HGPLAIN": "1"})

    try:
        subprocess.check_call(pull_args, cwd=str(cwd), env=env)
        subprocess.check_call(update_args, cwd=str(dest), env=env)
    finally:
        print("=" * 80)


def current_firefox_checkout(env, hg: Optional[Path] = None):
    """Determine whether we're in a Firefox checkout.

    Returns one of None, ``git``, or ``hg``.
    """
    HG_ROOT_REVISIONS = set(
        [
            # From mozilla-unified.
            "8ba995b74e18334ab3707f27e9eb8f4e37ba3d29"
        ]
    )

    path = Path.cwd()
    while path:
        hg_dir = path / ".hg"
        git_dir = path / ".git"
        moz_configure = path / "moz.configure"
        if hg and hg_dir.exists():
            # Verify the hg repo is a Firefox repo by looking at rev 0.
            try:
                node = subprocess.check_output(
                    [str(hg), "log", "-r", "0", "--template", "{node}"],
                    cwd=str(path),
                    env=env,
                    universal_newlines=True,
                )
                if node in HG_ROOT_REVISIONS:
                    _warn_if_risky_revision(path)
                    return "hg", path
                # Else the root revision is different. There could be nested
                # repos. So keep traversing the parents.
            except subprocess.CalledProcessError:
                pass

        # Just check for known-good files in the checkout, to prevent attempted
        # foot-shootings.  Determining a canonical git checkout of mozilla-unified
        # is...complicated
        elif git_dir.exists() or hg_dir.exists():
            moz_configure = path / "moz.configure"
            if moz_configure.exists():
                _warn_if_risky_revision(path)
                return ("git" if git_dir.exists() else "hg"), path
        elif moz_configure.exists():
            return "SOURCE", path

        if not len(path.parents):
            break
        path = path.parent

    raise UserError(
        "Could not identify the root directory of your checkout! "
        "Are you running `mach bootstrap` in an hg or git clone?"
    )


def update_git_tools(git: Optional[Path], root_state_dir: Path):
    """Update git tools, hooks and extensions"""
    # Ensure git-cinnabar is up to date.
    cinnabar_dir = root_state_dir / "git-cinnabar"
    cinnabar_exe = cinnabar_dir / "git-cinnabar"

    if sys.platform.startswith(("win32", "msys")):
        cinnabar_exe = cinnabar_exe.with_suffix(".exe")

    # Previously, this script would do a full clone of the git-cinnabar
    # repository. It now only downloads prebuilt binaries, so if we are
    # updating from an old setup, remove the repository and start over.
    if (cinnabar_dir / ".git").exists():
        # git sets pack files read-only, which causes problems removing
        # them on Windows. To work around that, we use an error handler
        # on rmtree that retries to remove the file after chmod'ing it.
        def onerror(func, path, exc):
            if func == os.unlink:
                os.chmod(path, stat.S_IRWXU)
                func(path)
            else:
                raise

        shutil.rmtree(str(cinnabar_dir), onerror=onerror)

    # If we already have an executable, ask it to update itself.
    exists = cinnabar_exe.exists()
    if exists:
        try:
            subprocess.check_call([str(cinnabar_exe), "self-update"])
        except subprocess.CalledProcessError as e:
            print(e)

    # git-cinnabar 0.6.0rc1 self-update had a bug that could leave an empty
    # file. If that happens, install from scratch.
    if not exists or cinnabar_exe.stat().st_size == 0:
        from urllib.request import urlopen

        import certifi

        if not cinnabar_dir.exists():
            cinnabar_dir.mkdir()

        cinnabar_url = "https://github.com/glandium/git-cinnabar/"
        download_py = cinnabar_dir / "download.py"
        with open(download_py, "wb") as fh:
            shutil.copyfileobj(
                urlopen(
                    f"{cinnabar_url}/raw/master/download.py", cafile=certifi.where()
                ),
                fh,
            )

        try:
            subprocess.check_call(
                [sys.executable, str(download_py)], cwd=str(cinnabar_dir)
            )
        except subprocess.CalledProcessError as e:
            print(e)
        finally:
            download_py.unlink()

    return cinnabar_dir


def configure_git(
    git: Optional[Path],
    cinnabar: Optional[Path],
    root_state_dir: Path,
    top_src_dir: Path,
):
    """Run the Git configuration steps."""

    git_str = to_optional_str(git)

    match = re.search(
        r"(\d+\.\d+\.\d+)",
        subprocess.check_output([git_str, "--version"], universal_newlines=True),
    )
    if not match:
        raise Exception("Could not find git version")
    git_version = Version(match.group(1))

    if git_version < MINIMUM_RECOMMENDED_GIT_VERSION:
        print(
            OLD_GIT_WARNING.format(
                old_version=git_version,
                minimum_recommended_version=MINIMUM_RECOMMENDED_GIT_VERSION,
            )
        )

    if git_version >= Version("2.17"):
        # "core.untrackedCache" has a bug before 2.17
        subprocess.check_call(
            [git_str, "config", "core.untrackedCache", "true"], cwd=str(top_src_dir)
        )

    cinnabar_dir = str(update_git_tools(git, root_state_dir))

    if not cinnabar:
        if "MOZILLABUILD" in os.environ:
            # Slightly modify the path on Windows to be correct
            # for the copy/paste into the .bash_profile
            cinnabar_dir = win_to_msys_path(cinnabar_dir)

            print(
                ADD_GIT_CINNABAR_PATH.format(
                    prefix="%USERPROFILE%", cinnabar_dir=cinnabar_dir
                )
            )
        else:
            print(ADD_GIT_CINNABAR_PATH.format(prefix="~", cinnabar_dir=cinnabar_dir))


def _warn_if_risky_revision(path: Path):
    # Warn the user if they're trying to bootstrap from an obviously old
    # version of tree as reported by the version control system (a month in
    # this case). This is an approximate calculation but is probably good
    # enough for our purposes.
    NUM_SECONDS_IN_MONTH = 60 * 60 * 24 * 30
    from mozversioncontrol import get_repository_object

    repo = get_repository_object(path)
    if (time.time() - repo.get_commit_time()) >= NUM_SECONDS_IN_MONTH:
        print(OLD_REVISION_WARNING)


def _macos_is_running_under_rosetta():
    proc = subprocess.run(
        ["sysctl", "-n", "sysctl.proc_translated"],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    return (
        proc.returncode == 0 and proc.stdout.decode("ascii", "replace").strip() == "1"
    )
