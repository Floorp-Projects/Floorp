# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from collections import OrderedDict

import os
import platform
import re
import sys
import subprocess
import time
from distutils.version import LooseVersion
from mozfile import which
from mach.util import get_state_dir, UserError
from mach.telemetry import initialize_telemetry_setting

from mozboot.base import MODERN_RUST_VERSION
from mozboot.centosfedora import CentOSFedoraBootstrapper
from mozboot.opensuse import OpenSUSEBootstrapper
from mozboot.debian import DebianBootstrapper
from mozboot.freebsd import FreeBSDBootstrapper
from mozboot.gentoo import GentooBootstrapper
from mozboot.osx import OSXBootstrapper, OSXBootstrapperLight
from mozboot.openbsd import OpenBSDBootstrapper
from mozboot.archlinux import ArchlinuxBootstrapper
from mozboot.solus import SolusBootstrapper
from mozboot.void import VoidBootstrapper
from mozboot.windows import WindowsBootstrapper
from mozboot.mozillabuild import MozillaBuildBootstrapper
from mozboot.mozconfig import find_mozconfig, MozconfigBuilder

# Use distro package to retrieve linux platform information
import distro

APPLICATION_CHOICE = """
Note on Artifact Mode:

Artifact builds download prebuilt C++ components rather than building
them locally. Artifact builds are faster!

Artifact builds are recommended for people working on Firefox or
Firefox for Android frontends, or the GeckoView Java API. They are unsuitable
for those working on C++ code. For more information see:
https://firefox-source-docs.mozilla.org/contributing/build/artifact_builds.html.

Please choose the version of Firefox you want to build:
%s
Your choice:
""".strip()

