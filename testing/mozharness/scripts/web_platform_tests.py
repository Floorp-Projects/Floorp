#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
from __future__ import absolute_import
import copy
import gzip
import json
import os
import sys

from datetime import datetime, timedelta

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

import mozinfo

from mozharness.base.errors import BaseErrorList
from mozharness.base.script import PreScriptAction
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.automation import TBPL_RETRY
from mozharness.mozilla.testing.android import AndroidMixin
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.mozilla.testing.codecoverage import (
    CodeCoverageMixin,
    code_coverage_config_options,
)
from mozharness.mozilla.testing.errors import WptHarnessErrorList

from mozharness.mozilla.structuredlog import StructuredOutputParser
from mozharness.base.log import INFO


class WebPlatformTest(TestingMixin, MercurialScript, CodeCoverageMixin, AndroidMixin):
    config_options = (
        [
            [
                ["--test-type"],
                {
                    "action": "extend",
                    "dest": "test_type",
                    "help": "Specify the test types to run.",
                },
            ],
            [
                ["--disable-e10s"],
                {
                    "action": "store_false",
                    "dest": "e10s",
                    "default": True,
                    "help": "Run without e10s enabled",
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
                    "help": "Permits a software GL implementation (such as LLVMPipe) "
                    "to use the GL compositor.",
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
                ["--headless"],
                {
                    "action": "store_true",
                    "dest": "headless",
                    "default": False,
                    "help": "Run tests in headless mode.",
                },
            ],
            [
                ["--headless-width"],
                {
                    "action": "store",
                    "dest": "headless_width",
                    "default": "1600",
                    "help": "Specify headless virtual screen width (default: 1600).",
                },
            ],
            [
                ["--headless-height"],
                {
                    "action": "store",
                    "dest": "headless_height",
                    "default": "1200",
                    "help": "Specify headless virtual screen height (default: 1200).",
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
                ["--skip-implementation-status"],
                {
                    "action": "extend",
                    "dest": "skip_implementation_status",
                    "default": [],
                    "help": "Defines a way to not run a specific implementation status "
                    " (i.e. not implemented).",
                },
            ],
            [
                ["--backlog"],
                {
                    "action": "store_true",
                    "dest": "backlog",
                    "default": False,
                    "help": "Defines if test category is backlog.",
                },
            ],
            [
                ["--skip-timeout"],
                {
                    "action": "store_true",
                    "dest": "skip_timeout",
                    "default": False,
                    "help": "Ignore tests that are expected status of TIMEOUT",
                },
            ],
            [
                ["--include"],
                {
                    "action": "store",
                    "dest": "include",
                    "default": None,
                    "help": "URL prefix to include.",
                },
            ],
        ]
        + copy.deepcopy(testing_config_options)
        + copy.deepcopy(code_coverage_config_options)
    )

    def __init__(self, require_config_file=True):
        super(WebPlatformTest, self).__init__(
            config_options=self.config_options,
            all_actions=[
                "clobber",
                "download-and-extract",
                "download-and-process-manifest",
                "create-virtualenv",
                "pull",
                "start-emulator",
                "verify-device",
                "install",
                "run-tests",
            ],
            require_config_file=require_config_file,
            config={"require_test_zip": True},
        )

        # Surely this should be in the superclass
        c = self.config
        self.installer_url = c.get("installer_url")
        self.test_url = c.get("test_url")
        self.test_packages_url = c.get("test_packages_url")
        self.installer_path = c.get("installer_path")
        self.binary_path = c.get("binary_path")
        self.abs_app_dir = None
        self.xre_path = None
        if self.is_emulator:
            self.device_serial = "emulator-5554"

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

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(WebPlatformTest, self).query_abs_dirs()

        dirs = {}
        dirs["abs_app_install_dir"] = os.path.join(
            abs_dirs["abs_work_dir"], "application"
        )
        dirs["abs_test_install_dir"] = os.path.join(abs_dirs["abs_work_dir"], "tests")
        dirs["abs_test_bin_dir"] = os.path.join(dirs["abs_test_install_dir"], "bin")
        dirs["abs_wpttest_dir"] = os.path.join(
            dirs["abs_test_install_dir"], "web-platform"
        )
        dirs["abs_blob_upload_dir"] = os.path.join(
            abs_dirs["abs_work_dir"], "blobber_upload_dir"
        )
        dirs["abs_test_extensions_dir"] = os.path.join(
            dirs["abs_test_install_dir"], "extensions"
        )
        if self.is_android:
            dirs["abs_xre_dir"] = os.path.join(abs_dirs["abs_work_dir"], "hostutils")
        if self.is_emulator:
            work_dir = os.environ.get("MOZ_FETCHES_DIR") or abs_dirs["abs_work_dir"]
            dirs["abs_sdk_dir"] = os.path.join(work_dir, "android-sdk-linux")
            dirs["abs_avds_dir"] = os.path.join(work_dir, "android-device")
            dirs["abs_bundletool_path"] = os.path.join(work_dir, "bundletool.jar")
            if self.config["enable_webrender"]:
                # AndroidMixin uses this when launching the emulator. We only want
                # GLES3 if we're running WebRender
                self.use_gles3 = True

        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs

        return self.abs_dirs

    @PreScriptAction("create-virtualenv")
    def _pre_create_virtualenv(self, action):
        dirs = self.query_abs_dirs()

        requirements = os.path.join(
            dirs["abs_test_install_dir"], "config", "marionette_requirements.txt"
        )

        self.register_virtualenv_module(requirements=[requirements], two_pass=True)

    def _query_geckodriver(self):
        path = None
        c = self.config
        dirs = self.query_abs_dirs()
        repl_dict = {}
        repl_dict.update(dirs)
        path = c.get("geckodriver", "geckodriver")
        if path:
            path = path % repl_dict
        return path

    def _query_cmd(self, test_types):
        if not self.binary_path:
            self.fatal("Binary path could not be determined")
            # And exit

        c = self.config
        run_file_name = "runtests.py"

        dirs = self.query_abs_dirs()
        abs_app_dir = self.query_abs_app_dir()
        str_format_values = {
            "binary_path": self.binary_path,
            "test_path": dirs["abs_wpttest_dir"],
            "test_install_path": dirs["abs_test_install_dir"],
            "abs_app_dir": abs_app_dir,
            "abs_work_dir": dirs["abs_work_dir"],
            "xre_path": self.xre_path,
        }

        cmd = [self.query_python_path("python"), "-u"]
        cmd.append(os.path.join(dirs["abs_wpttest_dir"], run_file_name))

        mozinfo.find_and_update_from_json(dirs["abs_test_install_dir"])

        # Default to fission disabled
        fission_enabled = "fission.autostart=true" in c["extra_prefs"]

        raw_log_file, error_summary_file = self.get_indexed_logs(
            dirs["abs_blob_upload_dir"], "wpt"
        )

        cmd += [
            "--log-raw=-",
            "--log-raw=%s" % raw_log_file,
            "--log-wptreport=%s"
            % os.path.join(dirs["abs_blob_upload_dir"], "wptreport.json"),
            "--log-errorsummary=%s" % error_summary_file,
            "--binary=%s" % self.binary_path,
            "--symbols-path=%s" % self.symbols_path,
            "--stackwalk-binary=%s" % self.query_minidump_stackwalk(),
            "--stackfix-dir=%s" % os.path.join(dirs["abs_test_install_dir"], "bin"),
            "--no-pause-after-test",
            "--instrument-to-file=%s"
            % os.path.join(dirs["abs_blob_upload_dir"], "wpt_instruments.txt"),
            "--specialpowers-path=%s"
            % os.path.join(
                dirs["abs_test_extensions_dir"], "specialpowers@mozilla.org.xpi"
            ),
        ]

        is_windows_7 = (
            mozinfo.info["os"] == "win" and mozinfo.info["os_version"] == "6.1"
        )

        if (
            self.is_android
            or mozinfo.info["tsan"]
            or "wdspec" in test_types
            or fission_enabled
            # Bug 1392106 - skia error 0x80070005: Access is denied.
            or is_windows_7
            and mozinfo.info["debug"]
        ):
            processes = 1
        else:
            processes = 2
        cmd.append("--processes=%s" % processes)

        if self.is_android:
            cmd += [
                "--device-serial=%s" % self.device_serial,
                "--package-name=%s" % self.query_package_name(),
            ]

        if is_windows_7:
            # On Windows 7 --install-fonts fails, so fall back to a Firefox-specific codepath
            self._install_fonts()
        else:
            cmd += ["--install-fonts"]

        for test_type in test_types:
            cmd.append("--test-type=%s" % test_type)

        if c["extra_prefs"]:
            cmd.extend(["--setpref={}".format(p) for p in c["extra_prefs"]])

        if not fission_enabled and "fission.autostart=false" not in c["extra_prefs"]:
            cmd.append("--setpref=fission.autostart=false")

        if not c["e10s"]:
            cmd.append("--disable-e10s")

        if c["enable_webrender"]:
            cmd.append("--enable-webrender")

        if c["skip_timeout"]:
            cmd.append("--skip-timeout")

        for implementation_status in c["skip_implementation_status"]:
            cmd.append("--skip-implementation-status=%s" % implementation_status)

        # Bug 1643177 - reduce timeout multiplier for web-platform-tests backlog
        if c["backlog"]:
            cmd.append("--timeout-multiplier=0.25")

        test_paths = set()
        if not (self.verify_enabled or self.per_test_coverage):
            mozharness_test_paths = json.loads(
                os.environ.get("MOZHARNESS_TEST_PATHS", '""')
            )
            if mozharness_test_paths:
                path = os.path.join(dirs["abs_fetches_dir"], "wpt_tests_by_group.json")

                if not os.path.exists(path):
                    self.critical("Unable to locate web-platform-test groups file.")

                cmd.append("--test-groups={}".format(path))

                for key in mozharness_test_paths.keys():
                    paths = mozharness_test_paths.get(key, [])
                    for path in paths:
                        if not path.startswith("/"):
                            # Assume this is a filesystem path rather than a test id
                            path = os.path.relpath(path, "testing/web-platform")
                            if ".." in path:
                                self.fatal("Invalid WPT path: {}".format(path))
                            path = os.path.join(dirs["abs_wpttest_dir"], path)
                        test_paths.add(path)
            else:
                # As per WPT harness, the --run-by-dir flag is incompatible with
                # the --test-groups flag.
                cmd.append("--run-by-dir=%i" % (3 if not mozinfo.info["asan"] else 0))
                for opt in ["total_chunks", "this_chunk"]:
                    val = c.get(opt)
                    if val:
                        cmd.append("--%s=%s" % (opt.replace("_", "-"), val))

        options = list(c.get("options", []))

        if "wdspec" in test_types:
            geckodriver_path = self._query_geckodriver()
            if not geckodriver_path or not os.path.isfile(geckodriver_path):
                self.fatal(
                    "Unable to find geckodriver binary "
                    "in common test package: %s" % str(geckodriver_path)
                )
            cmd.append("--webdriver-binary=%s" % geckodriver_path)
            cmd.append("--webdriver-arg=-vv")  # enable trace logs

        test_type_suite = {
            "testharness": "web-platform-tests",
            "crashtest": "web-platform-tests-crashtest",
            "print-reftest": "web-platform-tests-print-reftest",
            "reftest": "web-platform-tests-reftest",
            "wdspec": "web-platform-tests-wdspec",
        }
        for test_type in test_types:
            try_options, try_tests = self.try_args(test_type_suite[test_type])

            cmd.extend(
                self.query_options(
                    options, try_options, str_format_values=str_format_values
                )
            )
            cmd.extend(
                self.query_tests_args(try_tests, str_format_values=str_format_values)
            )
        if "include" in c and c["include"]:
            cmd.append("--include=%s" % c["include"])

        cmd.extend(test_paths)

        return cmd

    def download_and_extract(self):
        super(WebPlatformTest, self).download_and_extract(
            extract_dirs=[
                "mach",
                "bin/*",
                "config/*",
                "extensions/*",
                "mozbase/*",
                "marionette/*",
                "tools/*",
                "web-platform/*",
                "mozpack/*",
                "mozbuild/*",
            ],
            suite_categories=["web-platform"],
        )
        dirs = self.query_abs_dirs()
        if self.is_android:
            self.xre_path = self.download_hostutils(dirs["abs_xre_dir"])
        # Make sure that the logging directory exists
        if self.mkdir_p(dirs["abs_blob_upload_dir"]) == -1:
            self.fatal("Could not create blobber upload directory")
            # Exit

    def download_and_process_manifest(self):
        """Downloads the tests-by-manifest JSON mapping generated by the decision task.

        web-platform-tests are chunked in the decision task as of Bug 1608837
        and this means tests are resolved by the TestResolver as part of this process.

        The manifest file contains tests keyed by the groups generated in
        TestResolver.get_wpt_group().

        Upon successful call, a JSON file containing only the web-platform test
        groups are saved in the fetch directory.

        Bug:
            1634554
        """
        dirs = self.query_abs_dirs()
        url = os.environ.get("TESTS_BY_MANIFEST_URL", "")
        if not url:
            self.fatal("TESTS_BY_MANIFEST_URL not defined.")

        artifact_name = url.split("/")[-1]

        # Save file to the MOZ_FETCHES dir.
        self.download_file(
            url, file_name=artifact_name, parent_dir=dirs["abs_fetches_dir"]
        )

        with gzip.open(os.path.join(dirs["abs_fetches_dir"], artifact_name), "r") as f:
            tests_by_manifest = json.loads(f.read())

        # We need to filter out non-web-platform-tests without knowing what the
        # groups are. Fortunately, all web-platform test 'manifests' begin with a
        # forward slash.
        test_groups = {
            key: tests_by_manifest[key]
            for key in tests_by_manifest.keys()
            if key.startswith("/")
        }

        outfile = os.path.join(dirs["abs_fetches_dir"], "wpt_tests_by_group.json")
        with open(outfile, "w+") as f:
            json.dump(test_groups, f, indent=2, sort_keys=True)

    def install(self):
        if self.is_android:
            self.install_android_app(self.installer_path)
        else:
            super(WebPlatformTest, self).install()

    def _install_fonts(self):
        if self.is_android:
            return
        # Ensure the Ahem font is available
        dirs = self.query_abs_dirs()

        if not sys.platform.startswith("darwin"):
            font_path = os.path.join(os.path.dirname(self.binary_path), "fonts")
        else:
            font_path = os.path.join(
                os.path.dirname(self.binary_path),
                os.pardir,
                "Resources",
                "res",
                "fonts",
            )
        if not os.path.exists(font_path):
            os.makedirs(font_path)
        ahem_src = os.path.join(dirs["abs_wpttest_dir"], "tests", "fonts", "Ahem.ttf")
        ahem_dest = os.path.join(font_path, "Ahem.ttf")
        with open(ahem_src, "rb") as src, open(ahem_dest, "wb") as dest:
            dest.write(src.read())

    def run_tests(self):
        dirs = self.query_abs_dirs()

        parser = StructuredOutputParser(
            config=self.config,
            log_obj=self.log_obj,
            log_compact=True,
            error_list=BaseErrorList + WptHarnessErrorList,
            allow_crashes=True,
        )

        env = {"MINIDUMP_SAVE_PATH": dirs["abs_blob_upload_dir"]}
        env["RUST_BACKTRACE"] = "full"

        if self.config["allow_software_gl_layers"]:
            env["MOZ_LAYERS_ALLOW_SOFTWARE_GL"] = "1"
        if self.config["headless"]:
            env["MOZ_HEADLESS"] = "1"
            env["MOZ_HEADLESS_WIDTH"] = self.config["headless_width"]
            env["MOZ_HEADLESS_HEIGHT"] = self.config["headless_height"]

        env["STYLO_THREADS"] = "4"

        if self.is_android:
            env["ADB_PATH"] = self.adb_path

        env = self.query_env(partial_env=env, log_level=INFO)

        start_time = datetime.now()
        max_per_test_time = timedelta(minutes=60)
        max_per_test_tests = 10
        if self.per_test_coverage:
            max_per_test_tests = 30
        executed_tests = 0
        executed_too_many_tests = False

        if self.per_test_coverage or self.verify_enabled:
            suites = self.query_per_test_category_suites(None, None)
            if "wdspec" in suites:
                # geckodriver is required for wdspec, but not always available
                geckodriver_path = self._query_geckodriver()
                if not geckodriver_path or not os.path.isfile(geckodriver_path):
                    suites.remove("wdspec")
                    self.info("Skipping 'wdspec' tests - no geckodriver")
        else:
            test_types = self.config.get("test_type", [])
            suites = [None]
        for suite in suites:
            if executed_too_many_tests and not self.per_test_coverage:
                continue

            if suite:
                test_types = [suite]

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
                    if (datetime.now() - start_time) > max_per_test_time:
                        # Running tests has run out of time. That is okay! Stop running
                        # them so that a task timeout is not triggered, and so that
                        # (partial) results are made available in a timely manner.
                        self.info(
                            "TinderboxPrint: Running tests took too long: Not all tests "
                            "were executed.<br/>"
                        )
                        return
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

                cmd = self._query_cmd(test_types)
                cmd.extend(per_test_args)

                final_env = copy.copy(env)

                if self.per_test_coverage:
                    self.set_coverage_env(final_env, is_baseline_test)

                return_code = self.run_command(
                    cmd,
                    cwd=dirs["abs_work_dir"],
                    output_timeout=1000,
                    output_parser=parser,
                    env=final_env,
                )

                if self.per_test_coverage:
                    self.add_per_test_coverage_report(
                        final_env, suite, per_test_args[-1]
                    )

                tbpl_status, log_level, summary = parser.evaluate_parser(
                    return_code, previous_summary=summary
                )
                self.record_status(tbpl_status, level=log_level)

                if len(per_test_args) > 0:
                    self.log_per_test_status(per_test_args[-1], tbpl_status, log_level)
                    if tbpl_status == TBPL_RETRY:
                        self.info("Per-test run abandoned due to RETRY status")
                        return


# main {{{1
if __name__ == "__main__":
    web_platform_tests = WebPlatformTest()
    web_platform_tests.run_and_exit()
