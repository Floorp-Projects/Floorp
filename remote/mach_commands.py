# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import (
    absolute_import,
    print_function,
    unicode_literals,
)

import argparse
import json
import sys
import os
import re
import tempfile
import shutil
import subprocess
from collections import OrderedDict

from six import iteritems

from mach.decorators import (
    Command,
    CommandArgument,
    CommandProvider,
    SubCommand,
)

from mozbuild.base import (
    MachCommandBase,
    MozbuildObject,
    BinaryNotFoundException,
)
from mozbuild import nodeutil
import mozlog
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
    from mozprocess import processhandler
    env = None
    if kwargs.get("env"):
        env = os.environ.copy()
        env.update(kwargs["env"])

    proc_kwargs = {}
    if "processOutputLine" in kwargs:
        proc_kwargs["processOutputLine"] = kwargs["processOutputLine"]

    p = processhandler.ProcessHandler(cmd="npm",
                                      args=list(args),
                                      cwd=kwargs.get("cwd"),
                                      env=env,
                                      **proc_kwargs)
    if not kwargs.get("wait", True):
        return p

    wait_proc(p, cmd="npm", exit_on_fail=kwargs.get("exit_on_fail", True))

    return p.returncode


def wait_proc(p, cmd=None, exit_on_fail=True):
    try:
        p.run()
        p.wait()
    finally:
        p.kill()
    if exit_on_fail and p.returncode > 0:
        msg = ("%s: exit code %s" % (cmd, p.returncode) if cmd
               else "exit code %s" % p.returncode)
        exit(p.returncode, msg)