APPLICATIONS = OrderedDict(
    [
        ("Firefox for Desktop Artifact Mode", "browser_artifact_mode"),
        ("Firefox for Desktop", "browser"),
        ("GeckoView/Firefox for Android Artifact Mode", "mobile_android_artifact_mode"),
        ("GeckoView/Firefox for Android", "mobile_android"),
        ("SpiderMonkey JavaScript engine", "js"),
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
""".strip()

CONFIGURE_MERCURIAL = """
Mozilla recommends a number of changes to Mercurial to enhance your
experience with it.

Would you like to run a configuration wizard to ensure Mercurial is
optimally configured?"""

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
MINIMUM_RECOMMENDED_GIT_VERSION = LooseVersion("2.24")
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
        mach_context=None,
    ):
        self.instance = None
        self.choice = choice
        self.hg_configure = hg_configure
        self.no_system_changes = no_system_changes
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

            if dist_id in ("centos", "fedora", "rocky"):
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
            elif dist_id in ("arch") or os.path.exists("/etc/arch-release"):
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
            if platform.machine() == "arm64":
                cls = OSXBootstrapperLight
            else:
                cls = OSXBootstrapper
            args["version"] = osx_version

        elif sys.platform.startswith("openbsd"):
            cls = OpenBSDBootstrapper
            args["version"] = platform.uname()[2]

        elif sys.platform.startswith("dragonfly") or sys.platform.startswith("freebsd"):
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

    def maybe_install_private_packages_or_exit(
        self, state_dir, checkout_root, application
    ):
        # Install the clang packages needed for building the style system, as
        # well as the version of NodeJS that we currently support.
        # Also install the clang static-analysis package by default
        # The best place to install our packages is in the state directory
        # we have.  We should have created one above in non-interactive mode.
        self.instance.ensure_node_packages(state_dir, checkout_root)
        self.instance.ensure_fix_stacks_packages(state_dir, checkout_root)
        self.instance.ensure_minidump_stackwalk_packages(state_dir, checkout_root)
        if not self.instance.artifact_mode:
            self.instance.ensure_stylo_packages(state_dir, checkout_root)
            self.instance.ensure_clang_static_analysis_package(state_dir, checkout_root)
            self.instance.ensure_nasm_packages(state_dir, checkout_root)
            self.instance.ensure_sccache_packages(state_dir, checkout_root)
        # Like 'ensure_browser_packages' or 'ensure_mobile_android_packages'
        getattr(self.instance, "ensure_%s_packages" % application)(
            state_dir, checkout_root
        )

    def check_code_submission(self, checkout_root):
        if self.instance.no_interactive or which("moz-phab"):
            return

        # Skip moz-phab install until bug 1696357 is fixed and makes it to a moz-phab
        # release.
        if sys.platform.startswith("darwin") and platform.machine() == "arm64":
            return

        if not self.instance.prompt_yesno("Will you be submitting commits to Mozilla?"):
            return

        mach_binary = os.path.join(checkout_root, "mach")
        subprocess.check_call((sys.executable, mach_binary, "install-moz-phab"))

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
            proc = subprocess.run(
                ["sysctl", "-n", "sysctl.proc_translated"],
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
            )
            if (
                proc.returncode == 0
                and proc.stdout.decode("ascii", "replace").strip() == "1"
            ):
                print(
                    "Python is being emulated under Rosetta. Please use a native "
                    "Python instead. If you still really want to go ahead, set "
                    "the MACH_I_DO_WANT_TO_USE_ROSETTA environment variable.",
                    file=sys.stderr,
                )
                return 1

        state_dir = get_state_dir()
        self.instance.state_dir = state_dir

        # We need to enable the loading of hgrc in case extensions are
        # required to open the repo.
        (checkout_type, checkout_root) = current_firefox_checkout(
            env=self.instance._hg_cleanenv(load_hgrc=True), hg=which("hg")
        )
        self.instance.validate_environment(checkout_root)
        self._validate_python_environment()

        if self.instance.no_system_changes:
            self.maybe_install_private_packages_or_exit(
                state_dir, checkout_root, application
            )
            self._output_mozconfig(application, mozconfig_builder)
            sys.exit(0)

        self.instance.install_system_packages()

        # Like 'install_browser_packages' or 'install_mobile_android_packages'.
        getattr(self.instance, "install_%s_packages" % application)(mozconfig_builder)

        hg_installed, hg_modern = self.instance.ensure_mercurial_modern()
        if not self.instance.artifact_mode:
            self.instance.ensure_rust_modern()

        # Possibly configure Mercurial, but not if the current checkout or repo
        # type is Git.
        if hg_installed and checkout_type == "hg":
            if not self.instance.no_interactive:
                configure_hg = self.instance.prompt_yesno(prompt=CONFIGURE_MERCURIAL)
            else:
                configure_hg = self.hg_configure

            if configure_hg:
                configure_mercurial(which("hg"), state_dir)

        # Offer to configure Git, if the current checkout or repo type is Git.
        elif which("git") and checkout_type == "git":
            should_configure_git = False
            if not self.instance.no_interactive:
                should_configure_git = self.instance.prompt_yesno(prompt=CONFIGURE_GIT)
            else:
                # Assuming default configuration setting applies to all VCS.
                should_configure_git = self.hg_configure

            if should_configure_git:
                configure_git(
                    which("git"), which("git-cinnabar"), state_dir, checkout_root
                )

        self.maybe_install_private_packages_or_exit(
            state_dir, checkout_root, application
        )
        self.check_code_submission(checkout_root)
        # Wait until after moz-phab setup to check telemetry so that employees
        # will be automatically opted-in.
        if not self.instance.no_interactive and not settings.mach_telemetry.is_set_up:
            initialize_telemetry_setting(settings, checkout_root, state_dir)

        print(FINISHED % name)
        if not (
            which("rustc")
            and self.instance._parse_version("rustc") >= MODERN_RUST_VERSION
        ):
            print(
                "To build %s, please restart the shell (Start a new terminal window)"
                % name
            )

        self._output_mozconfig(application, mozconfig_builder)

    def _output_mozconfig(self, application, mozconfig_builder):
        # Like 'generate_browser_mozconfig' or 'generate_mobile_android_mozconfig'.
        additional_mozconfig = getattr(
            self.instance, "generate_%s_mozconfig" % application
        )()
        if additional_mozconfig:
            mozconfig_builder.append(additional_mozconfig)
        raw_mozconfig = mozconfig_builder.generate()

        if raw_mozconfig:
            mozconfig_path = find_mozconfig(self.mach_context.topdir)
            if not mozconfig_path:
                # No mozconfig file exists yet
                mozconfig_path = os.path.join(self.mach_context.topdir, "mozconfig")
                with open(mozconfig_path, "w") as mozconfig_file:
                    mozconfig_file.write(raw_mozconfig)
                print(
                    'Your requested configuration has been written to "%s".'
                    % mozconfig_path
                )
            else:
                suggestion = MOZCONFIG_SUGGESTION_TEMPLATE % (
                    mozconfig_path,
                    raw_mozconfig,
                )
                print(suggestion, end="")

    def _validate_python_environment(self):
        valid = True
        try:
            # distutils is singled out here because some distros (namely Ubuntu)
            # include it in a separate package outside of the main Python
            # installation.
            import distutils.sysconfig
            import distutils.spawn

            assert distutils.sysconfig is not None and distutils.spawn is not None
        except ImportError as e:
            print("ERROR: Could not import package %s" % e.name, file=sys.stderr)
            self.instance.suggest_install_distutils()
            valid = False
        except AssertionError:
            print("ERROR: distutils is not behaving as expected.", file=sys.stderr)
            self.instance.suggest_install_distutils()
            valid = False
        pip3 = which("pip3")
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


def update_vct(hg, root_state_dir):
    """Ensure version-control-tools in the state directory is up to date."""
    vct_dir = os.path.join(root_state_dir, "version-control-tools")

    # Ensure the latest revision of version-control-tools is present.
    update_mercurial_repo(
        hg, "https://hg.mozilla.org/hgcustom/version-control-tools", vct_dir, "@"
    )

    return vct_dir


def configure_mercurial(hg, root_state_dir):
    """Run the Mercurial configuration wizard."""
    vct_dir = update_vct(hg, root_state_dir)

    # Run the config wizard from v-c-t.
    args = [
        hg,
        "--config",
        "extensions.configwizard=%s/hgext/configwizard" % vct_dir,
        "configwizard",
    ]
    subprocess.call(args)


def update_mercurial_repo(hg, url, dest, revision):
    """Perform a clone/pull + update of a Mercurial repository."""
    # Disable common extensions whose older versions may cause `hg`
    # invocations to abort.
    pull_args = [hg]
    if os.path.exists(dest):
        pull_args.extend(["pull", url])
        cwd = dest
    else:
        pull_args.extend(["clone", "--noupdate", url, dest])
        cwd = "/"

    update_args = [hg, "update", "-r", revision]

    print("=" * 80)
    print("Ensuring %s is up to date at %s" % (url, dest))

    env = os.environ.copy()
    env.update({"HGPLAIN": "1"})

    try:
        subprocess.check_call(pull_args, cwd=cwd, env=env)
        subprocess.check_call(update_args, cwd=dest, env=env)
    finally:
        print("=" * 80)


def current_firefox_checkout(env, hg=None):
    """Determine whether we're in a Firefox checkout.

    Returns one of None, ``git``, or ``hg``.
    """
    HG_ROOT_REVISIONS = set(
        [
            # From mozilla-unified.
            "8ba995b74e18334ab3707f27e9eb8f4e37ba3d29"
        ]
    )

    path = os.getcwd()
    while path:
        hg_dir = os.path.join(path, ".hg")
        git_dir = os.path.join(path, ".git")
        if hg and os.path.exists(hg_dir):
            # Verify the hg repo is a Firefox repo by looking at rev 0.
            try:
                node = subprocess.check_output(
                    [hg, "log", "-r", "0", "--template", "{node}"],
                    cwd=path,
                    env=env,
                    universal_newlines=True,
                )
                if node in HG_ROOT_REVISIONS:
                    _warn_if_risky_revision(path)
                    return ("hg", path)
                # Else the root revision is different. There could be nested
                # repos. So keep traversing the parents.
            except subprocess.CalledProcessError:
                pass

        # Just check for known-good files in the checkout, to prevent attempted
        # foot-shootings.  Determining a canonical git checkout of mozilla-unified
        # is...complicated
        elif os.path.exists(git_dir):
            moz_configure = os.path.join(path, "moz.configure")
            if os.path.exists(moz_configure):
                _warn_if_risky_revision(path)
                return ("git", path)

        path, child = os.path.split(path)
        if child == "":
            break

    raise UserError(
        "Could not identify the root directory of your checkout! "
        "Are you running `mach bootstrap` in an hg or git clone?"
    )


def update_git_tools(git, root_state_dir):
    """Update git tools, hooks and extensions"""
    # Ensure git-cinnabar is up to date.
    cinnabar_dir = os.path.join(root_state_dir, "git-cinnabar")

    # Ensure the latest revision of git-cinnabar is present.
    update_git_repo(git, "https://github.com/glandium/git-cinnabar.git", cinnabar_dir)

    # Perform a download of cinnabar.
    download_args = [git, "cinnabar", "download"]

    try:
        subprocess.check_call(download_args, cwd=cinnabar_dir)
    except subprocess.CalledProcessError as e:
        print(e)
    return cinnabar_dir


def update_git_repo(git, url, dest):
    """Perform a clone/pull + update of a Git repository."""
    pull_args = [git]

    if os.path.exists(dest):
        pull_args.extend(["pull"])
        cwd = dest
    else:
        pull_args.extend(["clone", "--no-checkout", url, dest])
        cwd = "/"

    update_args = [git, "checkout"]

    print("=" * 80)
    print("Ensuring %s is up to date at %s" % (url, dest))

    try:
        subprocess.check_call(pull_args, cwd=cwd)
        subprocess.check_call(update_args, cwd=dest)
    finally:
        print("=" * 80)


def configure_git(git, cinnabar, root_state_dir, top_src_dir):
    """Run the Git configuration steps."""

    match = re.search(
        r"(\d+\.\d+\.\d+)",
        subprocess.check_output([git, "--version"], universal_newlines=True),
    )
    if not match:
        raise Exception("Could not find git version")
    git_version = LooseVersion(match.group(1))

    if git_version < MINIMUM_RECOMMENDED_GIT_VERSION:
        print(
            OLD_GIT_WARNING.format(
                old_version=git_version,
                minimum_recommended_version=MINIMUM_RECOMMENDED_GIT_VERSION,
            )
        )

    if git_version >= LooseVersion("2.17"):
        # "core.untrackedCache" has a bug before 2.17
        subprocess.check_call(
            [git, "config", "core.untrackedCache", "true"], cwd=top_src_dir
        )

    cinnabar_dir = update_git_tools(git, root_state_dir)

    if not cinnabar:
        if "MOZILLABUILD" in os.environ:
            # Slightly modify the path on Windows to be correct
            # for the copy/paste into the .bash_profile
            cinnabar_dir = "/" + cinnabar_dir
            cinnabar_dir = cinnabar_dir.replace(":", "")

            print(
                ADD_GIT_CINNABAR_PATH.format(
                    prefix="%USERPROFILE%", cinnabar_dir=cinnabar_dir
                )
            )
        else:
            print(ADD_GIT_CINNABAR_PATH.format(prefix="~", cinnabar_dir=cinnabar_dir))


def _warn_if_risky_revision(path):
    # Warn the user if they're trying to bootstrap from an obviously old
    # version of tree as reported by the version control system (a month in
    # this case). This is an approximate calculation but is probably good
    # enough for our purposes.
    NUM_SECONDS_IN_MONTH = 60 * 60 * 24 * 30
    from mozversioncontrol import get_repository_object

    repo = get_repository_object(path)
    if (time.time() - repo.get_commit_time()) >= NUM_SECONDS_IN_MONTH:
        print(OLD_REVISION_WARNING)
