#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This script provides one-line bootstrap support to configure systems to build
# the tree. It does so by cloning the repo before calling directly into `mach
# bootstrap`.

# Note that this script can't assume anything in particular about the host
# Python environment (except that it's run with a sufficiently recent version of
# Python 3), so we are restricted to stdlib modules.

from __future__ import absolute_import, print_function, unicode_literals

import sys

major, minor = sys.version_info[:2]
if (major < 3) or (major == 3 and minor < 5):
    print(
        "Bootstrap currently only runs on Python 3.5+."
        "Please try re-running with python3.5+."
    )
    sys.exit(1)

import os
import shutil
import stat
import subprocess
import tempfile
import zipfile

from optparse import OptionParser
from urllib.request import urlopen

CLONE_MERCURIAL_PULL_FAIL = """
Failed to pull from hg.mozilla.org.

This is most likely because of unstable network connection.
Try running `cd %s && hg pull https://hg.mozilla.org/mozilla-unified` manually,
or download a mercurial bundle and use it:
https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Source_Code/Mercurial/Bundles"""

WINDOWS = sys.platform.startswith("win32") or sys.platform.startswith("msys")
VCS_HUMAN_READABLE = {
    "hg": "Mercurial",
    "git": "Git",
}


def which(name):
    """Python implementation of which.

    It returns the path of an executable or None if it couldn't be found.
    """
    # git-cinnabar.exe doesn't exist, but .exe versions of the other executables
    # do.
    if WINDOWS and name != "git-cinnabar":
        name += ".exe"
    search_dirs = os.environ["PATH"].split(os.pathsep)

    for path in search_dirs:
        test = os.path.join(path, name)
        if os.path.isfile(test) and os.access(test, os.X_OK):
            return test

    return None


def validate_clone_dest(dest):
    dest = os.path.abspath(dest)

    if not os.path.exists(dest):
        return dest

    if not os.path.isdir(dest):
        print("ERROR! Destination %s exists but is not a directory." % dest)
        return None

    if not os.listdir(dest):
        return dest
    else:
        print("ERROR! Destination directory %s exists but is nonempty." % dest)
        return None


def input_clone_dest(vcs, no_interactive):
    repo_name = "mozilla-unified"
    print("Cloning into %s using %s..." % (repo_name, VCS_HUMAN_READABLE[vcs]))
    while True:
        dest = None
        if not no_interactive:
            dest = input(
                "Destination directory for clone (leave empty to use "
                "default destination of %s): " % repo_name
            ).strip()
        if not dest:
            dest = repo_name
        dest = validate_clone_dest(os.path.expanduser(dest))
        if dest:
            return dest
        if no_interactive:
            return None


def hg_clone_firefox(hg, dest):
    # We create an empty repo then modify the config before adding data.
    # This is necessary to ensure storage settings are optimally
    # configured.
    args = [
        hg,
        # The unified repo is generaldelta, so ensure the client is as
        # well.
        "--config",
        "format.generaldelta=true",
        "init",
        dest,
    ]
    res = subprocess.call(args)
    if res:
        print("unable to create destination repo; please try cloning manually")
        return None

    # Strictly speaking, this could overwrite a config based on a template
    # the user has installed. Let's pretend this problem doesn't exist
    # unless someone complains about it.
    with open(os.path.join(dest, ".hg", "hgrc"), "a") as fh:
        fh.write("[paths]\n")
        fh.write("default = https://hg.mozilla.org/mozilla-unified\n")
        fh.write("\n")

        # The server uses aggressivemergedeltas which can blow up delta chain
        # length. This can cause performance to tank due to delta chains being
        # too long. Limit the delta chain length to something reasonable
        # to bound revlog read time.
        fh.write("[format]\n")
        fh.write("# This is necessary to keep performance in check\n")
        fh.write("maxchainlen = 10000\n")

    res = subprocess.call(
        [hg, "pull", "https://hg.mozilla.org/mozilla-unified"], cwd=dest
    )
    print("")
    if res:
        print(CLONE_MERCURIAL_PULL_FAIL % dest)
        return None

    print('updating to "central" - the development head of Gecko and Firefox')
    res = subprocess.call([hg, "update", "-r", "central"], cwd=dest)
    if res:
        print(
            "error updating; you will need to `cd %s && hg update -r central` "
            "manually" % dest
        )
    return dest


