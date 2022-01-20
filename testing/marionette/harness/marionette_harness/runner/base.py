# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import json
import os
import random
import re
import socket
import sys
import time
import traceback
import unittest

from argparse import ArgumentParser
from collections import defaultdict
from copy import deepcopy

import six

import mozinfo
import moznetwork
import mozprofile
import mozversion

from manifestparser import TestManifest
from manifestparser.filters import tags
from marionette_driver.marionette import Marionette
from moztest.adapters.unit import StructuredTestResult, StructuredTestRunner
from moztest.results import TestResult, TestResultCollection, relevant_line

from six import reraise, MAXSIZE

from . import serve


here = os.path.abspath(os.path.dirname(__file__))


def update_mozinfo(path=None):
    """Walk up directories to find mozinfo.json and update the info."""
    path = path or here
    dirs = set()
    while path != os.path.expanduser("~"):
        if path in dirs:
            break
        dirs.add(path)
        path = os.path.split(path)[0]

    return mozinfo.find_and_update_from_json(*dirs)


class MarionetteTest(TestResult):
    @property
    def test_name(self):
        if self.test_class is not None:
            return "{0}.py {1}.{2}".format(
                self.test_class.split(".")[0], self.test_class, self.name
            )
        else:
            return self.name


class MarionetteTestResult(StructuredTestResult, TestResultCollection):

    resultClass = MarionetteTest

    def __init__(self, *args, **kwargs):
        self.marionette = kwargs.pop("marionette")
        TestResultCollection.__init__(self, "MarionetteTest")
        self.passed = 0
        self.testsRun = 0
        self.result_modifiers = []  # used by mixins to modify the result
        StructuredTestResult.__init__(self, *args, **kwargs)

    @property
    def skipped(self):
        return [t for t in self if t.result == "SKIPPED"]

    @skipped.setter
    def skipped(self, value):
        pass

    @property
    def expectedFailures(self):
        return [t for t in self if t.result == "KNOWN-FAIL"]

    @expectedFailures.setter
    def expectedFailures(self, value):
        pass

    @property
    def unexpectedSuccesses(self):
        return [t for t in self if t.result == "UNEXPECTED-PASS"]

    @unexpectedSuccesses.setter
    def unexpectedSuccesses(self, value):
        pass

    @property
    def tests_passed(self):
        return [t for t in self if t.result == "PASS"]

    @property
    def errors(self):
        return [t for t in self if t.result == "ERROR"]

    @errors.setter
    def errors(self, value):
        pass

    @property
    def failures(self):
        return [t for t in self if t.result == "UNEXPECTED-FAIL"]

    @failures.setter
    def failures(self, value):
        pass

    @property
    def duration(self):
        if self.stop_time:
            return self.stop_time - self.start_time
        else:
            return 0

    def add_test_result(
        self,
        test,
        result_expected="PASS",
        result_actual="PASS",
        output="",
        context=None,
        **kwargs
    ):
        def get_class(test):
            return test.__class__.__module__ + "." + test.__class__.__name__

        name = str(test).split()[0]
        test_class = get_class(test)
        if hasattr(test, "jsFile"):
            name = os.path.basename(test.jsFile)
            test_class = None

        t = self.resultClass(
            name=name,
            test_class=test_class,
            time_start=test.start_time,
            result_expected=result_expected,
            context=context,
            **kwargs
        )
        # call any registered result modifiers
        for modifier in self.result_modifiers:
            result_expected, result_actual, output, context = modifier(
                t, result_expected, result_actual, output, context
            )
        t.finish(
            result_actual,
            time_end=time.time() if test.start_time else 0,
            reason=relevant_line(output),
            output=output,
        )
        self.append(t)

    def addError(self, test, err):
        self.add_test_result(
            test, output=self._exc_info_to_string(err, test), result_actual="ERROR"
        )
        super(MarionetteTestResult, self).addError(test, err)

    def addFailure(self, test, err):
        self.add_test_result(
            test,
            output=self._exc_info_to_string(err, test),
            result_actual="UNEXPECTED-FAIL",
        )
        super(MarionetteTestResult, self).addFailure(test, err)

    def addSuccess(self, test):
        self.passed += 1
        self.add_test_result(test, result_actual="PASS")
        super(MarionetteTestResult, self).addSuccess(test)

    def addExpectedFailure(self, test, err):
        """Called when an expected failure/error occured."""
        self.add_test_result(
            test, output=self._exc_info_to_string(err, test), result_actual="KNOWN-FAIL"
        )
        super(MarionetteTestResult, self).addExpectedFailure(test, err)

    def addUnexpectedSuccess(self, test):
        """Called when a test was expected to fail, but succeed."""
        self.add_test_result(test, result_actual="UNEXPECTED-PASS")
        super(MarionetteTestResult, self).addUnexpectedSuccess(test)

    def addSkip(self, test, reason):
        self.add_test_result(test, output=reason, result_actual="SKIPPED")
        super(MarionetteTestResult, self).addSkip(test, reason)

    def getInfo(self, test):
        return test.test_name

    def getDescription(self, test):
        doc_first_line = test.shortDescription()
        if self.descriptions and doc_first_line:
            return "\n".join((str(test), doc_first_line))
        else:
            desc = str(test)
            return desc

    def printLogs(self, test):
        for testcase in test._tests:
            if hasattr(testcase, "loglines") and testcase.loglines:
                # Don't dump loglines to the console if they only contain
                # TEST-START and TEST-END.
                skip_log = True
                for line in testcase.loglines:
                    str_line = " ".join(line)
                    if "TEST-END" not in str_line and "TEST-START" not in str_line:
                        skip_log = False
                        break
                if skip_log:
                    return
                self.logger.info("START LOG:")
                for line in testcase.loglines:
                    self.logger.info(" ".join(line).encode("ascii", "replace"))
                self.logger.info("END LOG:")

    def stopTest(self, *args, **kwargs):
        unittest._TextTestResult.stopTest(self, *args, **kwargs)
        if self.marionette.check_for_crash():
            # this tells unittest.TestSuite not to continue running tests
            self.shouldStop = True
            test = next((a for a in args if isinstance(a, unittest.TestCase)), None)
            if test:
                self.addError(test, sys.exc_info())


