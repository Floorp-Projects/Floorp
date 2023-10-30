# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
from collections import OrderedDict

import mozlog
import mozprofile
from mach.decorators import Command, CommandArgument, SubCommand
from mozbuild import nodeutil
from mozbuild.base import BinaryNotFoundException, MozbuildObject

EX_CONFIG = 78
EX_SOFTWARE = 70
EX_USAGE = 64


def setup():
    # add node and npm from mozbuild to front of system path
    npm, _ = nodeutil.find_npm_executable()
    if not npm:
        exit(EX_CONFIG, "could not find npm executable")
    path = os.path.abspath(os.path.join(npm, os.pardir))
    os.environ["PATH"] = "{}{}{}".format(path, os.pathsep, os.environ["PATH"])


def remotedir(command_context):
    return os.path.join(command_context.topsrcdir, "remote")


@Command("remote", category="misc", description="Remote protocol related operations.")
def remote(command_context):
    """The remote subcommands all relate to the remote protocol."""
    command_context._sub_mach(["help", "remote"])
    return 1


@SubCommand(
    "remote", "vendor-puppeteer", "Pull in latest changes of the Puppeteer client."
)
@CommandArgument(
    "--repository",
    metavar="REPO",
    default="https://github.com/puppeteer/puppeteer.git",
    help="The (possibly local) repository to clone from.",
)
@CommandArgument(
    "--commitish",
    metavar="COMMITISH",
    required=True,
    help="The commit or tag object name to check out.",
)
@CommandArgument(
    "--no-install",
    dest="install",
    action="store_false",
    default=True,
    help="Do not install the just-pulled Puppeteer package,",
)
def vendor_puppeteer(command_context, repository, commitish, install):
    puppeteer_dir = os.path.join(remotedir(command_context), "test", "puppeteer")

    # Preserve our custom mocha reporter
    shutil.move(
        os.path.join(puppeteer_dir, "json-mocha-reporter.js"),
        remotedir(command_context),
    )
    shutil.rmtree(puppeteer_dir, ignore_errors=True)
    os.makedirs(puppeteer_dir)
    with TemporaryDirectory() as tmpdir:
        git("clone", "-q", repository, tmpdir)
        git("checkout", commitish, worktree=tmpdir)
        git(
            "checkout-index",
            "-a",
            "-f",
            "--prefix",
            "{}/".format(puppeteer_dir),
            worktree=tmpdir,
        )

    # remove files which may interfere with git checkout of central
    try:
        os.remove(os.path.join(puppeteer_dir, ".gitattributes"))
        os.remove(os.path.join(puppeteer_dir, ".gitignore"))
    except OSError:
        pass

    unwanted_dirs = ["experimental", "docs"]

    for dir in unwanted_dirs:
        dir_path = os.path.join(puppeteer_dir, dir)
        if os.path.isdir(dir_path):
            shutil.rmtree(dir_path)

    shutil.move(
        os.path.join(remotedir(command_context), "json-mocha-reporter.js"),
        puppeteer_dir,
    )

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
    with open(os.path.join(puppeteer_dir, "moz.yaml"), "w") as fh:
        yaml.safe_dump(
            annotation,
            fh,
            default_flow_style=False,
            encoding="utf-8",
            allow_unicode=True,
        )

    if install:
        env = {"HUSKY": "0", "PUPPETEER_SKIP_DOWNLOAD": "1"}
        run_npm(
            "install",
            cwd=os.path.join(command_context.topsrcdir, puppeteer_dir),
            env=env,
        )