def git_clone_firefox(git, dest, watchman):
    tempdir = None
    cinnabar = None
    env = dict(os.environ)
    try:
        cinnabar = which("git-cinnabar")
        if not cinnabar:
            cinnabar_url = (
                "https://github.com/glandium/git-cinnabar/archive/" "master.zip"
            )
            # If git-cinnabar isn't installed already, that's fine; we can
            # download a temporary copy. `mach bootstrap` will clone a full copy
            # of the repo in the state dir; we don't want to copy all that logic
            # to this tiny bootstrapping script.
            tempdir = tempfile.mkdtemp()
            with open(os.path.join(tempdir, "git-cinnabar.zip"), mode="w+b") as archive:
                with urlopen(cinnabar_url) as repo:
                    shutil.copyfileobj(repo, archive)
                archive.seek(0)
                with zipfile.ZipFile(archive) as zipf:
                    zipf.extractall(path=tempdir)
            cinnabar_dir = os.path.join(tempdir, "git-cinnabar-master")
            cinnabar = os.path.join(cinnabar_dir, "git-cinnabar")
            # Make git-cinnabar and git-remote-hg executable.
            st = os.stat(cinnabar)
            os.chmod(cinnabar, st.st_mode | stat.S_IEXEC)
            st = os.stat(os.path.join(cinnabar_dir, "git-remote-hg"))
            os.chmod(
                os.path.join(cinnabar_dir, "git-remote-hg"), st.st_mode | stat.S_IEXEC
            )
            env["PATH"] = cinnabar_dir + os.pathsep + env["PATH"]
            subprocess.check_call(
                ["git", "cinnabar", "download"], cwd=cinnabar_dir, env=env
            )
            print(
                "WARNING! git-cinnabar is required for Firefox development  "
                "with git. After the clone is complete, the bootstrapper "
                "will ask if you would like to configure git; answer yes, "
                "and be sure to add git-cinnabar to your PATH according to "
                "the bootstrapper output."
            )

        # We're guaranteed to have `git-cinnabar` installed now.
        # Configure git per the git-cinnabar requirements.
        subprocess.check_call(
            [
                git,
                "clone",
                "-b",
                "bookmarks/central",
                "hg::https://hg.mozilla.org/mozilla-unified",
                dest,
            ],
            env=env,
        )
        subprocess.check_call([git, "config", "fetch.prune", "true"], cwd=dest, env=env)
        subprocess.check_call([git, "config", "pull.ff", "only"], cwd=dest, env=env)

        watchman_sample = os.path.join(dest, ".git/hooks/fsmonitor-watchman.sample")
        # Older versions of git didn't include fsmonitor-watchman.sample.
        if watchman and os.path.exists(watchman_sample):
            print("Configuring watchman")
            watchman_config = os.path.join(dest, ".git/hooks/query-watchman")
            if not os.path.exists(watchman_config):
                print("Copying %s to %s" % (watchman_sample, watchman_config))
                copy_args = [
                    "cp",
                    ".git/hooks/fsmonitor-watchman.sample",
                    ".git/hooks/query-watchman",
                ]
                subprocess.check_call(copy_args, cwd=dest)

            config_args = [git, "config", "core.fsmonitor", ".git/hooks/query-watchman"]
            subprocess.check_call(config_args, cwd=dest, env=env)
        return dest
    finally:
        if not cinnabar:
            print(
                "Failed to install git-cinnabar. Try performing a manual "
                "installation: https://github.com/glandium/git-cinnabar/wiki/"
                "Mozilla:-A-git-workflow-for-Gecko-development"
            )
        if tempdir:
            shutil.rmtree(tempdir)


def clone(vcs, no_interactive):
    hg = which("hg")
    if not hg:
        print(
            "Mercurial is not installed. Mercurial is required to clone "
            "Firefox%s." % (", even when cloning with Git" if vcs == "git" else "")
        )
        try:
            # We're going to recommend people install the Mercurial package with
            # pip3. That will work if `pip3` installs binaries to a location
            # that's in the PATH, but it might not be. To help out, if we CAN
            # import "mercurial" (in which case it's already been installed),
            # offer that as a solution.
            import mercurial  # noqa: F401

            print(
                "Hint: have you made sure that Mercurial is installed to a "
                "location in your PATH?"
            )
        except ImportError:
            print("Try installing hg with `pip3 install Mercurial`.")
        return None
    if vcs == "hg":
        binary = hg
    else:
        binary = which(vcs)
        if not binary:
            print("Git is not installed.")
            print("Try installing git using your system package manager.")
            return None

    dest = input_clone_dest(vcs, no_interactive)
    if not dest:
        return None

    print("Cloning Firefox %s repository to %s" % (VCS_HUMAN_READABLE[vcs], dest))
    if vcs == "hg":
        return hg_clone_firefox(binary, dest)
    else:
        watchman = which("watchman")
        return git_clone_firefox(binary, dest, watchman)


def bootstrap(srcdir, application_choice, no_interactive, no_system_changes):
    args = [sys.executable, os.path.join(srcdir, "mach")]

    if no_interactive:
        # --no-interactive is a global argument, not a command argument,
        # so it needs to be specified before "bootstrap" is appended.
        args += ["--no-interactive"]

    args += ["bootstrap"]

    if application_choice:
        args += ["--application-choice", application_choice]
    if no_system_changes:
        args += ["--no-system-changes"]

    print("Running `%s`" % " ".join(args))
    return subprocess.call(args, cwd=srcdir)


def main(args):
    parser = OptionParser()
    parser.add_option(
        "--application-choice",
        dest="application_choice",
        help='Pass in an application choice (see "APPLICATIONS" in '
        "python/mozboot/mozboot/bootstrap.py) instead of using the "
        "default interactive prompt.",
    )
    parser.add_option(
        "--vcs",
        dest="vcs",
        default="hg",
        choices=["git", "hg"],
        help="VCS (hg or git) to use for downloading the source code, "
        "instead of using the default interactive prompt.",
    )
    parser.add_option(
        "--no-interactive",
        dest="no_interactive",
        action="store_true",
        help="Answer yes to any (Y/n) interactive prompts.",
    )
    parser.add_option(
        "--no-system-changes",
        dest="no_system_changes",
        action="store_true",
        help="Only executes actions that leave the system " "configuration alone.",
    )

    options, leftover = parser.parse_args(args)

    try:
        srcdir = clone(options.vcs, options.no_interactive)
        if not srcdir:
            return 1
        print("Clone complete.")
        return bootstrap(
            srcdir,
            options.application_choice,
            options.no_interactive,
            options.no_system_changes,
        )
    except Exception:
        print("Could not bootstrap Firefox! Consider filing a bug.")
        raise


if __name__ == "__main__":
    sys.exit(main(sys.argv))