class MarionetteTextTestRunner(StructuredTestRunner):

    resultclass = MarionetteTestResult

    def __init__(self, **kwargs):
        self.marionette = kwargs.pop("marionette")
        self.capabilities = kwargs.pop("capabilities")

        StructuredTestRunner.__init__(self, **kwargs)

    def _makeResult(self):
        return self.resultclass(
            self.stream,
            self.descriptions,
            self.verbosity,
            marionette=self.marionette,
            logger=self.logger,
            result_callbacks=self.result_callbacks,
        )

    def run(self, test):
        result = super(MarionetteTextTestRunner, self).run(test)
        result.printLogs(test)
        return result


class BaseMarionetteArguments(ArgumentParser):
    def __init__(self, **kwargs):
        ArgumentParser.__init__(self, **kwargs)

        def dir_path(path):
            path = os.path.abspath(os.path.expanduser(path))
            if not os.access(path, os.F_OK):
                os.makedirs(path)
            return path

        self.argument_containers = []
        self.add_argument(
            "tests",
            nargs="*",
            default=[],
            help="Tests to run. "
            "One or more paths to test files (Python or JS), "
            "manifest files (.ini) or directories. "
            "When a directory is specified, "
            "all test files in the directory will be run.",
        )
        self.add_argument(
            "--binary",
            help="path to gecko executable to launch before running the test",
        )
        self.add_argument(
            "--address", help="host:port of running Gecko instance to connect to"
        )
        self.add_argument(
            "--emulator",
            action="store_true",
            help="If no --address is given, then the harness will launch an "
            "emulator. (See Remote options group.) "
            "If --address is given, then the harness assumes you are "
            "running an emulator already, and will launch gecko app "
            "on that emulator.",
        )
        self.add_argument(
            "--app", help="application to use. see marionette_driver.geckoinstance"
        )
        self.add_argument(
            "--app-arg",
            dest="app_args",
            action="append",
            default=[],
            help="specify a command line argument to be passed onto the application",
        )
        self.add_argument(
            "--profile",
            help="profile to use when launching the gecko process. If not passed, "
            "then a profile will be constructed and used",
            type=dir_path,
        )
        self.add_argument(
            "--setpref",
            action="append",
            metavar="PREF=VALUE",
            dest="prefs_args",
            help="set a browser preference; repeat for multiple preferences.",
        )
        self.add_argument(
            "--preferences",
            action="append",
            dest="prefs_files",
            help="read preferences from a JSON or INI file. For INI, use "
            "'file.ini:section' to specify a particular section.",
        )
        self.add_argument(
            "--addon",
            action="append",
            dest="addons",
            help="addon to install; repeat for multiple addons.",
        )
        self.add_argument(
            "--repeat", type=int, help="number of times to repeat the test(s)"
        )
        self.add_argument(
            "--run-until-failure",
            action="store_true",
            help="Run tests repeatedly and stop on the first time a test fails. "
            "Default cap is 30 runs, which can be overwritten "
            "with the --repeat parameter.",
        )
        self.add_argument(
            "--testvars",
            action="append",
            help="path to a json file with any test data required",
        )
        self.add_argument(
            "--symbols-path",
            help="absolute path to directory containing breakpad symbols, or the "
            "url of a zip file containing symbols",
        )
        self.add_argument(
            "--socket-timeout",
            type=float,
            default=Marionette.DEFAULT_SOCKET_TIMEOUT,
            help="Set the global timeout for marionette socket operations."
            " Default: %(default)ss.",
        )
        self.add_argument(
            "--startup-timeout",
            type=int,
            default=Marionette.DEFAULT_STARTUP_TIMEOUT,
            help="the max number of seconds to wait for a Marionette connection "
            "after launching a binary. Default: %(default)ss.",
        )
        self.add_argument(
            "--shuffle",
            action="store_true",
            default=False,
            help="run tests in a random order",
        )
        self.add_argument(
            "--shuffle-seed",
            type=int,
            default=random.randint(0, MAXSIZE),
            help="Use given seed to shuffle tests",
        )
        self.add_argument(
            "--total-chunks",
            type=int,
            help="how many chunks to split the tests up into",
        )
        self.add_argument("--this-chunk", type=int, help="which chunk to run")
        self.add_argument(
            "--server-root",
            help="url to a webserver or path to a document root from which content "
            "resources are served (default: {}).".format(
                os.path.join(os.path.dirname(here), "www")
            ),
        )
        self.add_argument(
            "--gecko-log",
            help="Define the path to store log file. If the path is"
            " a directory, the real log file will be created"
            " given the format gecko-(timestamp).log. If it is"
            " a file, if will be used directly. '-' may be passed"
            " to write to stdout. Default: './gecko.log'",
        )
        self.add_argument(
            "--logger-name",
            default="Marionette-based Tests",
            help="Define the name to associate with the logger used",
        )
        self.add_argument(
            "--jsdebugger",
            action="store_true",
            default=False,
            help="Enable the jsdebugger for marionette javascript.",
        )
        self.add_argument(
            "--pydebugger",
            help="Enable python post-mortem debugger when a test fails."
            " Pass in the debugger you want to use, eg pdb or ipdb.",
        )
        self.add_argument(
            "--enable-fission",
            action="store_true",
            dest="enable_fission",
            default=False,
            help="Enable Fission (site isolation) in Gecko.",
        )
        self.add_argument(
            "--enable-webrender",
            action="store_true",
            dest="enable_webrender",
            default=False,
            help="Enable the WebRender compositor in Gecko.",
        )
        self.add_argument(
            "-z",
            "--headless",
            action="store_true",
            dest="headless",
            default=os.environ.get("MOZ_HEADLESS", False),
            help="Run tests in headless mode.",
        )
        self.add_argument(
            "--tag",
            action="append",
            dest="test_tags",
            default=None,
            help="Filter out tests that don't have the given tag. Can be "
            "used multiple times in which case the test must contain "
            "at least one of the given tags.",
        )
        self.add_argument(
            "--workspace",
            action="store",
            default=None,
            help="Path to directory for Marionette output. "
            "(Default: .) (Default profile dest: TMP)",
            type=dir_path,
        )
        self.add_argument(
            "-v",
            "--verbose",
            action="count",
            help="Increase verbosity to include debug messages with -v, "
            "and trace messages with -vv.",
        )
        self.register_argument_container(RemoteMarionetteArguments())

    def register_argument_container(self, container):
        group = self.add_argument_group(container.name)

        for cli, kwargs in container.args:
            group.add_argument(*cli, **kwargs)

        self.argument_containers.append(container)

    def parse_known_args(self, args=None, namespace=None):
        args, remainder = ArgumentParser.parse_known_args(self, args, namespace)
        for container in self.argument_containers:
            if hasattr(container, "parse_args_handler"):
                container.parse_args_handler(args)
        return (args, remainder)

    def _get_preferences(self, prefs_files, prefs_args):
        """Return user defined profile preferences as a dict."""
        # object that will hold the preferences
        prefs = mozprofile.prefs.Preferences()

        # add preferences files
        if prefs_files:
            for prefs_file in prefs_files:
                prefs.add_file(prefs_file)

        separator = "="
        cli_prefs = []
        if prefs_args:
            misformatted = []
            for pref in prefs_args:
                if separator not in pref:
                    misformatted.append(pref)
                else:
                    cli_prefs.append(pref.split(separator, 1))
            if misformatted:
                self._print_message(
                    "Warning: Ignoring preferences not in key{}value format: {}\n".format(
                        separator, ", ".join(misformatted)
                    )
                )
        # string preferences
        prefs.add(cli_prefs, cast=True)

        return dict(prefs())

    def verify_usage(self, args):
        if not args.tests:
            self.error(
                "You must specify one or more test files, manifests, or directories."
            )

        missing_tests = [path for path in args.tests if not os.path.exists(path)]
        if missing_tests:
            self.error(
                "Test file(s) not found: " + " ".join([path for path in missing_tests])
            )

        if not args.address and not args.binary and not args.emulator:
            self.error("You must specify --binary, or --address, or --emulator")

        if args.repeat is not None and args.repeat < 0:
            self.error("The value of --repeat has to be equal or greater than 0.")

        if args.total_chunks is not None and args.this_chunk is None:
            self.error("You must specify which chunk to run.")

        if args.this_chunk is not None and args.total_chunks is None:
            self.error("You must specify how many chunks to split the tests into.")

        if args.total_chunks is not None:
            if not 1 < args.total_chunks:
                self.error("Total chunks must be greater than 1.")
            if not 1 <= args.this_chunk <= args.total_chunks:
                self.error(
                    "Chunk to run must be between 1 and {}.".format(args.total_chunks)
                )

        if args.jsdebugger:
            args.app_args.append("-jsdebugger")
            args.socket_timeout = None

        args.prefs = self._get_preferences(args.prefs_files, args.prefs_args)

        for container in self.argument_containers:
            if hasattr(container, "verify_usage_handler"):
                container.verify_usage_handler(args)

        return args


