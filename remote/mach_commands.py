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

from mozbuild.base import (
    MachCommandBase,
    MozbuildObject,
)
from mozbuild import nodeutil
import mozprofile


EX_CONFIG = 78
EX_SOFTWARE = 70
EX_USAGE = 64

DEFAULT_REPO = "https://github.com/andreastt/puppeteer.git"
DEFAULT_COMMITISH = "firefox"


def setup():
    # add node and npm from mozbuild to front of system path
    npm, _ = nodeutil.find_npm_executable()
    if not npm:
        exit(EX_CONFIG, "could not find npm executable")
    path = os.path.abspath(os.path.join(npm, os.pardir))
    os.environ["PATH"] = "{}:{}".format(path, os.environ["PATH"])


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


def npm(*args, **kwargs):
    env = None
    if kwargs.get("env"):
        env = os.environ.copy()
        env.update(kwargs["env"])

    p = subprocess.Popen(("npm",) + args,
                         cwd=kwargs.get("cwd"),
                         env=env)

    p.wait()
    if p.returncode > 0:
        exit(p.returncode, "npm: exit code {}".format(p.returncode))


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


class PuppeteerRunner(MozbuildObject):
    def __init__(self, *args, **kwargs):
        super(PuppeteerRunner, self).__init__(*args, **kwargs)

        self.profile = mozprofile.Profile()

        self.remotedir = os.path.join(self.topsrcdir, "remote")
        self.puppeteerdir = os.path.join(self.remotedir, "test", "puppeteer")

    def run_test(self, *tests, **params):
        """
        Runs Puppeteer unit tests with npm.

        Possible optional test parameters:

        `binary`:
          Path for the browser binary to use.  Defaults to the local
          build.
        `jobs`:
          Number of tests to run in parallel.  Defaults to not
          parallelise, e.g. `-j1`.
        `headless`:
          Boolean to indicate whether to activate Firefox' headless mode.
        `extra_prefs`:
          Dictionary of extra preferences to write to the profile,
          before invoking npm.  Overrides default preferences.
        """
        setup()

        binary = params.get("binary") or self.get_binary_path()

        # currently runs against puppeteer-chrome
        # but future intention is to run against puppeteer-firefox
        # when it targets the Mozilla remote agent instead of Juggler
        env = {"CHROME": binary,
               "DUMPIO": "1"}

        if params.get("jobs"):
            env["PPTR_PARALLEL_TESTS"] = str(params["jobs"])

        if params.get("headless"):
            env["MOZ_HEADLESS"] = "1"

        prefs = params.get("extra_prefs", {})
        for k, v in params.get("extra_prefs", {}).items():
            prefs[k] = mozprofile.Preferences.cast(v)

        prefs.update({
            # https://bugzilla.mozilla.org/show_bug.cgi?id=1544393
            "remote.enabled": True,
            # https://bugzilla.mozilla.org/show_bug.cgi?id=1543115
            "browser.dom.window.dump.enabled": True,
        })

        self.profile.set_preferences(prefs)

        # PROFILE is a Puppeteer workaround (see ab302d6)
        # for passing the --profile flag to Firefox
        env["PROFILE"] = self.profile.profile

        return npm("run", "unit", "--verbose", *tests,
                   cwd=self.puppeteerdir,
                   env=env)


@CommandProvider
class PuppeteerTest(MachCommandBase):
    @Command("puppeteer-test", category="testing",
             description="Run Puppeteer unit tests.")
    @CommandArgument("--binary",
                     type=str,
                     help="Path to Firefox binary.  Defaults to local build.")
    @CommandArgument("-z", "--headless",
                     action="store_true",
                     help="Run browser in headless mode (default).")
    @CommandArgument("--setpref",
                     action="append",
                     dest="extra_prefs",
                     metavar="<pref>=<value>",
                     help="Defines additional user preferences.")
    @CommandArgument("-j",
                     dest="jobs",
                     type=int,
                     metavar="<N>",
                     help="Optionally run tests in parallel.")
    @CommandArgument("tests", nargs="*")
    def puppeteer_test(self, binary=None, headless=True, extra_prefs=None,
                       jobs=1, tests=None, **kwargs):
        # moztest calls this programmatically with test objects or manifests
        if "test_objects" in kwargs and tests is not None:
            raise ValueError("Expected either 'test_objects' or 'tests'")

        if "test_objects" in kwargs:
            tests = []
            for test in kwargs["test_objects"]:
                tests.append(test["path"])

        prefs = {}
        for s in (extra_prefs or []):
            kv = s.split("=")
            if len(kv) != 2:
                exit(EX_USAGE, "syntax error in --setpref={}".format(s))
            prefs[kv[0]] = kv[1].strip()

        self.install_puppeteer()

        params = {"binary": binary,
                  "headless": headless,
                  "extra_prefs": prefs,
                  "jobs": jobs}
        puppeteer = self._spawn(PuppeteerRunner)
        try:
            return puppeteer.run_test(*tests, **params)
        except Exception as e:
            exit(EX_SOFTWARE, e)

    def install_puppeteer(self):
        setup()
        npm("install",
            cwd=os.path.join(self.topsrcdir, "remote", "test", "puppeteer"),
            env={"PUPPETEER_SKIP_CHROMIUM_DOWNLOAD": "1"})


def exit(code, error=None):
    if error is not None:
        if isinstance(error, Exception):
            import traceback
            traceback.print_exc()
        else:
            message = str(error).split("\n")[0].strip()
            print("{}: {}".format(sys.argv[0], message), file=sys.stderr)
    sys.exit(code)
