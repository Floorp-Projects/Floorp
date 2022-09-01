#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import copy
import glob
import os
import re
import sys
import subprocess
import tempfile

from shutil import copyfile, rmtree

from six import string_types

import mozharness

from mozharness.base.errors import PythonErrorList
from mozharness.base.log import OutputParser, DEBUG, ERROR, CRITICAL, INFO
from mozharness.mozilla.automation import (
    EXIT_STATUS_DICT,
    TBPL_SUCCESS,
    TBPL_RETRY,
    TBPL_WORST_LEVEL_TUPLE,
)
from mozharness.base.python import Python3Virtualenv
from mozharness.mozilla.testing.android import AndroidMixin
from mozharness.mozilla.testing.errors import HarnessErrorList, TinderBoxPrintRe
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.testing.codecoverage import (
    CodeCoverageMixin,
    code_coverage_config_options,
)

scripts_path = os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__)))
external_tools_path = os.path.join(scripts_path, "external_tools")
here = os.path.abspath(os.path.dirname(__file__))

RaptorErrorList = (
    PythonErrorList
    + HarnessErrorList
    + [
        {"regex": re.compile(r"""run-as: Package '.*' is unknown"""), "level": DEBUG},
        {"substr": r"""raptorDebug""", "level": DEBUG},
        {
            "regex": re.compile(r"""^raptor[a-zA-Z-]*( - )?( )?(?i)error(:)?"""),
            "level": ERROR,
        },
        {
            "regex": re.compile(r"""^raptor[a-zA-Z-]*( - )?( )?(?i)critical(:)?"""),
            "level": CRITICAL,
        },
        {
            "regex": re.compile(r"""No machine_name called '.*' can be found"""),
            "level": CRITICAL,
        },
        {
            "substr": r"""No such file or directory: 'browser_output.txt'""",
            "level": CRITICAL,
            "explanation": "Most likely the browser failed to launch, or the test otherwise "
            "failed to start.",
        },
    ]
)

# When running raptor locally, we can attempt to make use of
# the users locally cached ffmpeg binary from from when the user
# ran `./mach browsertime --setup`
FFMPEG_LOCAL_CACHE = {
    "mac": "ffmpeg-4.1.1-macos64-static",
    "linux": "ffmpeg-4.1.4-i686-static",
    "win": "ffmpeg-4.1.1-win64-static",
}