def git(*args, **kwargs):
    cmd = ("git",)
    if kwargs.get("worktree"):
        cmd += ("-C", kwargs["worktree"])
    cmd += args

    pipe = kwargs.get("pipe")
    git_p = subprocess.Popen(
        cmd,
        env={"GIT_CONFIG_NOSYSTEM": "1"},
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
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


def run_npm(*args, **kwargs):
    from mozprocess import run_and_wait

    def output_timeout_handler(proc):
        # In some cases, we wait longer for a mocha timeout
        print(
            "Timed out after {} seconds of no output".format(kwargs["output_timeout"])
        )

    env = None
    npm, _ = nodeutil.find_npm_executable()
    if kwargs.get("env"):
        env = os.environ.copy()
        env.update(kwargs["env"])

    proc_kwargs = {"output_timeout_handler": output_timeout_handler}
    for kw in ["output_line_handler", "output_timeout"]:
        if kw in kwargs:
            proc_kwargs[kw] = kwargs[kw]

    cmd = [npm]
    cmd.extend(list(args))

    p = run_and_wait(
        args=cmd,
        cwd=kwargs.get("cwd"),
        env=env,
        text=True,
        **proc_kwargs,
    )
    post_wait_proc(p, cmd=npm, exit_on_fail=kwargs.get("exit_on_fail", True))

    return p.returncode


def post_wait_proc(p, cmd=None, exit_on_fail=True):
    if p.poll() is None:
        p.kill()
    if exit_on_fail and p.returncode > 0:
        msg = (
            "%s: exit code %s" % (cmd, p.returncode)
            if cmd
            else "exit code %s" % p.returncode
        )
        exit(p.returncode, msg)


class MochaOutputHandler(object):
    def __init__(self, logger, expected):
        self.hook_re = re.compile('"before\b?.*" hook|"after\b?.*" hook')

        self.logger = logger
        self.proc = None
        self.test_results = OrderedDict()
        self.expected = expected
        self.unexpected_skips = set()

        self.has_unexpected = False
        self.logger.suite_start([], name="puppeteer-tests")
        self.status_map = {
            "CRASHED": "CRASH",
            "OK": "PASS",
            "TERMINATED": "CRASH",
            "pass": "PASS",
            "fail": "FAIL",
            "pending": "SKIP",
        }

    @property
    def pid(self):
        return self.proc and self.proc.pid

    def __call__(self, proc, line):
        self.proc = proc
        line = line.rstrip("\r\n")
        event = None
        try:
            if line.startswith("[") and line.endswith("]"):
                event = json.loads(line)
            self.process_event(event)
        except ValueError:
            pass
        finally:
            self.logger.process_output(self.pid, line, command="npm")

    def testExpectation(self, testIdPattern, expected_name):
        if testIdPattern.find("*") == -1:
            return expected_name == testIdPattern
        else:
            return re.compile(re.escape(testIdPattern).replace(r"\*", ".*")).search(
                expected_name
            )

    def process_event(self, event):
        if isinstance(event, list) and len(event) > 1:
            status = self.status_map.get(event[0])
            test_start = event[0] == "test-start"
            if not status and not test_start:
                return
            test_info = event[1]
            test_full_title = test_info.get("fullTitle", "")
            test_name = test_full_title
            test_path = test_info.get("file", "")
            test_file_name = os.path.basename(test_path).replace(".js", "")
            test_err = test_info.get("err")
            if status == "FAIL" and test_err:
                if "timeout" in test_err.lower():
                    status = "TIMEOUT"
            if test_name and test_path:
                test_name = "{} ({})".format(test_name, os.path.basename(test_path))
            # mocha hook failures are not tracked in metadata
            if status != "PASS" and self.hook_re.search(test_name):
                self.logger.error("TEST-UNEXPECTED-ERROR %s" % (test_name,))
                return
            if test_start:
                self.logger.test_start(test_name)
                return
            expected_name = "[{}] {}".format(test_file_name, test_full_title)
            expected_item = next(
                (
                    expectation
                    for expectation in reversed(list(self.expected))
                    if self.testExpectation(expectation["testIdPattern"], expected_name)
                ),
                None,
            )
            if expected_item is None:
                expected = ["PASS"]
            else:
                expected = expected_item["expectations"]
            # mozlog doesn't really allow unexpected skip,
            # so if a test is disabled just expect that and note the unexpected skip
            # Also, mocha doesn't log test-start for skipped tests
            if status == "SKIP":
                self.logger.test_start(test_name)
                if self.expected and status not in expected:
                    self.unexpected_skips.add(test_name)
                expected = ["SKIP"]
            known_intermittent = expected[1:]
            expected_status = expected[0]

            # check if we've seen a result for this test before this log line
            result_recorded = self.test_results.get(test_name)
            if result_recorded:
                self.logger.warning(
                    "Received a second status for {}: "
                    "first {}, now {}".format(test_name, result_recorded, status)
                )
            # mocha intermittently logs an additional test result after the
            # test has already timed out. Avoid recording this second status.
            if result_recorded != "TIMEOUT":
                self.test_results[test_name] = status
                if status not in expected:
                    self.has_unexpected = True
            self.logger.test_end(
                test_name,
                status=status,
                expected=expected_status,
                known_intermittent=known_intermittent,
            )

    def after_end(self):
        if self.unexpected_skips:
            self.has_unexpected = True
            for test_name in self.unexpected_skips:
                self.logger.error(
                    "TEST-UNEXPECTED-MISSING Unexpected skipped %s" % (test_name,)
                )
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
        self.puppeteer_dir = os.path.join(self.remotedir, "test", "puppeteer")

    def run_test(self, logger, *tests, **params):
        """
        Runs Puppeteer unit tests with npm.

        Possible optional test parameters:

        `bidi`:
          Boolean to indicate whether to test Firefox with BiDi protocol.
        `binary`:
          Path for the browser binary to use.  Defaults to the local
          build.
        `headless`:
          Boolean to indicate whether to activate Firefox' headless mode.
        `extra_prefs`:
          Dictionary of extra preferences to write to the profile,
          before invoking npm.  Overrides default preferences.
        `enable_webrender`:
          Boolean to indicate whether to enable WebRender compositor in Gecko.
        """
        setup()

        with_bidi = params.get("bidi", False)
        binary = params.get("binary") or self.get_binary_path()
        product = params.get("product", "firefox")

        env = {
            # Print browser process ouptut
            "DUMPIO": "1",
            # Checked by Puppeteer's custom mocha config
            "CI": "1",
            # Causes some tests to be skipped due to assumptions about install
            "PUPPETEER_ALT_INSTALL": "1",
        }
        extra_options = {}
        for k, v in params.get("extra_launcher_options", {}).items():
            extra_options[k] = json.loads(v)

        # Override upstream defaults: no retries, shorter timeout
        mocha_options = [
            "--reporter",
            "./json-mocha-reporter.js",
            "--retries",
            "0",
            "--fullTrace",
            "--timeout",
            "20000",
            "--no-parallel",
            "--no-coverage",
        ]
        env["HEADLESS"] = str(params.get("headless", False))
        test_command = "test:" + product

        if product == "firefox":
            env["BINARY"] = binary
            env["PUPPETEER_PRODUCT"] = "firefox"
            env["MOZ_WEBRENDER"] = "%d" % params.get("enable_webrender", False)
        else:
            env["PUPPETEER_CACHE_DIR"] = os.path.join(
                self.topobjdir,
                "_tests",
                "remote",
                "test",
                "puppeteer",
                ".cache",
            )

        if with_bidi is True:
            test_command = test_command + ":bidi"
        elif env["HEADLESS"] == "True":
            test_command = test_command + ":headless"
        else:
            test_command = test_command + ":headful"

        command = ["run", test_command, "--"] + mocha_options

        prefs = {}
        for k, v in params.get("extra_prefs", {}).items():
            print("Using extra preference: {}={}".format(k, v))
            prefs[k] = mozprofile.Preferences.cast(v)

        if prefs:
            extra_options["extraPrefsFirefox"] = prefs

        if extra_options:
            env["EXTRA_LAUNCH_OPTIONS"] = json.dumps(extra_options)

        expected_path = os.path.join(
            os.path.dirname(__file__),
            "test",
            "puppeteer",
            "test",
            "TestExpectations.json",
        )
        if os.path.exists(expected_path):
            with open(expected_path) as f:
                expected_data = json.load(f)
        else:
            expected_data = []

        expected_platform = platform.uname().system.lower()
        if expected_platform == "windows":
            expected_platform = "win32"

        # Filter expectation data for the selected browser,
        # headless or headful mode, the operating system,
        # run in BiDi mode or not.
        expectations = [
            expectation
            for expectation in expected_data
            if is_relevant_expectation(
                expectation, product, with_bidi, env["HEADLESS"], expected_platform
            )
        ]

        output_handler = MochaOutputHandler(logger, expectations)
        return_code = run_npm(
            *command,
            cwd=self.puppeteer_dir,
            env=env,
            output_line_handler=output_handler,
            # Puppeteer unit tests don't always clean-up child processes in case of
            # failure, so use an output_timeout as a fallback
            output_timeout=60,
            exit_on_fail=False,
        )

        output_handler.after_end()

        # Non-zero return codes are non-fatal for now since we have some
        # issues with unresolved promises that shouldn't otherwise block
        # running the tests
        if return_code != 0:
            logger.warning("npm exited with code %s" % return_code)

        if output_handler.has_unexpected:
            exit(1, "Got unexpected results")


def create_parser_puppeteer():
    p = argparse.ArgumentParser()
    p.add_argument(
        "--product", type=str, default="firefox", choices=["chrome", "firefox"]
    )
    p.add_argument(
        "--bidi",
        action="store_true",
        help="Flag that indicates whether to test Firefox with BiDi protocol.",
    )
    p.add_argument(
        "--binary",
        type=str,
        help="Path to browser binary.  Defaults to local Firefox build.",
    )
    p.add_argument(
        "--ci",
        action="store_true",
        help="Flag that indicates that tests run in a CI environment.",
    )
    p.add_argument(
        "--disable-fission",
        action="store_true",
        default=False,
        dest="disable_fission",
        help="Disable Fission (site isolation) in Gecko.",
    )
    p.add_argument(
        "--enable-webrender",
        action="store_true",
        help="Enable the WebRender compositor in Gecko.",
    )
    p.add_argument(
        "-z", "--headless", action="store_true", help="Run browser in headless mode."
    )
    p.add_argument(
        "--setpref",
        action="append",
        dest="extra_prefs",
        metavar="<pref>=<value>",
        help="Defines additional user preferences.",
    )
    p.add_argument(
        "--setopt",
        action="append",
        dest="extra_options",
        metavar="<option>=<value>",
        help="Defines additional options for `puppeteer.launch`.",
    )
    p.add_argument(
        "-v",
        dest="verbosity",
        action="count",
        default=0,
        help="Increase remote agent logging verbosity to include "
        "debug level messages with -v, trace messages with -vv,"
        "and to not truncate long trace messages with -vvv",
    )
    p.add_argument("tests", nargs="*")
    mozlog.commandline.add_logging_group(p)
    return p


def is_relevant_expectation(
    expectation, expected_product, with_bidi, is_headless, expected_platform
):
    parameters = expectation["parameters"]

    if expected_product == "firefox":
        is_expected_product = "chrome" not in parameters
    else:
        is_expected_product = "firefox" not in parameters

    if with_bidi is True:
        is_expected_protocol = "cdp" not in parameters
        is_headless = "True"
    else:
        is_expected_protocol = "webDriverBiDi" not in parameters

    if is_headless == "True":
        is_expected_mode = "headful" not in parameters
    else:
        is_expected_mode = "headless" not in parameters

    is_expected_platform = expected_platform in expectation["platforms"]

    return (
        is_expected_product
        and is_expected_protocol
        and is_expected_mode
        and is_expected_platform
    )


@Command(
    "puppeteer-test",
    category="testing",
    description="Run Puppeteer unit tests.",
    parser=create_parser_puppeteer,
)
@CommandArgument(
    "--no-install",
    dest="install",
    action="store_false",
    default=True,
    help="Do not install the Puppeteer package",
)
def puppeteer_test(
    command_context,
    bidi=None,
    binary=None,
    ci=False,
    disable_fission=False,
    enable_webrender=False,
    headless=False,
    extra_prefs=None,
    extra_options=None,
    install=False,
    verbosity=0,
    tests=None,
    product="firefox",
    **kwargs,
):
    logger = mozlog.commandline.setup_logging(
        "puppeteer-test", kwargs, {"mach": sys.stdout}
    )

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
    for s in extra_prefs or []:
        kv = s.split("=")
        if len(kv) != 2:
            logger.error("syntax error in --setpref={}".format(s))
            exit(EX_USAGE)
        prefs[kv[0]] = kv[1].strip()

    options = {}
    for s in extra_options or []:
        kv = s.split("=")
        if len(kv) != 2:
            logger.error("syntax error in --setopt={}".format(s))
            exit(EX_USAGE)
        options[kv[0]] = kv[1].strip()

    prefs.update({"fission.autostart": True})
    if disable_fission:
        prefs.update({"fission.autostart": False})

    if verbosity == 1:
        prefs["remote.log.level"] = "Debug"
    elif verbosity > 1:
        prefs["remote.log.level"] = "Trace"
    if verbosity > 2:
        prefs["remote.log.truncate"] = False

    if install:
        install_puppeteer(command_context, product, ci)

    params = {
        "bidi": bidi,
        "binary": binary,
        "headless": headless,
        "enable_webrender": enable_webrender,
        "extra_prefs": prefs,
        "product": product,
        "extra_launcher_options": options,
    }
    puppeteer = command_context._spawn(PuppeteerRunner)
    try:
        return puppeteer.run_test(logger, *tests, **params)
    except BinaryNotFoundException as e:
        logger.error(e)
        logger.info(e.help())
        exit(1)
    except Exception as e:
        exit(EX_SOFTWARE, e)


def install_puppeteer(command_context, product, ci):
    setup()
    env = {"HUSKY": "0"}

    puppeteer_dir = os.path.join("remote", "test", "puppeteer")
    puppeteer_dir_full_path = os.path.join(command_context.topsrcdir, puppeteer_dir)
    puppeteer_test_dir = os.path.join(puppeteer_dir, "test")

    if product == "chrome":
        env["PUPPETEER_CACHE_DIR"] = os.path.join(
            command_context.topobjdir, "_tests", puppeteer_dir, ".cache"
        )
    else:
        env["PUPPETEER_SKIP_DOWNLOAD"] = "1"

    if not ci:
        run_npm(
            "run",
            "clean",
            cwd=puppeteer_dir_full_path,
            env=env,
            exit_on_fail=False,
        )

    command = "ci" if ci else "install"
    run_npm(command, cwd=puppeteer_dir_full_path, env=env)
    run_npm(
        "run",
        "build",
        cwd=os.path.join(command_context.topsrcdir, puppeteer_test_dir),
        env=env,
    )


def exit(code, error=None):
    if error is not None:
        if isinstance(error, Exception):
            import traceback

            traceback.print_exc()
        else:
            message = str(error).split("\n")[0].strip()
            print("{}: {}".format(sys.argv[0], message), file=sys.stderr)
    sys.exit(code)
