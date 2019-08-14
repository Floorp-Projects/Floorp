# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import (
    absolute_import,
    print_function,
    unicode_literals,
)

import sys
import os
import tempfile
import shutil
import subprocess

from mach.decorators import (
    Command,
    CommandArgument,
    CommandProvider,
    SubCommand,
)

from mozbuild.base import MachCommandBase


EX_CONFIG = 78
EX_SOFTWARE = 70
EX_USAGE = 64

DEFAULT_REPO = "https://github.com/andreastt/puppeteer.git"
DEFAULT_COMMITISH = "firefox"


@CommandProvider
class RemoteCommands(MachCommandBase):
    def __init__(self, context):
        MachCommandBase.__init__(self, context)
        self.remotedir = os.path.join(self.topsrcdir, "remote")

    @Command("remote", category="misc",
             description="Remote protocol related operations.")
    def remote(self):
        self.parser.print_usage()
        exit(EX_USAGE)

    @SubCommand("remote", "vendor-puppeteer",
                "Pull in latest changes of the Puppeteer client.")
    @CommandArgument("--repository",
                     metavar="REPO",
                     default=DEFAULT_REPO,
                     help="The (possibly remote) repository to clone from. "
                          "Defaults to {}.".format(DEFAULT_REPO))
    @CommandArgument("--commitish",
                     metavar="COMMITISH",
                     default=DEFAULT_COMMITISH,
                     help="The commit or tag object name to check out. "
                          "Defaults to \"{}\".".format(DEFAULT_COMMITISH))
    def vendor_puppeteer(self, repository, commitish):
        puppeteerdir = os.path.join(self.remotedir, "test", "puppeteer")

        shutil.rmtree(puppeteerdir, ignore_errors=True)
        os.makedirs(puppeteerdir)
        with TemporaryDirectory() as tmpdir:
            git("clone", "-q", repository, tmpdir)
            git("checkout", commitish, worktree=tmpdir)
            git("checkout-index", "-a", "-f",
                "--prefix", "{}/".format(puppeteerdir),
                worktree=tmpdir)

        # remove files which may interfere with git checkout of central
        try:
            os.remove(os.path.join(puppeteerdir, ".gitattributes"))
            os.remove(os.path.join(puppeteerdir, ".gitignore"))
        except OSError:
            pass

        import yaml
        annotation = {
            "schema": 1,
            "bugzilla": {
                "product": "Remote Protocol",
                "component": "Agent",
            },
            "origin": {
                "name": "puppeteer",
                "description": "Headless Chrome Node API",
                "url": repository,
                "license": "Apache-2.0",
                "release": commitish,
            },
        }
        with open(os.path.join(puppeteerdir, "moz.yaml"), "w") as fh:
            yaml.safe_dump(annotation, fh,
                           default_flow_style=False,
                           encoding="utf-8",
                           allow_unicode=True)


def git(*args, **kwargs):
    cmd = ("git",)
    if kwargs.get("worktree"):
        cmd += ("-C", kwargs["worktree"])
    cmd += args

    pipe = kwargs.get("pipe")
    git_p = subprocess.Popen(cmd,
                             env={"GIT_CONFIG_NOSYSTEM": "1"},
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
    pipe_p = None
    if pipe:
        pipe_p = subprocess.Popen(pipe, stdin=git_p.stdout, stderr=subprocess.PIPE)

    if pipe:
        _, pipe_err = pipe_p.communicate()
    out, git_err = git_p.communicate()

    # use error from first program that failed
    if git_p.returncode > 0:
        exit(EX_SOFTWARE, git_err)
    if pipe and pipe_p.returncode > 0:
        exit(EX_SOFTWARE, pipe_err)

    return out


# tempfile.TemporaryDirectory missing from Python 2.7
class TemporaryDirectory(object):
    def __init__(self):
        self.path = tempfile.mkdtemp()
        self._closed = False

    def __repr__(self):
        return "<{} {!r}>".format(self.__class__.__name__, self.path)

    def __enter__(self):
        return self.path

    def __exit__(self, exc, value, tb):
        self.clean()

    def __del__(self):
        self.clean()

    def clean(self):
        if self.path and not self._closed:
            shutil.rmtree(self.path)
            self._closed = True


def exit(code, error=None):
    if error is not None:
        if isinstance(error, Exception):
            import traceback
            traceback.print_exc()
        else:
            message = str(error).split("\n")[0].strip()
            print("{}: {}".format(sys.argv[0], message), file=sys.stderr)
    sys.exit(code)