class Raptor(
    TestingMixin, MercurialScript, CodeCoverageMixin, AndroidMixin, Python3Virtualenv
):
    """
    Install and run Raptor tests
    """

    # Options to Browsertime.  Paths are expected to be absolute.
    browsertime_options = [
        [
            ["--browsertime-node"],
            {"dest": "browsertime_node", "default": None, "help": argparse.SUPPRESS},
        ],
        [
            ["--browsertime-browsertimejs"],
            {
                "dest": "browsertime_browsertimejs",
                "default": None,
                "help": argparse.SUPPRESS,
            },
        ],
        [
            ["--browsertime-vismet-script"],
            {
                "dest": "browsertime_vismet_script",
                "default": None,
                "help": argparse.SUPPRESS,
            },
        ],
        [
            ["--browsertime-chromedriver"],
            {
                "dest": "browsertime_chromedriver",
                "default": None,
                "help": argparse.SUPPRESS,
            },
        ],
        [
            ["--browsertime-ffmpeg"],
            {"dest": "browsertime_ffmpeg", "default": None, "help": argparse.SUPPRESS},
        ],
        [
            ["--browsertime-geckodriver"],
            {
                "dest": "browsertime_geckodriver",
                "default": None,
                "help": argparse.SUPPRESS,
            },
        ],
        [
            ["--browsertime-video"],
            {
                "dest": "browsertime_video",
                "action": "store_true",
                "default": False,
                "help": argparse.SUPPRESS,
            },
        ],
        [
            ["--browsertime-visualmetrics"],
            {
                "dest": "browsertime_visualmetrics",
                "action": "store_true",
                "default": False,
                "help": argparse.SUPPRESS,
            },
        ],
        [
            ["--browsertime-no-ffwindowrecorder"],
            {
                "dest": "browsertime_no_ffwindowrecorder",
                "action": "store_true",
                "default": False,
                "help": argparse.SUPPRESS,
            },
        ],
        [
            ["--browsertime-arg"],
            {
                "action": "append",
                "metavar": "PREF=VALUE",
                "dest": "browsertime_user_args",
                "default": [],
                "help": argparse.SUPPRESS,
            },
        ],
        [
            ["--browsertime"],
            {
                "dest": "browsertime",
                "action": "store_true",
                "default": True,
                "help": argparse.SUPPRESS,
            },
        ],
    ]

    config_options = (
        [
            [
                ["--test"],
                {"action": "store", "dest": "test", "help": "Raptor test to run"},
            ],
            [
                ["--app"],
                {
                    "default": "firefox",
                    "choices": [
                        "firefox",
                        "chrome",
                        "chrome-m",
                        "chromium",
                        "fennec",
                        "geckoview",
                        "refbrow",
                        "fenix",
                    ],
                    "dest": "app",
                    "help": "Name of the application we are testing (default: firefox).",
                },
            ],
            [
                ["--activity"],
                {
                    "dest": "activity",
                    "help": "The Android activity used to launch the Android app. "
                    "e.g.: org.mozilla.fenix.browser.BrowserPerformanceTestActivity",
                },
            ],
            [
                ["--intent"],
                {
                    "dest": "intent",
                    "help": "Name of the Android intent action used to launch the Android app",
                },
            ],
            [
                ["--is-release-build"],
                {
                    "action": "store_true",
                    "dest": "is_release_build",
                    "help": "Whether the build is a release build which requires work arounds "
                    "using MOZ_DISABLE_NONLOCAL_CONNECTIONS to support installing unsigned "
                    "webextensions. Defaults to False.",
                },
            ],
            [
                ["--add-option"],
                {
                    "action": "extend",
                    "dest": "raptor_cmd_line_args",
                    "default": None,
                    "help": "Extra options to Raptor.",
                },
            ],
            [
                ["--device-name"],
                {
                    "dest": "device_name",
                    "default": None,
                    "help": "Device name of mobile device.",
                },
            ],
            [
                ["--geckoProfile"],
                {
                    "dest": "gecko_profile",
                    "action": "store_true",
                    "default": False,
                    "help": argparse.SUPPRESS,
                },
            ],
            [
                ["--geckoProfileInterval"],
                {
                    "dest": "gecko_profile_interval",
                    "type": "int",
                    "help": argparse.SUPPRESS,
                },
            ],
            [
                ["--geckoProfileEntries"],
                {
                    "dest": "gecko_profile_entries",
                    "type": "int",
                    "help": argparse.SUPPRESS,
                },
            ],
            [
                ["--geckoProfileFeatures"],
                {
                    "dest": "gecko_profile_features",
                    "type": "str",
                    "help": argparse.SUPPRESS,
                },
            ],
            [
                ["--gecko-profile"],
                {
                    "dest": "gecko_profile",
                    "action": "store_true",
                    "default": False,
                    "help": "Whether to profile the test run and save the profile results.",
                },
            ],
            [
                ["--gecko-profile-interval"],
                {
                    "dest": "gecko_profile_interval",
                    "type": "int",
                    "help": "The interval between samples taken by the profiler (ms).",
                },
            ],
            [
                ["--gecko-profile-entries"],
                {
                    "dest": "gecko_profile_entries",
                    "type": "int",
                    "help": "How many samples to take with the profiler.",
                },
            ],
            [
                ["--gecko-profile-threads"],
                {
                    "dest": "gecko_profile_threads",
                    "type": "str",
                    "help": "Comma-separated list of threads to sample.",
                },
            ],
            [
                ["--gecko-profile-features"],
                {
                    "dest": "gecko_profile_features",
                    "type": "str",
                    "help": "Features to enable in the profiler.",
                },
            ],
            [
                ["--extra-profiler-run"],
                {
                    "dest": "extra_profiler_run",
                    "action": "store_true",
                    "default": False,
                    "help": "Run the tests again with profiler enabled after the main run.",
                },
            ],
            [
                ["--page-cycles"],
                {
                    "dest": "page_cycles",
                    "type": "int",
                    "help": (
                        "How many times to repeat loading the test page (for page load "
                        "tests); for benchmark tests this is how many times the benchmark test "
                        "will be run."
                    ),
                },
            ],
            [
                ["--page-timeout"],
                {
                    "dest": "page_timeout",
                    "type": "int",
                    "help": "How long to wait (ms) for one page_cycle to complete, before timing out.",  # NOQA: E501
                },
            ],
            [
                ["--browser-cycles"],
                {
                    "dest": "browser_cycles",
                    "type": "int",
                    "help": (
                        "The number of times a cold load test is repeated (for cold load tests "
                        "only, where the browser is shutdown and restarted between test "
                        "iterations)."
                    ),
                },
            ],
            [
                ["--project"],
                {
                    "action": "store",
                    "dest": "project",
                    "default": "mozilla-central",
                    "type": "str",
                    "help": "Name of the project (try, mozilla-central, etc.)",
                },
            ],
            [
                ["--test-url-params"],
                {
                    "action": "store",
                    "dest": "test_url_params",
                    "help": "Parameters to add to the test_url query string.",
                },
            ],
            [
                ["--host"],
                {
                    "dest": "host",
                    "type": "str",
                    "default": "127.0.0.1",
                    "help": "Hostname from which to serve urls (default: 127.0.0.1). "
                    "The value HOST_IP will cause the value of host to be "
                    "to be loaded from the environment variable HOST_IP.",
                },
            ],
            [
                ["--power-test"],
                {
                    "dest": "power_test",
                    "action": "store_true",
                    "default": False,
                    "help": (
                        "Use Raptor to measure power usage on Android browsers (Geckoview "
                        "Example, Fenix, Refbrow, and Fennec) as well as on Intel-based MacOS "
                        "machines that have Intel Power Gadget installed."
                    ),
                },
            ],
            [
                ["--memory-test"],
                {
                    "dest": "memory_test",
                    "action": "store_true",
                    "default": False,
                    "help": "Use Raptor to measure memory usage.",
                },
            ],
            [
                ["--cpu-test"],
                {
                    "dest": "cpu_test",
                    "action": "store_true",
                    "default": False,
                    "help": "Use Raptor to measure CPU usage.",
                },
            ],
            [
                ["--disable-perf-tuning"],
                {
                    "action": "store_true",
                    "dest": "disable_perf_tuning",
                    "default": False,
                    "help": "Disable performance tuning on android.",
                },
            ],
            [
                ["--conditioned-profile"],
                {
                    "dest": "conditioned_profile",
                    "type": "str",
                    "default": None,
                    "help": (
                        "Name of conditioned profile to use. Prefix with `artifact:` "
                        "if we should obtain the profile from CI.",
                    ),
                },
            ],
            [
                ["--live-sites"],
                {
                    "dest": "live_sites",
                    "action": "store_true",
                    "default": False,
                    "help": "Run tests using live sites instead of recorded sites.",
                },
            ],
            [
                ["--test-bytecode-cache"],
                {
                    "dest": "test_bytecode_cache",
                    "action": "store_true",
                    "default": False,
                    "help": (
                        "If set, the pageload test will set the preference "
                        "`dom.script_loader.bytecode_cache.strategy=-1` and wait 20 seconds "
                        "after the first cold pageload to populate the bytecode cache before "
                        "running a warm pageload test. Only available if `--chimera` "
                        "is also provided."
                    ),
                },
            ],
            [
                ["--chimera"],
                {
                    "dest": "chimera",
                    "action": "store_true",
                    "default": False,
                    "help": "Run tests in chimera mode. Each browser cycle will run a cold and warm test.",  # NOQA: E501
                },
            ],
            [
                ["--debug-mode"],
                {
                    "dest": "debug_mode",
                    "action": "store_true",
                    "default": False,
                    "help": "Run Raptor in debug mode (open browser console, limited page-cycles, etc.)",  # NOQA: E501
                },
            ],
            [
                ["--noinstall"],
                {
                    "dest": "noinstall",
                    "action": "store_true",
                    "default": False,
                    "help": "Do not offer to install Android APK.",
                },
            ],
            [
                ["--disable-e10s"],
                {
                    "dest": "e10s",
                    "action": "store_false",
                    "default": True,
                    "help": "Run without multiple processes (e10s).",
                },
            ],
            [
                ["--disable-fission"],
                {
                    "action": "store_false",
                    "dest": "fission",
                    "default": True,
                    "help": "Disable Fission (site isolation) in Gecko.",
                },
            ],
            [
                ["--setpref"],
                {
                    "action": "append",
                    "metavar": "PREF=VALUE",
                    "dest": "extra_prefs",
                    "default": [],
                    "help": "Set a browser preference. May be used multiple times.",
                },
            ],
            [
                ["--setenv"],
                {
                    "action": "append",
                    "metavar": "NAME=VALUE",
                    "dest": "environment",
                    "default": [],
                    "help": "Set a variable in the test environment. May be used multiple times.",
                },
            ],
            [
                ["--skip-preflight"],
                {
                    "action": "store_true",
                    "dest": "skip_preflight",
                    "default": False,
                    "help": "skip preflight commands to prepare machine.",
                },
            ],
            [
                ["--cold"],
                {
                    "action": "store_true",
                    "dest": "cold",
                    "default": False,
                    "help": "Enable cold page-load for browsertime tp6",
                },
            ],
            [
                ["--verbose"],
                {
                    "action": "store_true",
                    "dest": "verbose",
                    "default": False,
                    "help": "Verbose output",
                },
            ],
            [
                ["--enable-marionette-trace"],
                {
                    "action": "store_true",
                    "dest": "enable_marionette_trace",
                    "default": False,
                    "help": "Enable marionette tracing",
                },
            ],
            [
                ["--clean"],
                {
                    "action": "store_true",
                    "dest": "clean",
                    "default": False,
                    "help": (
                        "Clean the python virtualenv (remove, and rebuild) for "
                        "Raptor before running tests."
                    ),
                },
            ],
            [
                ["--webext"],
                {
                    "action": "store_true",
                    "dest": "webext",
                    "default": False,
                    "help": (
                        "Whether to use webextension to execute pageload tests "
                        "(WebExtension is being deprecated).",
                    ),
                },
            ],
            [
                ["--collect-perfstats"],
                {
                    "action": "store_true",
                    "dest": "collect_perfstats",
                    "default": False,
                    "help": (
                        "If set, the test will collect perfstats in addition to "
                        "the regular metrics it gathers."
                    ),
                },
            ],
        ]
        + testing_config_options
        + copy.deepcopy(code_coverage_config_options)
        + browsertime_options
    )

    def __init__(self, **kwargs):
        kwargs.setdefault("config_options", self.config_options)
        kwargs.setdefault(
            "all_actions",
            [
                "clobber",
                "download-and-extract",
                "populate-webroot",
                "create-virtualenv",
                "install-chrome-android",
                "install-chromium-distribution",
                "install",
                "run-tests",
            ],
        )
        kwargs.setdefault(
            "default_actions",
            [
                "clobber",
                "download-and-extract",
                "populate-webroot",
                "create-virtualenv",
                "install-chromium-distribution",
                "install",
                "run-tests",
            ],
        )
        kwargs.setdefault("config", {})
        super(Raptor, self).__init__(**kwargs)

        # Convenience
        self.workdir = self.query_abs_dirs()["abs_work_dir"]

        self.run_local = self.config.get("run_local")

        # App (browser testing on) defaults to firefox
        self.app = "firefox"

        if self.run_local:
            # Get app from command-line args, passed in from mach, inside 'raptor_cmd_line_args'
            # Command-line args can be in two formats depending on how the user entered them
            # i.e. "--app=geckoview" or separate as "--app", "geckoview" so we have to
            # parse carefully.  It's simplest to use `argparse` to parse partially.
            self.app = "firefox"
            if "raptor_cmd_line_args" in self.config:
                sub_parser = argparse.ArgumentParser()
                # It's not necessary to limit the allowed values: each value
                # will be parsed and verifed by raptor/raptor.py.
                sub_parser.add_argument("--app", default=None, dest="app")
                sub_parser.add_argument("-i", "--intent", default=None, dest="intent")
                sub_parser.add_argument(
                    "-a", "--activity", default=None, dest="activity"
                )

                # We'd prefer to use `parse_known_intermixed_args`, but that's
                # new in Python 3.7.
                known, unknown = sub_parser.parse_known_args(
                    self.config["raptor_cmd_line_args"]
                )

                if known.app:
                    self.app = known.app
                if known.intent:
                    self.intent = known.intent
                if known.activity:
                    self.activity = known.activity
        else:
            # Raptor initiated in production via mozharness
            self.test = self.config["test"]
            self.app = self.config.get("app", "firefox")
            self.binary_path = self.config.get("binary_path", None)

            if self.app in ("refbrow", "fenix"):
                self.app_name = self.binary_path

        self.installer_url = self.config.get("installer_url")
        self.raptor_json_url = self.config.get("raptor_json_url")
        self.raptor_json = self.config.get("raptor_json")
        self.raptor_json_config = self.config.get("raptor_json_config")
        self.repo_path = self.config.get("repo_path")
        self.obj_path = self.config.get("obj_path")
        self.test = None
        self.gecko_profile = self.config.get(
            "gecko_profile"
        ) or "--geckoProfile" in self.config.get("raptor_cmd_line_args", [])
        self.gecko_profile_interval = self.config.get("gecko_profile_interval")
        self.gecko_profile_entries = self.config.get("gecko_profile_entries")
        self.gecko_profile_threads = self.config.get("gecko_profile_threads")
        self.gecko_profile_features = self.config.get("gecko_profile_features")
        self.extra_profiler_run = self.config.get("extra_profiler_run")
        self.test_packages_url = self.config.get("test_packages_url")
        self.test_url_params = self.config.get("test_url_params")
        self.host = self.config.get("host")
        if self.host == "HOST_IP":
            self.host = os.environ["HOST_IP"]
        self.power_test = self.config.get("power_test")
        self.memory_test = self.config.get("memory_test")
        self.cpu_test = self.config.get("cpu_test")
        self.live_sites = self.config.get("live_sites")
        self.chimera = self.config.get("chimera")
        self.disable_perf_tuning = self.config.get("disable_perf_tuning")
        self.conditioned_profile = self.config.get("conditioned_profile")
        self.extra_prefs = self.config.get("extra_prefs")
        self.environment = self.config.get("environment")
        self.is_release_build = self.config.get("is_release_build")
        self.debug_mode = self.config.get("debug_mode", False)
        self.chromium_dist_path = None
        self.firefox_android_browsers = ["fennec", "geckoview", "refbrow", "fenix"]
        self.android_browsers = self.firefox_android_browsers + ["chrome-m"]
        self.browsertime_visualmetrics = self.config.get("browsertime_visualmetrics")
        self.browsertime_node = self.config.get("browsertime_node")
        self.browsertime_user_args = self.config.get("browsertime_user_args")
        self.browsertime_video = False
        self.enable_marionette_trace = self.config.get("enable_marionette_trace")
        self.browser_cycles = self.config.get("browser_cycles")
        self.clean = self.config.get("clean")

        for (arg,), details in Raptor.browsertime_options:
            # Allow overriding defaults on the `./mach raptor-test ...` command-line.
            value = self.config.get(details["dest"])
            if value and arg not in self.config.get("raptor_cmd_line_args", []):
                setattr(self, details["dest"], value)

    # We accept some configuration options from the try commit message in the
    # format mozharness: <options>. Example try commit message: mozharness:
    # --geckoProfile try: <stuff>
    def query_gecko_profile_options(self):
        gecko_results = []
        # If gecko_profile is set, we add that to Raptor's options
        if self.gecko_profile:
            gecko_results.append("--gecko-profile")
            if self.gecko_profile_interval:
                gecko_results.extend(
                    ["--gecko-profile-interval", str(self.gecko_profile_interval)]
                )
            if self.gecko_profile_entries:
                gecko_results.extend(
                    ["--gecko-profile-entries", str(self.gecko_profile_entries)]
                )
            if self.gecko_profile_features:
                gecko_results.extend(
                    ["--gecko-profile-features", self.gecko_profile_features]
                )
            if self.gecko_profile_threads:
                gecko_results.extend(
                    ["--gecko-profile-threads", self.gecko_profile_threads]
                )
        else:
            if self.extra_profiler_run:
                gecko_results.append("--extra-profiler-run")
        return gecko_results

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(Raptor, self).query_abs_dirs()
        abs_dirs["abs_blob_upload_dir"] = os.path.join(
            abs_dirs["abs_work_dir"], "blobber_upload_dir"
        )
        abs_dirs["abs_test_install_dir"] = os.path.join(
            abs_dirs["abs_work_dir"], "tests"
        )

        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def install_chrome_android(self):
        """Install Google Chrome for Android in production from tooltool"""
        if self.app != "chrome-m":
            self.info("Google Chrome for Android not required")
            return
        if self.config.get("run_local"):
            self.info(
                "Google Chrome for Android will not be installed "
                "from tooltool when running locally"
            )
            return
        self.info("Fetching and installing Google Chrome for Android")

        # Fetch the APK
        tmpdir = tempfile.mkdtemp()
        self.tooltool_fetch(
            os.path.join(
                self.raptor_path,
                "raptor",
                "tooltool-manifests",
                "chrome-android",
                "chrome87.manifest",
            ),
            output_dir=tmpdir,
        )

        # Find the downloaded APK
        files = os.listdir(tmpdir)
        if len(files) > 1:
            raise Exception(
                "Found more than one chrome APK file after tooltool download"
            )
        chromeapk = os.path.join(tmpdir, files[0])

        # Disable verification and install the APK
        self.device.shell_output("settings put global verifier_verify_adb_installs 0")
        self.install_android_app(chromeapk, replace=True)

        # Re-enable verification and delete the temporary directory
        self.device.shell_output("settings put global verifier_verify_adb_installs 1")
        rmtree(tmpdir)

        self.info("Google Chrome for Android successfully installed")

    def install_chromium_distribution(self):
        """Install Google Chromium distribution in production"""
        linux, mac, win = "linux", "mac", "win"
        chrome, chromium = "chrome", "chromium"

        available_chromium_dists = [chrome, chromium]
        binary_location = {
            chromium: {
                linux: ["chrome-linux", "chrome"],
                mac: ["chrome-mac", "Chromium.app", "Contents", "MacOS", "Chromium"],
                win: ["chrome-win", "Chrome.exe"],
            },
        }

        if self.app not in available_chromium_dists:
            self.info("Google Chrome or Chromium distributions are not required.")
            return

        if self.app == "chrome":
            self.info("Chrome should be preinstalled.")
            if win in self.platform_name():
                base_path = "C:\\%s\\Google\\Chrome\\Application\\chrome.exe"
                self.chromium_dist_path = base_path % "Progra~1"
                if not os.path.exists(self.chromium_dist_path):
                    self.chromium_dist_path = base_path % "Progra~2"
            elif linux in self.platform_name():
                self.chromium_dist_path = "/usr/bin/google-chrome"
            elif mac in self.platform_name():
                self.chromium_dist_path = (
                    "/Applications/Google Chrome.app/" "Contents/MacOS/Google Chrome"
                )
            else:
                self.error(
                    "Chrome is not installed on the platform %s yet."
                    % self.platform_name()
                )

            if os.path.exists(self.chromium_dist_path):
                self.info(
                    "Google Chrome found in expected location %s"
                    % self.chromium_dist_path
                )
            else:
                self.error("Cannot find Google Chrome at %s" % self.chromium_dist_path)

            return

        chromium_dist = self.app

        if self.config.get("run_local"):
            self.info("Expecting %s to be pre-installed locally" % chromium_dist)
            return

        self.info("Getting fetched %s build" % chromium_dist)
        self.chromium_dist_dest = os.path.normpath(
            os.path.abspath(os.environ["MOZ_FETCHES_DIR"])
        )

        if mac in self.platform_name():
            self.chromium_dist_path = os.path.join(
                self.chromium_dist_dest, *binary_location[chromium_dist][mac]
            )

        elif linux in self.platform_name():
            self.chromium_dist_path = os.path.join(
                self.chromium_dist_dest, *binary_location[chromium_dist][linux]
            )

        else:
            self.chromium_dist_path = os.path.join(
                self.chromium_dist_dest, *binary_location[chromium_dist][win]
            )

        self.info("%s dest is: %s" % (chromium_dist, self.chromium_dist_dest))
        self.info("%s path is: %s" % (chromium_dist, self.chromium_dist_path))

        # Now ensure Chromium binary exists
        if os.path.exists(self.chromium_dist_path):
            self.info(
                "Successfully installed %s to: %s"
                % (chromium_dist, self.chromium_dist_path)
            )
        else:
            self.info("Abort: failed to install %s" % chromium_dist)

    def raptor_options(self, args=None, **kw):
        """Return options to Raptor"""
        options = []
        kw_options = {}

        # Get the APK location to be able to get the browser version
        # through mozversion
        if self.app in self.firefox_android_browsers and not self.run_local:
            kw_options["installerpath"] = self.installer_path

        # If testing on Firefox, the binary path already came from mozharness/pro;
        # otherwise the binary path is forwarded from command-line arg (raptor_cmd_line_args).
        kw_options["app"] = self.app
        if self.app == "firefox" or (
            self.app in self.firefox_android_browsers and not self.run_local
        ):
            binary_path = self.binary_path or self.config.get("binary_path")
            if not binary_path:
                self.fatal("Raptor requires a path to the binary.")
            kw_options["binary"] = binary_path
            if self.app in self.firefox_android_browsers:
                # In production ensure we have correct app name,
                # i.e. fennec_aurora or fennec_release etc.
                kw_options["binary"] = self.query_package_name()
                self.info(
                    "Set binary to %s instead of %s"
                    % (kw_options["binary"], binary_path)
                )
        else:  # Running on Chromium
            if not self.run_local:
                # When running locally we already set the Chromium binary above, in init.
                # In production, we already installed Chromium, so set the binary path
                # to our install.
                kw_options["binary"] = self.chromium_dist_path or ""

        # Options overwritten from **kw
        if "test" in self.config:
            kw_options["test"] = self.config["test"]
        if "binary" in self.config:
            kw_options["binary"] = self.config["binary"]
        if self.symbols_path:
            kw_options["symbolsPath"] = self.symbols_path
        if self.config.get("obj_path", None) is not None:
            kw_options["obj-path"] = self.config["obj_path"]
        if self.test_url_params:
            kw_options["test-url-params"] = self.test_url_params
        if self.config.get("device_name") is not None:
            kw_options["device-name"] = self.config["device_name"]
        if self.config.get("activity") is not None:
            kw_options["activity"] = self.config["activity"]
        if self.config.get("conditioned_profile") is not None:
            kw_options["conditioned-profile"] = self.config["conditioned_profile"]

        kw_options.update(kw)
        if self.host:
            kw_options["host"] = self.host
        # Configure profiling options
        options.extend(self.query_gecko_profile_options())
        # Extra arguments
        if args is not None:
            options += args

        if self.config.get("run_local", False):
            options.extend(["--run-local"])
        if "raptor_cmd_line_args" in self.config:
            options += self.config["raptor_cmd_line_args"]
        if self.config.get("code_coverage", False):
            options.extend(["--code-coverage"])
        if self.config.get("is_release_build", False):
            options.extend(["--is-release-build"])
        if self.config.get("power_test", False):
            options.extend(["--power-test"])
        if self.config.get("memory_test", False):
            options.extend(["--memory-test"])
        if self.config.get("cpu_test", False):
            options.extend(["--cpu-test"])
        if self.config.get("live_sites", False):
            options.extend(["--live-sites"])
        if self.config.get("chimera", False):
            options.extend(["--chimera"])
        if self.config.get("disable_perf_tuning", False):
            options.extend(["--disable-perf-tuning"])
        if self.config.get("cold", False):
            options.extend(["--cold"])
        if not self.config.get("fission", True):
            options.extend(["--disable-fission"])
        if self.config.get("verbose", False):
            options.extend(["--verbose"])
        if self.config.get("extra_prefs"):
            options.extend(
                ["--setpref={}".format(i) for i in self.config.get("extra_prefs")]
            )
        if self.config.get("environment"):
            options.extend(
                ["--setenv={}".format(i) for i in self.config.get("environment")]
            )
        if self.config.get("enable_marionette_trace", False):
            options.extend(["--enable-marionette-trace"])
        if self.config.get("browser_cycles"):
            options.extend(
                ["--browser-cycles={}".format(self.config.get("browser_cycles"))]
            )
        if self.config.get("test_bytecode_cache", False):
            options.extend(["--test-bytecode-cache"])
        if self.config.get("collect_perfstats", False):
            options.extend(["--collect-perfstats"])

        if self.config.get("webext", False):
            options.extend(["--webext"])
        else:
            for (arg,), details in Raptor.browsertime_options:
                # Allow overriding defaults on the `./mach raptor-test ...` command-line
                value = self.config.get(details["dest"])
                if value is None or value != getattr(self, details["dest"], None):
                    # Check for modifications done to the instance variables
                    value = getattr(self, details["dest"], None)
                if value and arg not in self.config.get("raptor_cmd_line_args", []):
                    if isinstance(value, string_types):
                        options.extend([arg, os.path.expandvars(value)])
                    elif isinstance(value, (tuple, list)):
                        for val in value:
                            options.extend([arg, val])
                    else:
                        options.extend([arg])

        for key, value in kw_options.items():
            options.extend(["--%s" % key, value])

        return options

    def populate_webroot(self):
        """Populate the production test machines' webroots"""
        self.raptor_path = os.path.join(
            self.query_abs_dirs()["abs_test_install_dir"], "raptor"
        )
        if self.config.get("run_local"):
            self.raptor_path = os.path.join(self.repo_path, "testing", "raptor")

    def clobber(self):
        # Recreate the upload directory for storing the logcat collected
        # during APK installation.
        super(Raptor, self).clobber()
        upload_dir = self.query_abs_dirs()["abs_blob_upload_dir"]
        if not os.path.isdir(upload_dir):
            self.mkdir_p(upload_dir)

    def install_android_app(self, apk, replace=False):
        # Override AndroidMixin's install_android_app in order to capture
        # logcat during the installation. If the installation fails,
        # the logcat file will be left in the upload directory.
        self.logcat_start()
        try:
            super(Raptor, self).install_android_app(apk, replace=replace)
        finally:
            self.logcat_stop()

    def download_and_extract(self, extract_dirs=None, suite_categories=None):
        return super(Raptor, self).download_and_extract(
            suite_categories=["common", "condprof", "raptor"]
        )

    def create_virtualenv(self, **kwargs):
        """VirtualenvMixin.create_virtualenv() assumes we're using
        self.config['virtualenv_modules']. Since we're installing
        raptor from its source, we have to wrap that method here."""
        # If virtualenv already exists, just add to path and don't re-install.
        # We need it in-path to import jsonschema later when validating output for perfherder.
        _virtualenv_path = self.config.get("virtualenv_path")

        if self.clean:
            rmtree(_virtualenv_path, ignore_errors=True)

        if self.run_local and os.path.exists(_virtualenv_path):
            self.info("Virtualenv already exists, skipping creation")
            # ffmpeg exists outside of this virtual environment so
            # we re-add it to the platform environment on repeated
            # local runs of browsertime visual metric tests
            self.setup_local_ffmpeg()
            _python_interp = self.config.get("exes")["python"]

            if "win" in self.platform_name():
                _path = os.path.join(_virtualenv_path, "Lib", "site-packages")
            else:
                _path = os.path.join(
                    _virtualenv_path,
                    "lib",
                    os.path.basename(_python_interp),
                    "site-packages",
                )

            sys.path.append(_path)
            return

        # virtualenv doesn't already exist so create it
        # Install mozbase first, so we use in-tree versions
        if not self.run_local:
            mozbase_requirements = os.path.join(
                self.query_abs_dirs()["abs_test_install_dir"],
                "config",
                "mozbase_requirements.txt",
            )
        else:
            mozbase_requirements = os.path.join(
                os.path.dirname(self.raptor_path),
                "config",
                "mozbase_source_requirements.txt",
            )
        self.register_virtualenv_module(
            requirements=[mozbase_requirements],
            two_pass=True,
            editable=True,
        )

        modules = ["pip>=1.5"]

        # Add modules required for visual metrics
        py3_minor = sys.version_info.minor
        if py3_minor <= 7:
            modules.extend(
                [
                    "numpy==1.16.1",
                    "Pillow==6.1.0",
                    "scipy==1.2.3",
                    "pyssim==0.4",
                    "opencv-python==4.5.4.60",
                ]
            )
        else:  # python version >= 3.8
            modules.extend(
                [
                    "numpy==1.22.0",
                    "Pillow==9.0.0",
                    "scipy==1.7.3",
                    "pyssim==0.4",
                    "opencv-python==4.5.4.60",
                ]
            )

        if self.run_local:
            self.setup_local_ffmpeg()

        # Require pip >= 1.5 so pip will prefer .whl files to install
        super(Raptor, self).create_virtualenv(modules=modules)

        # Install Raptor dependencies
        self.install_module(
            requirements=[os.path.join(self.raptor_path, "requirements.txt")]
        )

    def setup_local_ffmpeg(self):
        """Make use of the users local ffmpeg when running browsertime visual
        metrics tests.
        """

        if "ffmpeg" in os.environ["PATH"]:
            return

        platform = self.platform_name()
        btime_cache = os.path.join(self.config["mozbuild_path"], "browsertime")
        if "mac" in platform:
            path_to_ffmpeg = os.path.join(
                btime_cache,
                FFMPEG_LOCAL_CACHE["mac"],
                "bin",
            )
        elif "linux" in platform:
            path_to_ffmpeg = os.path.join(
                btime_cache,
                FFMPEG_LOCAL_CACHE["linux"],
            )
        elif "win" in platform:
            path_to_ffmpeg = os.path.join(
                btime_cache,
                FFMPEG_LOCAL_CACHE["win"],
                "bin",
            )

        if os.path.exists(path_to_ffmpeg):
            os.environ["PATH"] += os.pathsep + path_to_ffmpeg
            self.browsertime_ffmpeg = path_to_ffmpeg
            self.info(
                "Added local ffmpeg found at: %s to environment." % path_to_ffmpeg
            )
        else:
            raise Exception(
                "No local ffmpeg binary found. Expected it to be here: %s"
                % path_to_ffmpeg
            )

    def install(self):
        if not self.config.get("noinstall", False):
            if self.app in self.firefox_android_browsers:
                self.device.uninstall_app(self.binary_path)
                self.install_android_app(self.installer_path)
            else:
                super(Raptor, self).install()

    def _artifact_perf_data(self, src, dest):
        if not os.path.isdir(os.path.dirname(dest)):
            # create upload dir if it doesn't already exist
            self.info("Creating dir: %s" % os.path.dirname(dest))
            os.makedirs(os.path.dirname(dest))
        self.info("Copying raptor results from %s to %s" % (src, dest))
        try:
            copyfile(src, dest)
        except Exception as e:
            self.critical("Error copying results %s to upload dir %s" % (src, dest))
            self.info(str(e))

    def run_tests(self, args=None, **kw):
        """Run raptor tests"""

        # Get Raptor options
        options = self.raptor_options(args=args, **kw)

        # Python version check
        python = self.query_python_path()
        self.run_command([python, "--version"])
        parser = RaptorOutputParser(
            config=self.config, log_obj=self.log_obj, error_list=RaptorErrorList
        )
        env = {}
        env["MOZ_UPLOAD_DIR"] = self.query_abs_dirs()["abs_blob_upload_dir"]
        if not self.run_local:
            env["MINIDUMP_STACKWALK"] = self.query_minidump_stackwalk()
        env["MINIDUMP_SAVE_PATH"] = self.query_abs_dirs()["abs_blob_upload_dir"]
        env["RUST_BACKTRACE"] = "full"
        if not os.path.isdir(env["MOZ_UPLOAD_DIR"]):
            self.mkdir_p(env["MOZ_UPLOAD_DIR"])
        env = self.query_env(partial_env=env, log_level=INFO)
        # adjust PYTHONPATH to be able to use raptor as a python package
        if "PYTHONPATH" in env:
            env["PYTHONPATH"] = self.raptor_path + os.pathsep + env["PYTHONPATH"]
        else:
            env["PYTHONPATH"] = self.raptor_path

        # mitmproxy needs path to mozharness when installing the cert, and tooltool
        env["SCRIPTSPATH"] = scripts_path
        env["EXTERNALTOOLSPATH"] = external_tools_path

        # Needed to load unsigned Raptor WebExt on release builds
        if self.is_release_build:
            env["MOZ_DISABLE_NONLOCAL_CONNECTIONS"] = "1"

        if self.repo_path is not None:
            env["MOZ_DEVELOPER_REPO_DIR"] = self.repo_path
        if self.obj_path is not None:
            env["MOZ_DEVELOPER_OBJ_DIR"] = self.obj_path

        # Sets a timeout for how long Raptor should run without output
        output_timeout = self.config.get("raptor_output_timeout", 3600)
        # Run Raptor tests
        run_tests = os.path.join(self.raptor_path, "raptor", "raptor.py")

        mozlog_opts = ["--log-tbpl-level=debug"]
        if not self.run_local and "suite" in self.config:
            fname_pattern = "%s_%%s.log" % self.config["test"]
            mozlog_opts.append(
                "--log-errorsummary=%s"
                % os.path.join(env["MOZ_UPLOAD_DIR"], fname_pattern % "errorsummary")
            )
            mozlog_opts.append(
                "--log-raw=%s"
                % os.path.join(env["MOZ_UPLOAD_DIR"], fname_pattern % "raw")
            )

        def launch_in_debug_mode(cmdline):
            cmdline = set(cmdline)
            debug_opts = {"--debug", "--debugger", "--debugger_args"}

            return bool(debug_opts.intersection(cmdline))

        if self.app in self.android_browsers:
            self.logcat_start()

        command = [python, run_tests] + options + mozlog_opts
        if launch_in_debug_mode(command):
            raptor_process = subprocess.Popen(command, cwd=self.workdir, env=env)
            raptor_process.wait()
        else:
            self.return_code = self.run_command(
                command,
                cwd=self.workdir,
                output_timeout=output_timeout,
                output_parser=parser,
                env=env,
            )

        if self.app in self.android_browsers:
            self.logcat_stop()

        if parser.minidump_output:
            self.info("Looking at the minidump files for debugging purposes...")
            for item in parser.minidump_output:
                self.run_command(["ls", "-l", item])

        elif not self.run_local:
            # Copy results to upload dir so they are included as an artifact
            self.info("Copying Raptor results to upload dir:")

            src = os.path.join(self.query_abs_dirs()["abs_work_dir"], "raptor.json")
            dest = os.path.join(env["MOZ_UPLOAD_DIR"], "perfherder-data.json")
            self.info(str(dest))
            self._artifact_perf_data(src, dest)

            # Make individual perfherder data JSON's for each supporting data type
            for file in glob.glob(
                os.path.join(self.query_abs_dirs()["abs_work_dir"], "*")
            ):
                path, filename = os.path.split(file)

                if not filename.startswith("raptor-"):
                    continue

                # filename is expected to contain a unique data name
                # i.e. raptor-os-baseline-power.json would result in
                # the data name os-baseline-power
                data_name = "-".join(filename.split("-")[1:])
                data_name = ".".join(data_name.split(".")[:-1])

                src = file
                dest = os.path.join(
                    env["MOZ_UPLOAD_DIR"], "perfherder-data-%s.json" % data_name
                )
                self._artifact_perf_data(src, dest)

            src = os.path.join(
                self.query_abs_dirs()["abs_work_dir"], "screenshots.html"
            )
            if os.path.exists(src):
                dest = os.path.join(env["MOZ_UPLOAD_DIR"], "screenshots.html")
                self.info(str(dest))
                self._artifact_perf_data(src, dest)

        # Allow log failures to over-ride successful runs of the test harness and
        # give log failures priority, so that, for instance, log failures resulting
        # in TBPL_RETRY cause a retry rather than simply reporting an error.
        if parser.tbpl_status != TBPL_SUCCESS:
            parser_status = EXIT_STATUS_DICT[parser.tbpl_status]
            self.info(
                "return code %s changed to %s due to log output"
                % (str(self.return_code), str(parser_status))
            )
            self.return_code = parser_status