class RemoteMarionetteArguments(object):
    name = "Remote (Emulator/Device)"
    args = [
        [
            ["--emulator-binary"],
            {
                "help": "Path to emulator binary. By default mozrunner uses `which emulator`",
                "dest": "emulator_bin",
            },
        ],
        [
            ["--adb"],
            {
                "help": "Path to the adb. By default mozrunner uses `which adb`",
                "dest": "adb_path",
            },
        ],
        [
            ["--avd"],
            {
                "help": (
                    "Name of an AVD available in your environment."
                    "See mozrunner.FennecEmulatorRunner"
                ),
            },
        ],
        [
            ["--avd-home"],
            {
                "help": "Path to avd parent directory",
            },
        ],
        [
            ["--device"],
            {
                "help": (
                    "Serial ID to connect to as seen in `adb devices`,"
                    "e.g emulator-5444"
                ),
                "dest": "device_serial",
            },
        ],
        [
            ["--package"],
            {
                "help": "Name of Android package, e.g. org.mozilla.fennec",
                "dest": "package_name",
            },
        ],
    ]


class Fixtures(object):
    def where_is(self, uri, on="http"):
        return serve.where_is(uri, on)


class BaseMarionetteTestRunner(object):

    textrunnerclass = MarionetteTextTestRunner
    driverclass = Marionette

    def __init__(
        self,
        address=None,
        app=None,
        app_args=None,
        binary=None,
        profile=None,
        logger=None,
        logdir=None,
        repeat=None,
        run_until_failure=None,
        testvars=None,
        symbols_path=None,
        shuffle=False,
        shuffle_seed=random.randint(0, MAXSIZE),
        this_chunk=1,
        total_chunks=1,
        server_root=None,
        gecko_log=None,
        result_callbacks=None,
        prefs=None,
        test_tags=None,
        socket_timeout=None,
        startup_timeout=None,
        addons=None,
        workspace=None,
        verbose=0,
        emulator=False,
        headless=False,
        enable_fission=False,
        enable_webrender=False,
        **kwargs
    ):
        self._appName = None
        self._capabilities = None
        self._filename_pattern = None
        self._version_info = {}

        self.fixture_servers = {}
        self.fixtures = Fixtures()
        self.extra_kwargs = kwargs
        self.test_kwargs = deepcopy(kwargs)
        self.address = address
        self.app = app
        self.app_args = app_args or []
        self.bin = binary
        self.emulator = emulator
        self.profile = profile
        self.addons = addons
        self.logger = logger
        self.marionette = None
        self.logdir = logdir
        self.repeat = repeat or 0
        self.run_until_failure = run_until_failure or False
        self.symbols_path = symbols_path
        self.socket_timeout = socket_timeout
        self.startup_timeout = startup_timeout
        self.shuffle = shuffle
        self.shuffle_seed = shuffle_seed
        self.server_root = server_root
        self.this_chunk = this_chunk
        self.total_chunks = total_chunks
        self.mixin_run_tests = []
        self.manifest_skipped_tests = []
        self.tests = []
        self.result_callbacks = result_callbacks or []
        self.prefs = prefs or {}
        self.test_tags = test_tags
        self.workspace = workspace
        # If no workspace is set, default location for gecko.log is .
        # and default location for profile is TMP
        self.workspace_path = workspace or os.getcwd()
        self.verbose = verbose
        self.headless = headless
        self.enable_webrender = enable_webrender

        self.enable_fission = enable_fission
        if self.enable_fission:
            self.prefs.update(
                {
                    "fission.autostart": True,
                }
            )
        else:
            self.prefs.update(
                {
                    "fission.autostart": False,
                }
            )

        # If no repeat has been set, default to 30 extra runs
        if self.run_until_failure and repeat is None:
            self.repeat = 30

        def gather_debug(test, status):
            # No screenshots and page source for skipped tests
            if status == "SKIP":
                return

            rv = {}
            marionette = test._marionette_weakref()

            # In the event we're gathering debug without starting a session,
            # skip marionette commands
            if marionette.session is not None:
                try:
                    with marionette.using_context(marionette.CONTEXT_CHROME):
                        rv["screenshot"] = marionette.screenshot()
                    with marionette.using_context(marionette.CONTEXT_CONTENT):
                        rv["source"] = marionette.page_source
                except Exception as exc:
                    self.logger.warning(
                        "Failed to gather test failure debug: {}".format(exc)
                    )
            return rv

        self.result_callbacks.append(gather_debug)

        # testvars are set up in self.testvars property
        self._testvars = None
        self.testvars_paths = testvars

        self.test_handlers = []

        self.reset_test_stats()

        self.logger.info(
            "Using workspace for temporary data: " '"{}"'.format(self.workspace_path)
        )

        if not gecko_log:
            self.gecko_log = os.path.join(self.workspace_path or "", "gecko.log")
        else:
            self.gecko_log = gecko_log

        self.results = []

    @property
    def filename_pattern(self):
        if self._filename_pattern is None:
            self._filename_pattern = re.compile("^test(((_.+?)+?\.((py))))$")

        return self._filename_pattern

    @property
    def testvars(self):
        if self._testvars is not None:
            return self._testvars

        self._testvars = {}

        def update(d, u):
            """Update a dictionary that may contain nested dictionaries."""
            for k, v in six.iteritems(u):
                o = d.get(k, {})
                if isinstance(v, dict) and isinstance(o, dict):
                    d[k] = update(d.get(k, {}), v)
                else:
                    d[k] = u[k]
            return d

        json_testvars = self._load_testvars()
        for j in json_testvars:
            self._testvars = update(self._testvars, j)
        return self._testvars

    def _load_testvars(self):
        data = []
        if self.testvars_paths is not None:
            for path in list(self.testvars_paths):
                path = os.path.abspath(os.path.expanduser(path))
                if not os.path.exists(path):
                    raise IOError("--testvars file {} does not exist".format(path))
                try:
                    with open(path) as f:
                        data.append(json.loads(f.read()))
                except ValueError as e:
                    msg = "JSON file ({0}) is not properly formatted: {1}"
                    reraise(
                        ValueError,
                        ValueError(msg.format(os.path.abspath(path), e)),
                        sys.exc_info()[2],
                    )
        return data

    @property
    def capabilities(self):
        if self._capabilities:
            return self._capabilities

        self.marionette.start_session()
        self._capabilities = self.marionette.session_capabilities
        self.marionette.delete_session()
        return self._capabilities

    @property
    def appName(self):
        if self._appName:
            return self._appName

        self._appName = self.capabilities.get("browserName")
        return self._appName

    @property
    def bin(self):
        return self._bin

    @bin.setter
    def bin(self, path):
        """Set binary and reset parts of runner accordingly.
        Intended use: to change binary between calls to run_tests
        """
        self._bin = path
        self.tests = []
        self.cleanup()

    @property
    def version_info(self):
        if not self._version_info:
            try:
                # TODO: Get version_info in Fennec case
                self._version_info = mozversion.get_version(binary=self.bin)
            except Exception:
                self.logger.warning(
                    "Failed to retrieve version information for {}".format(self.bin)
                )
        return self._version_info

    def reset_test_stats(self):
        self.passed = 0
        self.failed = 0
        self.crashed = 0
        self.unexpected_successes = 0
        self.todo = 0
        self.skipped = 0
        self.failures = []

    def _build_kwargs(self):
        if self.logdir and not os.access(self.logdir, os.F_OK):
            os.mkdir(self.logdir)

        kwargs = {
            "socket_timeout": self.socket_timeout,
            "prefs": self.prefs,
            "startup_timeout": self.startup_timeout,
            "verbose": self.verbose,
            "symbols_path": self.symbols_path,
            "enable_webrender": self.enable_webrender,
        }
        if self.bin or self.emulator:
            kwargs.update(
                {
                    "host": "127.0.0.1",
                    "port": 2828,
                    "app": self.app,
                    "app_args": self.app_args,
                    "profile": self.profile,
                    "addons": self.addons,
                    "gecko_log": self.gecko_log,
                    # ensure Marionette class takes care of starting gecko instance
                    "bin": True,
                }
            )

        if self.bin:
            kwargs.update(
                {
                    "bin": self.bin,
                }
            )

        if self.emulator:
            kwargs.update(
                {
                    "avd_home": self.extra_kwargs.get("avd_home"),
                    "adb_path": self.extra_kwargs.get("adb_path"),
                    "emulator_binary": self.extra_kwargs.get("emulator_bin"),
                    "avd": self.extra_kwargs.get("avd"),
                    "package_name": self.extra_kwargs.get("package_name"),
                }
            )

        if self.address:
            host, port = self.address.split(":")
            kwargs.update(
                {
                    "host": host,
                    "port": int(port),
                }
            )
            if self.emulator:
                kwargs.update(
                    {
                        "connect_to_running_emulator": True,
                    }
                )
            if not self.bin and not self.emulator:
                try:
                    # Establish a socket connection so we can vertify the data come back
                    connection = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    connection.connect((host, int(port)))
                    connection.close()
                except Exception as e:
                    exc_cls, _, tb = sys.exc_info()
                    msg = "Connection attempt to {0}:{1} failed with error: {2}"
                    reraise(exc_cls, exc_cls(msg.format(host, port, e)), tb)
        if self.workspace:
            kwargs["workspace"] = self.workspace_path
        if self.headless:
            kwargs["headless"] = True

        return kwargs

    def record_crash(self):
        crash = True
        try:
            crash = self.marionette.check_for_crash()
            self.crashed += int(crash)
        except Exception:
            traceback.print_exc()
        return crash

    def _initialize_test_run(self, tests):
        assert len(tests) > 0
        assert len(self.test_handlers) > 0
        self.reset_test_stats()

    def _add_tests(self, tests):
        for test in tests:
            self.add_test(test)

        invalid_tests = [
            t["filepath"]
            for t in self.tests
            if not self._is_filename_valid(t["filepath"])
        ]
        if invalid_tests:
            raise Exception(
                "Test file names must be of the form "
                "'test_something.py'."
                " Invalid test names:\n  {}".format("\n  ".join(invalid_tests))
            )

    def _is_filename_valid(self, filename):
        filename = os.path.basename(filename)
        return self.filename_pattern.match(filename)

    def _fix_test_path(self, path):
        """Normalize a logged test path from the test package."""
        test_path_prefixes = [
            "tests{}".format(os.path.sep),
        ]

        path = os.path.relpath(path)
        for prefix in test_path_prefixes:
            if path.startswith(prefix):
                path = path[len(prefix) :]
                break
        path = path.replace("\\", "/")

        return path

    def _log_skipped_tests(self):
        for test in self.manifest_skipped_tests:
            rel_path = None
            if os.path.exists(test["path"]):
                rel_path = self._fix_test_path(test["path"])

            self.logger.test_start(rel_path)
            self.logger.test_end(rel_path, "SKIP", message=test["disabled"])
            self.todo += 1

    def run_tests(self, tests):
        start_time = time.time()
        self._initialize_test_run(tests)

        if self.marionette is None:
            self.marionette = self.driverclass(**self._build_kwargs())
            self.logger.info("Profile path is %s" % self.marionette.profile_path)

        if len(self.fixture_servers) == 0 or any(
            not server.is_alive for _, server in self.fixture_servers
        ):
            self.logger.info("Starting fixture servers")
            self.fixture_servers = self.start_fixture_servers()
            for url in serve.iter_url(self.fixture_servers):
                self.logger.info("Fixture server listening on %s" % url)

            # backwards compatibility
            self.marionette.baseurl = serve.where_is("/")

        self._add_tests(tests)

        device_info = None
        if self.marionette.instance and self.emulator:
            try:
                device_info = self.marionette.instance.runner.device.device.get_info()
            except Exception:
                self.logger.warning("Could not get device info", exc_info=True)

        tests_by_group = defaultdict(list)
        for test in self.tests:
            group = self._fix_test_path(test["group"])
            filepath = self._fix_test_path(test["filepath"])
            tests_by_group[group].append(filepath)

        self.logger.suite_start(
            tests_by_group,
            name="marionette-test",
            version_info=self.version_info,
            device_info=device_info,
        )

        if self.shuffle:
            self.logger.info("Using shuffle seed: %d" % self.shuffle_seed)

        self._log_skipped_tests()

        interrupted = None
        try:
            repeat_index = 0
            while repeat_index <= self.repeat:
                if repeat_index > 0:
                    self.logger.info("\nREPEAT {}\n-------".format(repeat_index))
                self.run_test_sets()
                if self.run_until_failure and self.failed > 0:
                    break

                repeat_index += 1

        except KeyboardInterrupt:
            # in case of KeyboardInterrupt during the test execution
            # we want to display current test results.
            # so we keep the exception to raise it later.
            interrupted = sys.exc_info()
        except Exception:
            # For any other exception we return immediately and have to
            # cleanup running processes
            self.cleanup()
            raise

        try:
            self._print_summary(tests)
            self.record_crash()
            self.elapsedtime = time.time() - start_time

            for run_tests in self.mixin_run_tests:
                run_tests(tests)

            self.logger.suite_end()
        except Exception:
            # raise only the exception if we were not interrupted
            if not interrupted:
                raise
        finally:
            self.cleanup()

            # reraise previous interruption now
            if interrupted:
                reraise(interrupted[0], interrupted[1], interrupted[2])

    def _print_summary(self, tests):
        self.logger.info("\nSUMMARY\n-------")
        self.logger.info("passed: {}".format(self.passed))
        if self.unexpected_successes == 0:
            self.logger.info("failed: {}".format(self.failed))
        else:
            self.logger.info(
                "failed: {0} (unexpected sucesses: {1})".format(
                    self.failed, self.unexpected_successes
                )
            )
        if self.skipped == 0:
            self.logger.info("todo: {}".format(self.todo))
        else:
            self.logger.info("todo: {0} (skipped: {1})".format(self.todo, self.skipped))

        if self.failed > 0:
            self.logger.info("\nFAILED TESTS\n-------")
            for failed_test in self.failures:
                self.logger.info("{}".format(failed_test[0]))

    def start_fixture_servers(self):
        root = self.server_root or os.path.join(os.path.dirname(here), "www")
        if self.appName == "fennec":
            return serve.start(root, host=moznetwork.get_ip())
        else:
            return serve.start(root)

    def add_test(self, test, expected="pass", group="default"):
        filepath = os.path.abspath(test)

        if os.path.isdir(filepath):
            for root, dirs, files in os.walk(filepath):
                for filename in files:
                    if filename.endswith(".ini"):
                        msg_tmpl = (
                            "Ignoring manifest '{0}'; running all tests in '{1}'."
                            " See --help for details."
                        )
                        relpath = os.path.relpath(
                            os.path.join(root, filename), filepath
                        )
                        self.logger.warning(msg_tmpl.format(relpath, filepath))
                    elif self._is_filename_valid(filename):
                        test_file = os.path.join(root, filename)
                        self.add_test(test_file)
            return

        file_ext = os.path.splitext(os.path.split(filepath)[-1])[1]

        if file_ext == ".ini":
            group = filepath

            manifest = TestManifest()
            manifest.read(filepath)

            json_path = update_mozinfo(filepath)
            mozinfo.update(
                {
                    "appname": self.appName,
                    "manage_instance": self.marionette.instance is not None,
                    "headless": self.headless,
                    "webrender": self.enable_webrender,
                }
            )
            self.logger.info("mozinfo updated from: {}".format(json_path))
            self.logger.info("mozinfo is: {}".format(mozinfo.info))

            filters = []
            if self.test_tags:
                filters.append(tags(self.test_tags))

            manifest_tests = manifest.active_tests(
                exists=False, disabled=True, filters=filters, **mozinfo.info
            )
            if len(manifest_tests) == 0:
                self.logger.error(
                    "No tests to run using specified "
                    "combination of filters: {}".format(manifest.fmt_filters())
                )

            target_tests = []
            for test in manifest_tests:
                if test.get("disabled"):
                    self.manifest_skipped_tests.append(test)
                else:
                    target_tests.append(test)

            for i in target_tests:
                if not os.path.exists(i["path"]):
                    raise IOError("test file: {} does not exist".format(i["path"]))

                self.add_test(i["path"], i["expected"], group=group)
            return

        self.tests.append({"filepath": filepath, "expected": expected, "group": group})

    def run_test(self, filepath, expected):
        testloader = unittest.TestLoader()
        suite = unittest.TestSuite()
        self.test_kwargs["expected"] = expected
        mod_name = os.path.splitext(os.path.split(filepath)[-1])[0]
        for handler in self.test_handlers:
            if handler.match(os.path.basename(filepath)):
                handler.add_tests_to_suite(
                    mod_name,
                    filepath,
                    suite,
                    testloader,
                    self.marionette,
                    self.fixtures,
                    self.testvars,
                    **self.test_kwargs
                )
                break

        if suite.countTestCases():
            runner = self.textrunnerclass(
                logger=self.logger,
                marionette=self.marionette,
                capabilities=self.capabilities,
                result_callbacks=self.result_callbacks,
            )

            results = runner.run(suite)
            self.results.append(results)

            self.failed += len(results.failures) + len(results.errors)
            if hasattr(results, "skipped"):
                self.skipped += len(results.skipped)
                self.todo += len(results.skipped)
            self.passed += results.passed
            for failure in results.failures + results.errors:
                self.failures.append(
                    (results.getInfo(failure), failure.output, "TEST-UNEXPECTED-FAIL")
                )
            if hasattr(results, "unexpectedSuccesses"):
                self.failed += len(results.unexpectedSuccesses)
                self.unexpected_successes += len(results.unexpectedSuccesses)
                for failure in results.unexpectedSuccesses:
                    self.failures.append(
                        (
                            results.getInfo(failure),
                            failure.output,
                            "TEST-UNEXPECTED-PASS",
                        )
                    )
            if hasattr(results, "expectedFailures"):
                self.todo += len(results.expectedFailures)

            self.mixin_run_tests = []
            for result in self.results:
                result.result_modifiers = []

    def run_test_set(self, tests):
        if self.shuffle:
            random.seed(self.shuffle_seed)
            random.shuffle(tests)

        for test in tests:
            self.run_test(test["filepath"], test["expected"])
            if self.record_crash():
                break

    def run_test_sets(self):
        if len(self.tests) < 1:
            raise Exception("There are no tests to run.")
        elif self.total_chunks is not None and self.total_chunks > len(self.tests):
            raise ValueError(
                "Total number of chunks must be between 1 and {}.".format(
                    len(self.tests)
                )
            )
        if self.total_chunks is not None and self.total_chunks > 1:
            chunks = [[] for i in range(self.total_chunks)]
            for i, test in enumerate(self.tests):
                target_chunk = i % self.total_chunks
                chunks[target_chunk].append(test)

            self.logger.info(
                "Running chunk {0} of {1} ({2} tests selected from a "
                "total of {3})".format(
                    self.this_chunk,
                    self.total_chunks,
                    len(chunks[self.this_chunk - 1]),
                    len(self.tests),
                )
            )
            self.tests = chunks[self.this_chunk - 1]

        self.run_test_set(self.tests)

    def cleanup(self):
        for proc in serve.iter_proc(self.fixture_servers):
            proc.stop()
            proc.kill()
        self.fixture_servers = {}

        if hasattr(self, "marionette") and self.marionette:
            if self.marionette.instance is not None:
                if self.marionette.instance.runner.is_running():
                    # Force a clean shutdown of the application process first if
                    # it is still running. If that fails, kill the process.
                    # Therefore a new session needs to be started.
                    self.marionette.start_session()
                    self.marionette.quit(in_app=True)

                self.marionette.instance.close(clean=True)
                self.marionette.instance = None

            self.marionette.cleanup()
            self.marionette = None

    __del__ = cleanup