class MochaOutputHandler(object):
    def __init__(self, logger, expected):
        self.logger = logger
        self.test_name_re = re.compile(r"\d+\) \[\s*(\w+)\s*\] (.*) \(.*\)")
        self.control_re = re.compile("\x1b\\[[0-9]*m")
        self.proc = None
        self.test_results = OrderedDict()
        self.expected = expected

        self.has_unexpected = False
        self.logger.suite_start([], name="puppeteer-tests")
        self.status_map = {
            "CRASHED": "CRASH",
            "OK": "PASS",
            "TERMINATED": "CRASH",
            "TIME": "TIMEOUT",
        }

    @property
    def pid(self):
        return self.proc and self.proc.pid

    def __call__(self, line):
        line = line.decode("utf-8", "replace")
        line_text = self.control_re.subn("", line)[0]
        m = self.test_name_re.match(line_text)
        if m:
            status, test_name = m.groups()
            status = self.status_map.get(status, status)
            # mozlog doesn't really allow unexpected skip,
            # so if a test is disabled just expect that
            if status == "SKIP":
                expected = ["SKIP"]
            else:
                expected = self.expected.get(test_name, ["PASS"])
            known_intermittent = expected[1:]
            expected_status = expected[0]

            self.test_results[test_name] = status

            self.logger.test_start(test_name)
            self.logger.test_end(test_name,
                                 status=status,
                                 expected=expected_status,
                                 known_intermittent=known_intermittent)

            if status not in expected:
                self.has_unexpected = True
        self.logger.process_output(self.pid, line, command="npm")

    def new_expected(self):
        new_expected = OrderedDict()
        for test_name, status in iteritems(self.test_results):
            if test_name not in self.expected:
                new_status = [status]
            else:
                if status in self.expected[test_name]:
                    new_status = self.expected[test_name]
                else:
                    new_status = [status]
            new_expected[test_name] = new_status
        return new_expected

    def after_end(self, subset=False):
        if not subset:
            missing = set(self.expected) - set(self.test_results)
            if missing:
                self.has_unexpected = True
                for test_name in missing:
                    self.logger.error("TEST-UNEXPECTED-MISSING %s" % (test_name,))
        self.logger.suite_end()


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

        self.remotedir = os.path.join(self.topsrcdir, "remote")
        self.puppeteerdir = os.path.join(self.remotedir, "test", "puppeteer")

    def run_test(self, logger, *tests, **params):
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
        `write_results`:
          Path to write the results json file
        `subset`
          Indicates only a subset of tests are being run, so we should
          skip the check for missing results
        """
        setup()

        binary = params.get("binary") or self.get_binary_path()
        product = params.get("product", "firefox")

        env = {"DUMPIO": "1"}
        extra_options = {}
        for k, v in params.get("extra_launcher_options", {}).items():
            extra_options[k] = json.loads(v)

        if product == "firefox":
            env["BINARY"] = binary
            command = ["run", "funit", "--", "--verbose"]
        elif product == "chrome":
            command = ["run", "unit", "--", "--verbose"]

        if params.get("jobs"):
            env["PPTR_PARALLEL_TESTS"] = str(params["jobs"])
        env["HEADLESS"] = str(params.get("headless", False))

        prefs = {}
        for k, v in params.get("extra_prefs", {}).items():
            prefs[k] = mozprofile.Preferences.cast(v)

        if prefs:
            extra_options["extraPrefsFirefox"] = prefs

        if extra_options:
            env["EXTRA_LAUNCH_OPTIONS"] = json.dumps(extra_options)

        expected_path = os.path.join(os.path.dirname(__file__),
                                     "puppeteer-expected.json")
        if product == "firefox" and os.path.exists(expected_path):
            with open(expected_path) as f:
                expected_data = json.load(f)
        else:
            expected_data = {}

        output_handler = MochaOutputHandler(logger, expected_data)
        proc = npm(*command, cwd=self.puppeteerdir, env=env,
                   processOutputLine=output_handler, wait=False)
        output_handler.proc = proc

        wait_proc(proc, "npm", exit_on_fail=False)

        output_handler.after_end(params.get("subset", False))

        # Non-zero return codes are non-fatal for now since we have some
        # issues with unresolved promises that shouldn't otherwise block
        # running the tests
        if proc.returncode != 0:
            logger.warning("npm exited with code %s" % proc.returncode)

        if params["write_results"]:
            with open(params["write_results"], "w") as f:
                json.dump(output_handler.new_expected(), f, indent=2,
                          separators=(",", ": "))

        if output_handler.has_unexpected:
            exit(1, "Got unexpected results")


def create_parser_puppeteer():
    p = argparse.ArgumentParser()
    p.add_argument("--product",
                   type=str,
                   default="firefox",
                   choices=["chrome", "firefox"])
    p.add_argument("--binary",
                   type=str,
                   help="Path to browser binary.  Defaults to local Firefox build.")
    p.add_argument("--enable-fission",
                   action="store_true",
                   help="Enable Fission (site isolation) in Gecko.")
    p.add_argument("-z", "--headless",
                   action="store_true",
                   help="Run browser in headless mode.")
    p.add_argument("--setpref",
                   action="append",
                   dest="extra_prefs",
                   metavar="<pref>=<value>",
                   help="Defines additional user preferences.")
    p.add_argument("--setopt",
                   action="append",
                   dest="extra_options",
                   metavar="<option>=<value>",
                   help="Defines additional options for `puppeteer.launch`.")
    p.add_argument("-j",
                   dest="jobs",
                   type=int,
                   metavar="<N>",
                   help="Optionally run tests in parallel.")
    p.add_argument("-v",
                   dest="verbosity",
                   action="count",
                   default=0,
                   help="Increase remote agent logging verbosity to include "
                        "debug level messages with -v, trace messages with -vv,"
                        "and to not truncate long trace messages with -vvv")
    p.add_argument("--write-results",
                   action="store",
                   nargs="?",
                   default=None,
                   const=os.path.join(os.path.dirname(__file__),
                                      "puppeteer-expected.json"),
                   help="Path to write updated results to (defaults to the "
                        "expectations file if the argument is provided but "
                        "no path is passed)")
    p.add_argument("--subset",
                   action="store_true",
                   default=False,
                   help="Indicate that only a subset of the tests are running, "
                        "so checks for missing tests should be skipped")
    p.add_argument("tests", nargs="*")
    mozlog.commandline.add_logging_group(p)
    return p


@CommandProvider
class PuppeteerTest(MachCommandBase):
    @Command("puppeteer-test", category="testing",
             description="Run Puppeteer unit tests.",
             parser=create_parser_puppeteer)
    def puppeteer_test(self, binary=None, enable_fission=False, headless=False,
                       extra_prefs=None, extra_options=None, jobs=1, verbosity=0,
                       tests=None, product="firefox", write_results=None,
                       subset=False, **kwargs):

        logger = mozlog.commandline.setup_logging("puppeteer-test",
                                                  kwargs,
                                                  {"mach": sys.stdout})

        # moztest calls this programmatically with test objects or manifests
        if "test_objects" in kwargs and tests is not None:
            logger.error("Expected either 'test_objects' or 'tests'")
            exit(1)

        if product != "firefox" and extra_prefs is not None:
            logger.error("User preferences are not recognized by %s" % product)
            exit(1)

        if "test_objects" in kwargs:
            tests = []
            for test in kwargs["test_objects"]:
                tests.append(test["path"])

        prefs = {}
        for s in (extra_prefs or []):
            kv = s.split("=")
            if len(kv) != 2:
                logger.error("syntax error in --setpref={}".format(s))
                exit(EX_USAGE)
            prefs[kv[0]] = kv[1].strip()

        options = {}
        for s in (extra_options or []):
            kv = s.split("=")
            if len(kv) != 2:
                logger.error("syntax error in --setopt={}".format(s))
                exit(EX_USAGE)
            options[kv[0]] = kv[1].strip()

        if enable_fission:
            prefs.update({"fission.autostart": True,
                          "dom.serviceWorkers.parent_intercept": True,
                          "browser.tabs.documentchannel": True})

        if verbosity == 1:
            prefs["remote.log.level"] = "Debug"
        elif verbosity > 1:
            prefs["remote.log.level"] = "Trace"
        if verbosity > 2:
            prefs["remote.log.truncate"] = False

        self.install_puppeteer(product)

        params = {"binary": binary,
                  "headless": headless,
                  "extra_prefs": prefs,
                  "product": product,
                  "jobs": jobs,
                  "extra_launcher_options": options,
                  "write_results": write_results,
                  "subset": subset}
        puppeteer = self._spawn(PuppeteerRunner)
        try:
            return puppeteer.run_test(logger, *tests, **params)
        except BinaryNotFoundException as e:
            logger.error(e)
            logger.info(e.help())
            exit(1)
        except Exception as e:
            exit(EX_SOFTWARE, e)

    def install_puppeteer(self, product):
        setup()
        env = {}
        if product != "chrome":
            env["PUPPETEER_SKIP_CHROMIUM_DOWNLOAD"] = "1"
        npm("install",
            cwd=os.path.join(self.topsrcdir, "remote", "test", "puppeteer"),
            env=env)


def exit(code, error=None):
    if error is not None:
        if isinstance(error, Exception):
            import traceback
            traceback.print_exc()
        else:
            message = str(error).split("\n")[0].strip()
            print("{}: {}".format(sys.argv[0], message), file=sys.stderr)
    sys.exit(code)