class RaptorOutputParser(OutputParser):
    minidump_regex = re.compile(
        r'''raptorError: "error executing: '(\S+) (\S+) (\S+)'"'''
    )
    RE_PERF_DATA = re.compile(r".*PERFHERDER_DATA:\s+(\{.*\})")

    def __init__(self, **kwargs):
        super(RaptorOutputParser, self).__init__(**kwargs)
        self.minidump_output = None
        self.found_perf_data = []
        self.tbpl_status = TBPL_SUCCESS
        self.worst_log_level = INFO
        self.harness_retry_re = TinderBoxPrintRe["harness_error"]["retry_regex"]

    def parse_single_line(self, line):
        m = self.minidump_regex.search(line)
        if m:
            self.minidump_output = (m.group(1), m.group(2), m.group(3))

        m = self.RE_PERF_DATA.match(line)
        if m:
            self.found_perf_data.append(m.group(1))

        if self.harness_retry_re.search(line):
            self.critical(" %s" % line)
            self.worst_log_level = self.worst_level(CRITICAL, self.worst_log_level)
            self.tbpl_status = self.worst_level(
                TBPL_RETRY, self.tbpl_status, levels=TBPL_WORST_LEVEL_TUPLE
            )
            return  # skip base parse_single_line
        super(RaptorOutputParser, self).parse_single_line(line)
