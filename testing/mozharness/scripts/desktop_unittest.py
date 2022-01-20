#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,

# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""desktop_unittest.py

author: Jordan Lund
"""

from __future__ import absolute_import
import json
import os
import re
import sys
import copy
import shutil
import glob
import imp

from datetime import datetime, timedelta

# load modules from parent dir
here = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(1, os.path.dirname(here))

from mozharness.base.errors import BaseErrorList
from mozharness.base.log import INFO, WARNING
from mozharness.base.script import PreScriptAction
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.automation import TBPL_EXCEPTION, TBPL_RETRY
from mozharness.mozilla.mozbase import MozbaseMixin
from mozharness.mozilla.structuredlog import StructuredOutputParser
from mozharness.mozilla.testing.errors import HarnessErrorList
from mozharness.mozilla.testing.unittest import DesktopUnittestOutputParser
from mozharness.mozilla.testing.codecoverage import (
    CodeCoverageMixin,
    code_coverage_config_options,
)
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options

PY2 = sys.version_info.major == 2
SUITE_CATEGORIES = [
    "gtest",
    "cppunittest",
    "jittest",
    "mochitest",
    "reftest",
    "xpcshell",
]
SUITE_DEFAULT_E10S = ["mochitest", "reftest"]
SUITE_NO_E10S = ["xpcshell"]
SUITE_REPEATABLE = ["mochitest", "reftest"]


# DesktopUnittest {{{1
class DesktopUnittest(TestingMixin, MercurialScript, MozbaseMixin, CodeCoverageMixin):
    config_options = (
        [
            [
                [
                    "--mochitest-suite",
                ],
                {
                    "action": "extend",
                    "dest": "specified_mochitest_suites",
                    "type": "string",
                    "help": "Specify which mochi suite to run. "
                    "Suites are defined in the config file.\n"
                    "Examples: 'all', 'plain1', 'plain5', 'chrome', or 'a11y'",
                },
            ],
            [
                [
                    "--reftest-suite",
                ],
                {
                    "action": "extend",
                    "dest": "specified_reftest_suites",
                    "type": "string",
                    "help": "Specify which reftest suite to run. "
                    "Suites are defined in the config file.\n"
                    "Examples: 'all', 'crashplan', or 'jsreftest'",
                },
            ],
            [
                [
                    "--xpcshell-suite",
                ],
                {
                    "action": "extend",
                    "dest": "specified_xpcshell_suites",
                    "type": "string",
                    "help": "Specify which xpcshell suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'xpcshell'",
                },
            ],
            [
                [
                    "--cppunittest-suite",
                ],
                {
                    "action": "extend",
                    "dest": "specified_cppunittest_suites",
                    "type": "string",
                    "help": "Specify which cpp unittest suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'cppunittest'",
                },
            ],
            [
                [
                    "--gtest-suite",
                ],
                {
                    "action": "extend",
                    "dest": "specified_gtest_suites",
                    "type": "string",
                    "help": "Specify which gtest suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'gtest'",
                },
            ],
            [
                [
                    "--jittest-suite",
                ],
                {
                    "action": "extend",
                    "dest": "specified_jittest_suites",
                    "type": "string",
                    "help": "Specify which jit-test suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'jittest'",
                },
            ],
            [
                [
                    "--run-all-suites",
                ],
                {
                    "action": "store_true",
                    "dest": "run_all_suites",
                    "default": False,
                    "help": "This will run all suites that are specified "
                    "in the config file. You do not need to specify "
                    "any other suites.\nBeware, this may take a while ;)",
                },
            ],
            [
                [
                    "--disable-e10s",
                ],
                {
                    "action": "store_false",
                    "dest": "e10s",
                    "default": True,
                    "help": "Run tests without multiple processes (e10s).",
                },
            ],
            [
                [
                    "--headless",
                ],
                {
                    "action": "store_true",
                    "dest": "headless",
                    "default": False,
                    "help": "Run tests in headless mode.",
                },
            ],
            [
                [
                    "--no-random",
                ],
                {
                    "action": "store_true",
                    "dest": "no_random",
                    "default": False,
                    "help": "Run tests with no random intermittents and bisect in case of real failure.",  # NOQA: E501
                },
            ],
            [
                ["--total-chunks"],
                {
                    "action": "store",
                    "dest": "total_chunks",
                    "help": "Number of total chunks",
                },
            ],
            [
                ["--this-chunk"],
                {
                    "action": "store",
                    "dest": "this_chunk",
                    "help": "Number of this chunk",
                },
            ],
            [
                ["--allow-software-gl-layers"],
                {
                    "action": "store_true",
                    "dest": "allow_software_gl_layers",
                    "default": False,
                    "help": "Permits a software GL implementation (such as LLVMPipe) to use "
                    "the GL compositor.",
                },
            ],
            [
                ["--enable-webrender"],
                {
                    "action": "store_true",
                    "dest": "enable_webrender",
                    "default": False,
                    "help": "Enable the WebRender compositor in Gecko.",
                },
            ],
            [
                ["--gpu-required"],
                {
                    "action": "store_true",
                    "dest": "gpu_required",
                    "default": False,
                    "help": "Run additional verification on modified tests using gpu instances.",
                },
            ],
            [
                ["--setpref"],
                {
                    "action": "append",
                    "metavar": "PREF=VALUE",
                    "dest": "extra_prefs",
                    "default": [],
                    "help": "Defines an extra user preference.",
                },
            ],
            [
                [
                    "--repeat",
                ],
                {
                    "action": "store",
                    "type": "int",
                    "dest": "repeat",
                    "default": 0,
                    "help": "Repeat the tests the given number of times. Supported "
                    "by mochitest, reftest, crashtest, ignored otherwise.",
                },
            ],
            [
                ["--enable-xorigin-tests"],
                {
                    "action": "store_true",
                    "dest": "enable_xorigin_tests",
                    "default": False,
                    "help": "Run tests in a cross origin iframe.",
                },
            ],
            [
                ["--enable-a11y-checks"],
                {
                    "action": "store_true",
                    "default": False,
                    "dest": "a11y_checks",
                    "help": "Run tests with accessibility checks disabled.",
                },
            ],
            [
                ["--run-failures"],
                {
                    "action": "store",
                    "default": "",
                    "type": "string",
                    "dest": "run_failures",
                    "help": "Run only failures matching keyword. "
                    "Examples: 'apple_silicon'",
                },
            ],
            [
                ["--crash-as-pass"],
                {
                    "action": "store_true",
                    "default": False,
                    "dest": "crash_as_pass",
                    "help": "treat harness level crash as a pass",
                },
            ],
            [
                ["--timeout-as-pass"],
                {
                    "action": "store_true",
                    "default": False,
                    "dest": "timeout_as_pass",
                    "help": "treat harness level timeout as a pass",
                },
            ],
        ]
        + copy.deepcopy(testing_config_options)
        + copy.deepcopy(code_coverage_config_options)
    )

    def __init__(self, require_config_file=True):
        # abs_dirs defined already in BaseScript but is here to make pylint happy
        self.abs_dirs = None
        super(DesktopUnittest, self).__init__(
            config_options=self.config_options,
            all_actions=[
                "clobber",
                "download-and-extract",
                "create-virtualenv",
                "start-pulseaudio",
                "install",
                "stage-files",
                "run-tests",
            ],
            require_config_file=require_config_file,
            config={"require_test_zip": True},
        )

        c = self.config
        self.global_test_options = []
        self.installer_url = c.get("installer_url")
        self.test_url = c.get("test_url")
        self.test_packages_url = c.get("test_packages_url")
        self.symbols_url = c.get("symbols_url")
        # this is so mozinstall in install() doesn't bug out if we don't run
        # the download_and_extract action
        self.installer_path = c.get("installer_path")
        self.binary_path = c.get("binary_path")
        self.abs_app_dir = None
        self.abs_res_dir = None

        # Construct an identifier to be used to identify Perfherder data
        # for resource monitoring recording. This attempts to uniquely
        # identify this test invocation configuration.
        perfherder_parts = []
        perfherder_options = []
        suites = (
            ("specified_mochitest_suites", "mochitest"),
            ("specified_reftest_suites", "reftest"),
            ("specified_xpcshell_suites", "xpcshell"),
            ("specified_cppunittest_suites", "cppunit"),
            ("specified_gtest_suites", "gtest"),
            ("specified_jittest_suites", "jittest"),
        )
        for s, prefix in suites:
            if s in c:
                perfherder_parts.append(prefix)
                perfherder_parts.extend(c[s])

        if "this_chunk" in c:
            perfherder_parts.append(c["this_chunk"])

        if c["e10s"]:
            perfherder_options.append("e10s")

        self.resource_monitor_perfherder_id = (
            ".".join(perfherder_parts),
            perfherder_options,
        )

    # helper methods {{{2
    def _pre_config_lock(self, rw_config):
        super(DesktopUnittest, self)._pre_config_lock(rw_config)
        c = self.config
        if not c.get("run_all_suites"):
            return  # configs are valid
        for category in SUITE_CATEGORIES:
            specific_suites = c.get("specified_%s_suites" % (category))
            if specific_suites:
                if specific_suites != "all":
                    self.fatal(
                        "Config options are not valid. Please ensure"
                        " that if the '--run-all-suites' flag was enabled,"
                        " then do not specify to run only specific suites "
                        "like:\n '--mochitest-suite browser-chrome'"
                    )

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(DesktopUnittest, self).query_abs_dirs()

        c = self.config
        dirs = {}
        dirs["abs_work_dir"] = abs_dirs["abs_work_dir"]
        dirs["abs_app_install_dir"] = os.path.join(
            abs_dirs["abs_work_dir"], "application"
        )
        dirs["abs_test_install_dir"] = os.path.join(abs_dirs["abs_work_dir"], "tests")
        dirs["abs_test_extensions_dir"] = os.path.join(
            dirs["abs_test_install_dir"], "extensions"
        )
        dirs["abs_test_bin_dir"] = os.path.join(dirs["abs_test_install_dir"], "bin")
        dirs["abs_test_bin_plugins_dir"] = os.path.join(
            dirs["abs_test_bin_dir"], "plugins"
        )
        dirs["abs_test_bin_components_dir"] = os.path.join(
            dirs["abs_test_bin_dir"], "components"
        )
        dirs["abs_mochitest_dir"] = os.path.join(
            dirs["abs_test_install_dir"], "mochitest"
        )
        dirs["abs_reftest_dir"] = os.path.join(dirs["abs_test_install_dir"], "reftest")
        dirs["abs_xpcshell_dir"] = os.path.join(
            dirs["abs_test_install_dir"], "xpcshell"
        )
        dirs["abs_cppunittest_dir"] = os.path.join(
            dirs["abs_test_install_dir"], "cppunittest"
        )
        dirs["abs_gtest_dir"] = os.path.join(dirs["abs_test_install_dir"], "gtest")
        dirs["abs_blob_upload_dir"] = os.path.join(
            abs_dirs["abs_work_dir"], "blobber_upload_dir"
        )
        dirs["abs_jittest_dir"] = os.path.join(
            dirs["abs_test_install_dir"], "jit-test", "jit-test"
        )

        if os.path.isabs(c["virtualenv_path"]):
            dirs["abs_virtualenv_dir"] = c["virtualenv_path"]
        else:
            dirs["abs_virtualenv_dir"] = os.path.join(
                abs_dirs["abs_work_dir"], c["virtualenv_path"]
            )
        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs

        return self.abs_dirs

    def query_abs_app_dir(self):
        """We can't set this in advance, because OSX install directories
        change depending on branding and opt/debug.
        """
        if self.abs_app_dir:
            return self.abs_app_dir
        if not self.binary_path:
            self.fatal("Can't determine abs_app_dir (binary_path not set!)")
        self.abs_app_dir = os.path.dirname(self.binary_path)
        return self.abs_app_dir

    def query_abs_res_dir(self):
        """The directory containing resources like plugins and extensions. On
        OSX this is Contents/Resources, on all other platforms its the same as
        the app dir.

        As with the app dir, we can't set this in advance, because OSX install
        directories change depending on branding and opt/debug.
        """
        if self.abs_res_dir:
            return self.abs_res_dir

        abs_app_dir = self.query_abs_app_dir()
        if self._is_darwin():
            res_subdir = self.config.get("mac_res_subdir", "Resources")
            self.abs_res_dir = os.path.join(os.path.dirname(abs_app_dir), res_subdir)
        else:
            self.abs_res_dir = abs_app_dir
        return self.abs_res_dir

    @PreScriptAction("create-virtualenv")
    def _pre_create_virtualenv(self, action):
        dirs = self.query_abs_dirs()

        self.register_virtualenv_module(name="mock")
        self.register_virtualenv_module(name="simplejson")

        requirements_files = [
            os.path.join(
                dirs["abs_test_install_dir"], "config", "marionette_requirements.txt"
            )
        ]

        if self._query_specified_suites("mochitest") is not None:
            # mochitest is the only thing that needs this
            if PY2:
                wspb_requirements = "websocketprocessbridge_requirements.txt"
            else:
                wspb_requirements = "websocketprocessbridge_requirements_3.txt"
            requirements_files.append(
                os.path.join(
                    dirs["abs_mochitest_dir"],
                    "websocketprocessbridge",
                    wspb_requirements,
                )
            )

        for requirements_file in requirements_files:
            self.register_virtualenv_module(
                requirements=[requirements_file], two_pass=True
            )

    def _query_symbols_url(self):
        """query the full symbols URL based upon binary URL"""
        # may break with name convention changes but is one less 'input' for script
        if self.symbols_url:
            return self.symbols_url

        symbols_url = None
        self.info("finding symbols_url based upon self.installer_url")
        if self.installer_url:
            for ext in [".zip", ".dmg", ".tar.bz2"]:
                if ext in self.installer_url:
                    symbols_url = self.installer_url.replace(
                        ext, ".crashreporter-symbols.zip"
                    )
            if not symbols_url:
                self.fatal(
                    "self.installer_url was found but symbols_url could \
                        not be determined"
                )
        else:
            self.fatal("self.installer_url was not found in self.config")
        self.info("setting symbols_url as %s" % (symbols_url))
        self.symbols_url = symbols_url
        return self.symbols_url

    def _get_mozharness_test_paths(self, suite_category, suite):
        test_paths = json.loads(os.environ.get("MOZHARNESS_TEST_PATHS", '""'))

        if "-coverage" in suite:
            suite = suite[: suite.index("-coverage")]

        if not test_paths or suite not in test_paths:
            return None

        suite_test_paths = test_paths[suite]

        if suite_category == "reftest":
            dirs = self.query_abs_dirs()
            suite_test_paths = [
                os.path.join(dirs["abs_reftest_dir"], "tests", p)
                for p in suite_test_paths
            ]

        return suite_test_paths

    def _query_abs_base_cmd(self, suite_category, suite):
        if self.binary_path:
            c = self.config
            dirs = self.query_abs_dirs()
            run_file = c["run_file_names"][suite_category]
            base_cmd = [self.query_python_path("python"), "-u"]
            base_cmd.append(os.path.join(dirs["abs_%s_dir" % suite_category], run_file))
            abs_app_dir = self.query_abs_app_dir()
            abs_res_dir = self.query_abs_res_dir()

            raw_log_file, error_summary_file = self.get_indexed_logs(
                dirs["abs_blob_upload_dir"], suite
            )

            str_format_values = {
                "binary_path": self.binary_path,
                "symbols_path": self._query_symbols_url(),
                "abs_work_dir": dirs["abs_work_dir"],
                "abs_app_dir": abs_app_dir,
                "abs_res_dir": abs_res_dir,
                "raw_log_file": raw_log_file,
                "error_summary_file": error_summary_file,
                "gtest_dir": os.path.join(dirs["abs_test_install_dir"], "gtest"),
            }

            # TestingMixin._download_and_extract_symbols() will set
            # self.symbols_path when downloading/extracting.
            if self.symbols_path:
                str_format_values["symbols_path"] = self.symbols_path

            if suite_category not in SUITE_NO_E10S:
                if suite_category in SUITE_DEFAULT_E10S and not c["e10s"]:
                    base_cmd.append("--disable-e10s")
                elif suite_category not in SUITE_DEFAULT_E10S and c["e10s"]:
                    base_cmd.append("--e10s")
            if c.get("repeat"):
                if suite_category in SUITE_REPEATABLE:
                    base_cmd.extend(["--repeat=%s" % c.get("repeat")])
                else:
                    self.log(
                        "--repeat not supported in {}".format(suite_category),
                        level=WARNING,
                    )

            # Ignore chunking if we have user specified test paths
            if not (self.verify_enabled or self.per_test_coverage):
                test_paths = self._get_mozharness_test_paths(suite_category, suite)
                if test_paths:
                    base_cmd.extend(test_paths)
                elif c.get("total_chunks") and c.get("this_chunk"):
                    base_cmd.extend(
                        [
                            "--total-chunks",
                            c["total_chunks"],
                            "--this-chunk",
                            c["this_chunk"],
                        ]
                    )

            if c["no_random"]:
                if suite_category == "mochitest":
                    base_cmd.append("--bisect-chunk=default")
                else:
                    self.warning(
                        "--no-random does not currently work with suites other than "
                        "mochitest."
                    )

            if c["headless"]:
                base_cmd.append("--headless")

            if c["enable_webrender"]:
                base_cmd.append("--enable-webrender")

            if c["enable_xorigin_tests"]:
                base_cmd.append("--enable-xorigin-tests")

            if c["extra_prefs"]:
                base_cmd.extend(["--setpref={}".format(p) for p in c["extra_prefs"]])

            if c["a11y_checks"]:
                base_cmd.append("--enable-a11y-checks")

            if c["run_failures"]:
                base_cmd.extend(["--run-failures={}".format(c["run_failures"])])

            if c["timeout_as_pass"]:
                base_cmd.append("--timeout-as-pass")

            if c["crash_as_pass"]:
                base_cmd.append("--crash-as-pass")

            # set pluginsPath
            abs_res_plugins_dir = os.path.join(abs_res_dir, "plugins")
            str_format_values["test_plugin_path"] = abs_res_plugins_dir

            if suite_category not in c["suite_definitions"]:
                self.fatal("'%s' not defined in the config!")

            if suite in (
                "browser-chrome-coverage",
                "xpcshell-coverage",
                "mochitest-devtools-chrome-coverage",
                "plain-coverage",
            ):
                base_cmd.append("--jscov-dir-prefix=%s" % dirs["abs_blob_upload_dir"])

            options = c["suite_definitions"][suite_category]["options"]
            if options:
                for option in options:
                    option = option % str_format_values
                    if not option.endswith("None"):
                        base_cmd.append(option)
                if self.structured_output(
                    suite_category, self._query_try_flavor(suite_category, suite)
                ):
                    base_cmd.append("--log-raw=-")
                return base_cmd
            else:
                self.warning(
                    "Suite options for %s could not be determined."
                    "\nIf you meant to have options for this suite, "
                    "please make sure they are specified in your "
                    "config under %s_options" % (suite_category, suite_category)
                )

            return base_cmd
        else:
            self.fatal(
                "'binary_path' could not be determined.\n This should "
                "be like '/path/build/application/firefox/firefox'"
                "\nIf you are running this script without the 'install' "
                "action (where binary_path is set), please ensure you are"
                " either:\n(1) specifying it in the config file under "
                "binary_path\n(2) specifying it on command line with the"
                " '--binary-path' flag"
            )

    def _query_specified_suites(self, category):
        """Checks if the provided suite does indeed exist.

        If at least one suite was given and if it does exist, return the suite
        as legitimate and line it up for execution.

        Otherwise, do not run any suites and return a fatal error.
        """
        c = self.config
        all_suites = c.get("all_{}_suites".format(category), None)
        specified_suites = c.get("specified_{}_suites".format(category), None)

        # Bug 1603842 - disallow selection of more than 1 suite at at time
        if specified_suites is None:
            # Path taken by test-verify
            return self.query_per_test_category_suites(category, all_suites)
        if specified_suites and len(specified_suites) > 1:
            self.fatal(
                """Selection of multiple suites is not permitted. \
                       Please select at most 1 test suite."""
            )
            return

        # Normal path taken by most test suites as only one suite is specified
        suite = specified_suites[0]
        if suite not in all_suites:
            self.fatal("""Selected suite does not exist!""")
        return {suite: all_suites[suite]}

    def _query_try_flavor(self, category, suite):
        flavors = {
            "mochitest": [
                ("plain.*", "mochitest"),
                ("browser-chrome.*", "browser-chrome"),
                ("mochitest-devtools-chrome.*", "devtools-chrome"),
                ("chrome", "chrome"),
            ],
            "xpcshell": [("xpcshell", "xpcshell")],
            "reftest": [("reftest", "reftest"), ("crashtest", "crashtest")],
        }
        for suite_pattern, flavor in flavors.get(category, []):
            if re.compile(suite_pattern).match(suite):
                return flavor

    def structured_output(self, suite_category, flavor=None):
        unstructured_flavors = self.config.get("unstructured_flavors")
        if not unstructured_flavors:
            return True
        if suite_category not in unstructured_flavors:
            return True
        if not unstructured_flavors.get(
            suite_category
        ) or flavor in unstructured_flavors.get(suite_category):
            return False
        return True

    def get_test_output_parser(
        self, suite_category, flavor=None, strict=False, **kwargs
    ):
        if not self.structured_output(suite_category, flavor):
            return DesktopUnittestOutputParser(suite_category=suite_category, **kwargs)
        self.info("Structured output parser in use for %s." % suite_category)
        return StructuredOutputParser(
            suite_category=suite_category, strict=strict, **kwargs
        )

    # Actions {{{2

    # clobber defined in BaseScript, deletes mozharness/build if exists
    # preflight_download_and_extract is in TestingMixin.
    # create_virtualenv is in VirtualenvMixin.
    # preflight_install is in TestingMixin.
    # install is in TestingMixin.

    @PreScriptAction("download-and-extract")
    def _pre_download_and_extract(self, action):
        """Abort if --artifact try syntax is used with compiled-code tests"""
        dir = self.query_abs_dirs()["abs_blob_upload_dir"]
        self.mkdir_p(dir)

        if not self.try_message_has_flag("artifact"):
            return
        self.info("Artifact build requested in try syntax.")
        rejected = []
        compiled_code_suites = [
            "cppunit",
            "gtest",
            "jittest",
        ]
        for category in SUITE_CATEGORIES:
            suites = self._query_specified_suites(category) or []
            for suite in suites:
                if any([suite.startswith(c) for c in compiled_code_suites]):
                    rejected.append(suite)
                    break
        if rejected:
            self.record_status(TBPL_EXCEPTION)
            self.fatal(
                "There are specified suites that are incompatible with "
                "--artifact try syntax flag: {}".format(", ".join(rejected)),
                exit_code=self.return_code,
            )

    def download_and_extract(self):
        """
        download and extract test zip / download installer
        optimizes which subfolders to extract from tests archive
        """
        c = self.config

        extract_dirs = None

        if c.get("run_all_suites"):
            target_categories = SUITE_CATEGORIES
        else:
            target_categories = [
                cat
                for cat in SUITE_CATEGORIES
                if self._query_specified_suites(cat) is not None
            ]
        super(DesktopUnittest, self).download_and_extract(
            extract_dirs=extract_dirs, suite_categories=target_categories
        )

    def start_pulseaudio(self):
        command = []
        # Implies that underlying system is Linux.
        if os.environ.get("NEED_PULSEAUDIO") == "true":
            command.extend(
                [
                    "pulseaudio",
                    "--daemonize",
                    "--log-level=4",
                    "--log-time=1",
                    "-vvvvv",
                    "--exit-idle-time=-1",
                ]
            )

            # Only run the initialization for Debian.
            # Ubuntu appears to have an alternate method of starting pulseaudio.
            if self._is_debian():
                self._kill_named_proc("pulseaudio")
                self.run_command(command)

            # All Linux systems need module-null-sink to be loaded, otherwise
            # media tests fail.
            self.run_command("pactl load-module module-null-sink")
            self.run_command("pactl list modules short")

    def stage_files(self):
        for category in SUITE_CATEGORIES:
            suites = self._query_specified_suites(category)
            stage = getattr(self, "_stage_{}".format(category), None)
            if suites and stage:
                stage(suites)

    def _stage_files(self, bin_name=None, fail_if_not_exists=True):
        dirs = self.query_abs_dirs()
        abs_app_dir = self.query_abs_app_dir()

        # For mac these directories are in Contents/Resources, on other
        # platforms abs_res_dir will point to abs_app_dir.
        abs_res_dir = self.query_abs_res_dir()
        abs_res_components_dir = os.path.join(abs_res_dir, "components")
        abs_res_plugins_dir = os.path.join(abs_res_dir, "plugins")
        abs_res_extensions_dir = os.path.join(abs_res_dir, "extensions")

        if bin_name:
            src = os.path.join(dirs["abs_test_bin_dir"], bin_name)
            if os.path.exists(src):
                self.info(
                    "copying %s to %s" % (src, os.path.join(abs_app_dir, bin_name))
                )
                shutil.copy2(src, os.path.join(abs_app_dir, bin_name))
            elif fail_if_not_exists:
                raise OSError("File %s not found" % src)
        self.copytree(
            dirs["abs_test_bin_components_dir"],
            abs_res_components_dir,
            overwrite="overwrite_if_exists",
        )
        self.mkdir_p(abs_res_plugins_dir)
        self.copytree(
            dirs["abs_test_bin_plugins_dir"],
            abs_res_plugins_dir,
            overwrite="overwrite_if_exists",
        )
        if os.path.isdir(dirs["abs_test_extensions_dir"]):
            self.mkdir_p(abs_res_extensions_dir)
            self.copytree(
                dirs["abs_test_extensions_dir"],
                abs_res_extensions_dir,
                overwrite="overwrite_if_exists",
            )

    def _stage_xpcshell(self, suites):
        self._stage_files(self.config["xpcshell_name"])
        # http3server isn't built for Windows tests or Linux asan/tsan
        # builds. Only stage if the `http3server_name` config is set and if
        # the file actually exists.
        if self.config.get("http3server_name"):
            self._stage_files(self.config["http3server_name"], fail_if_not_exists=False)

    def _stage_cppunittest(self, suites):
        abs_res_dir = self.query_abs_res_dir()
        dirs = self.query_abs_dirs()
        abs_cppunittest_dir = dirs["abs_cppunittest_dir"]

        # move manifest and js fils to resources dir, where tests expect them
        files = glob.glob(os.path.join(abs_cppunittest_dir, "*.js"))
        files.extend(glob.glob(os.path.join(abs_cppunittest_dir, "*.manifest")))
        for f in files:
            self.move(f, abs_res_dir)

    def _stage_gtest(self, suites):
        abs_res_dir = self.query_abs_res_dir()
        abs_app_dir = self.query_abs_app_dir()
        dirs = self.query_abs_dirs()
        abs_gtest_dir = dirs["abs_gtest_dir"]
        dirs["abs_test_bin_dir"] = os.path.join(dirs["abs_test_install_dir"], "bin")

        files = glob.glob(os.path.join(dirs["abs_test_bin_plugins_dir"], "gmp-*"))
        files.append(os.path.join(abs_gtest_dir, "dependentlibs.list.gtest"))
        for f in files:
            self.move(f, abs_res_dir)

        self.copytree(
            os.path.join(abs_gtest_dir, "gtest_bin"), os.path.join(abs_app_dir)
        )

    def _kill_proc_tree(self, pid):
        # Kill a process tree (including grandchildren) with signal.SIGTERM
        try:
            import signal
            import psutil

            if pid == os.getpid():
                return (None, None)

            parent = psutil.Process(pid)
            children = parent.children(recursive=True)
            children.append(parent)

            for p in children:
                p.send_signal(signal.SIGTERM)

            # allow for 60 seconds to kill procs
            timeout = 60
            gone, alive = psutil.wait_procs(children, timeout=timeout)
            for p in gone:
                self.info("psutil found pid %s dead" % p.pid)
            for p in alive:
                self.error("failed to kill pid %d after %d" % (p.pid, timeout))

            return (gone, alive)
        except Exception as e:
            self.error("Exception while trying to kill process tree: %s" % str(e))

    def _kill_named_proc(self, pname):
        try:
            import psutil
        except Exception as e:
            self.info(
                "Error importing psutil, not killing process %s: %s" % pname, str(e)
            )
            return

        for proc in psutil.process_iter():
            try:
                if proc.name() == pname:
                    procd = proc.as_dict(attrs=["pid", "ppid", "name", "username"])
                    self.info("in _kill_named_proc, killing %s" % procd)
                    self._kill_proc_tree(proc.pid)
            except Exception as e:
                self.info("Warning: Unable to kill process %s: %s" % (pname, str(e)))
                # may not be able to access process info for all processes
                continue

    def _remove_xen_clipboard(self):
        """
        When running on a Windows 7 VM, we have XenDPriv.exe running which
        interferes with the clipboard, lets terminate this process and remove
        the binary so it doesn't restart
        """
        if not self._is_windows():
            return

        self._kill_named_proc("XenDPriv.exe")
        xenpath = os.path.join(
            os.environ["ProgramFiles"], "Citrix", "XenTools", "XenDPriv.exe"
        )
        try:
            if os.path.isfile(xenpath):
                os.remove(xenpath)
        except Exception as e:
            self.error("Error: Failure to remove file %s: %s" % (xenpath, str(e)))

    def _report_system_info(self):
        """
        Create the system-info.log artifact file, containing a variety of
        system information that might be useful in diagnosing test failures.
        """
        try:
            import psutil

            path = os.path.join(
                self.query_abs_dirs()["abs_blob_upload_dir"], "system-info.log"
            )
            with open(path, "w") as f:
                f.write("System info collected at %s\n\n" % datetime.now())
                f.write("\nBoot time %s\n" % datetime.fromtimestamp(psutil.boot_time()))
                f.write("\nVirtual memory: %s\n" % str(psutil.virtual_memory()))
                f.write("\nDisk partitions: %s\n" % str(psutil.disk_partitions()))
                f.write("\nDisk usage (/): %s\n" % str(psutil.disk_usage(os.path.sep)))
                if not self._is_windows():
                    # bug 1417189: frequent errors querying users on Windows
                    f.write("\nUsers: %s\n" % str(psutil.users()))
                f.write("\nNetwork connections:\n")
                try:
                    for nc in psutil.net_connections():
                        f.write("  %s\n" % str(nc))
                except Exception:
                    f.write("Exception getting network info: %s\n" % sys.exc_info()[0])
                f.write("\nProcesses:\n")
                try:
                    for p in psutil.process_iter():
                        ctime = str(datetime.fromtimestamp(p.create_time()))
                        f.write(
                            "  PID %d %s %s created at %s\n"
                            % (p.pid, p.name(), str(p.cmdline()), ctime)
                        )
                except Exception:
                    f.write("Exception getting process info: %s\n" % sys.exc_info()[0])
        except Exception:
            # psutil throws a variety of intermittent exceptions
            self.info("Unable to complete system-info.log: %s" % sys.exc_info()[0])

    # pull defined in VCSScript.
    # preflight_run_tests defined in TestingMixin.

    def run_tests(self):
        self._remove_xen_clipboard()
        self._report_system_info()
        self.start_time = datetime.now()
        for category in SUITE_CATEGORIES:
            if not self._run_category_suites(category):
                break

    def get_timeout_for_category(self, suite_category):
        if suite_category == "cppunittest":
            return 2500
        return self.config["suite_definitions"][suite_category].get("run_timeout", 1000)

    def _run_category_suites(self, suite_category):
        """run suite(s) to a specific category"""
        dirs = self.query_abs_dirs()
        suites = self._query_specified_suites(suite_category)
        abs_app_dir = self.query_abs_app_dir()
        abs_res_dir = self.query_abs_res_dir()

        max_per_test_time = timedelta(minutes=60)
        max_per_test_tests = 10
        if self.per_test_coverage:
            max_per_test_tests = 30
        executed_tests = 0
        executed_too_many_tests = False
        xpcshell_selftests = 0

        if suites:
            self.info("#### Running %s suites" % suite_category)
            for suite in suites:
                if executed_too_many_tests and not self.per_test_coverage:
                    return False

                replace_dict = {
                    "abs_app_dir": abs_app_dir,
                    # Mac specific, but points to abs_app_dir on other
                    # platforms.
                    "abs_res_dir": abs_res_dir,
                }
                options_list = []
                env = {"TEST_SUITE": suite}
                if isinstance(suites[suite], dict):
                    options_list = suites[suite].get("options", [])
                    if (
                        self.verify_enabled
                        or self.per_test_coverage
                        or self._get_mozharness_test_paths(suite_category, suite)
                    ):
                        # Ignore tests list in modes where we are running specific tests.
                        tests_list = []
                    else:
                        tests_list = suites[suite].get("tests", [])
                    env = copy.deepcopy(suites[suite].get("env", {}))
                else:
                    options_list = suites[suite]
                    tests_list = []

                flavor = self._query_try_flavor(suite_category, suite)
                try_options, try_tests = self.try_args(flavor)

                suite_name = suite_category + "-" + suite
                tbpl_status, log_level = None, None
                error_list = BaseErrorList + HarnessErrorList
                parser = self.get_test_output_parser(
                    suite_category,
                    flavor=flavor,
                    config=self.config,
                    error_list=error_list,
                    log_obj=self.log_obj,
                )

                if suite_category == "reftest":
                    ref_formatter = imp.load_source(
                        "ReftestFormatter",
                        os.path.abspath(
                            os.path.join(dirs["abs_reftest_dir"], "output.py")
                        ),
                    )
                    parser.formatter = ref_formatter.ReftestFormatter()

                if self.query_minidump_stackwalk():
                    env["MINIDUMP_STACKWALK"] = self.minidump_stackwalk_path
                if self.config["nodejs_path"]:
                    env["MOZ_NODE_PATH"] = self.config["nodejs_path"]
                env["MOZ_UPLOAD_DIR"] = self.query_abs_dirs()["abs_blob_upload_dir"]
                env["MINIDUMP_SAVE_PATH"] = self.query_abs_dirs()["abs_blob_upload_dir"]
                env["RUST_BACKTRACE"] = "full"
                if not os.path.isdir(env["MOZ_UPLOAD_DIR"]):
                    self.mkdir_p(env["MOZ_UPLOAD_DIR"])

                if self.config["allow_software_gl_layers"]:
                    env["MOZ_LAYERS_ALLOW_SOFTWARE_GL"] = "1"

                env["STYLO_THREADS"] = "4"

                env = self.query_env(partial_env=env, log_level=INFO)
                cmd_timeout = self.get_timeout_for_category(suite_category)

                summary = {}
                for per_test_args in self.query_args(suite):
                    # Make sure baseline code coverage tests are never
                    # skipped and that having them run has no influence
                    # on the max number of actual tests that are to be run.
                    is_baseline_test = (
                        "baselinecoverage" in per_test_args[-1]
                        if self.per_test_coverage
                        else False
                    )
                    if executed_too_many_tests and not is_baseline_test:
                        continue

                    if not is_baseline_test:
                        if (datetime.now() - self.start_time) > max_per_test_time:
                            # Running tests has run out of time. That is okay! Stop running
                            # them so that a task timeout is not triggered, and so that
                            # (partial) results are made available in a timely manner.
                            self.info(
                                "TinderboxPrint: Running tests took too long: Not all tests "
                                "were executed.<br/>"
                            )
                            # Signal per-test time exceeded, to break out of suites and
                            # suite categories loops also.
                            return False
                        if executed_tests >= max_per_test_tests:
                            # When changesets are merged between trees or many tests are
                            # otherwise updated at once, there probably is not enough time
                            # to run all tests, and attempting to do so may cause other
                            # problems, such as generating too much log output.
                            self.info(
                                "TinderboxPrint: Too many modified tests: Not all tests "
                                "were executed.<br/>"
                            )
                            executed_too_many_tests = True

                        executed_tests = executed_tests + 1

                    abs_base_cmd = self._query_abs_base_cmd(suite_category, suite)
                    cmd = abs_base_cmd[:]
                    cmd.extend(
                        self.query_options(
                            options_list, try_options, str_format_values=replace_dict
                        )
                    )
                    cmd.extend(
                        self.query_tests_args(
                            tests_list, try_tests, str_format_values=replace_dict
                        )
                    )

                    final_cmd = copy.copy(cmd)
                    final_cmd.extend(per_test_args)

                    # Bug 1714406: In test-verify of xpcshell tests on Windows, repeated
                    # self-tests can trigger https://bugs.python.org/issue37380,
                    # for python < 3.7; avoid by running xpcshell self-tests only once
                    # per test-verify run.
                    if (
                        (self.verify_enabled or self.per_test_coverage)
                        and sys.platform.startswith("win")
                        and sys.version_info < (3, 7)
                        and "--self-test" in final_cmd
                    ):
                        xpcshell_selftests += 1
                        if xpcshell_selftests > 1:
                            final_cmd.remove("--self-test")

                    final_env = copy.copy(env)

                    if self.per_test_coverage:
                        self.set_coverage_env(final_env)

                    return_code = self.run_command(
                        final_cmd,
                        cwd=dirs["abs_work_dir"],
                        output_timeout=cmd_timeout,
                        output_parser=parser,
                        env=final_env,
                    )

                    if self.per_test_coverage:
                        self.add_per_test_coverage_report(
                            final_env, suite, per_test_args[-1]
                        )

                    # mochitest, reftest, and xpcshell suites do not return
                    # appropriate return codes. Therefore, we must parse the output
                    # to determine what the tbpl_status and worst_log_level must
                    # be. We do this by:
                    # 1) checking to see if our mozharness script ran into any
                    #    errors itself with 'num_errors' <- OutputParser
                    # 2) if num_errors is 0 then we look in the subclassed 'parser'
                    #    findings for harness/suite errors <- DesktopUnittestOutputParser
                    # 3) checking to see if the return code is in success_codes

                    success_codes = None
                    tbpl_status, log_level, summary = parser.evaluate_parser(
                        return_code, success_codes, summary
                    )
                    parser.append_tinderboxprint_line(suite_name)

                    self.record_status(tbpl_status, level=log_level)
                    if len(per_test_args) > 0:
                        self.log_per_test_status(
                            per_test_args[-1], tbpl_status, log_level
                        )
                        if tbpl_status == TBPL_RETRY:
                            self.info("Per-test run abandoned due to RETRY status")
                            return False
                    else:
                        self.log(
                            "The %s suite: %s ran with return status: %s"
                            % (suite_category, suite, tbpl_status),
                            level=log_level,
                        )

            if executed_too_many_tests:
                return False
        else:
            self.debug("There were no suites to run for %s" % suite_category)
        return True


# main {{{1
if __name__ == "__main__":
    desktop_unittest = DesktopUnittest()
    desktop_unittest.run_and_exit()
