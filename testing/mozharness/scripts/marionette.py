#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import copy
import json
import os
import sys

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.errors import BaseErrorList, TarErrorList
from mozharness.base.log import INFO
from mozharness.base.script import PreScriptAction
from mozharness.base.transfer import TransferMixin
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.structuredlog import StructuredOutputParser
from mozharness.mozilla.testing.codecoverage import (
    CodeCoverageMixin,
    code_coverage_config_options,
)
from mozharness.mozilla.testing.errors import HarnessErrorList, LogcatErrorList
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.mozilla.testing.unittest import TestSummaryOutputParserHelper


class MarionetteTest(TestingMixin, MercurialScript, TransferMixin, CodeCoverageMixin):
    config_options = (
        [
            [
                ["--application"],
                {
                    "action": "store",
                    "dest": "application",
                    "default": None,
                    "help": "application name of binary",
                },
            ],
            [
                ["--app-arg"],
                {
                    "action": "store",
                    "dest": "app_arg",
                    "default": None,
                    "help": "Optional command-line argument to pass to the browser",
                },
            ],
            [
                ["--marionette-address"],
                {
                    "action": "store",
                    "dest": "marionette_address",
                    "default": None,
                    "help": "The host:port of the Marionette server running inside Gecko. "
                    "Unused for emulator testing",
                },
            ],
            [
                ["--emulator"],
                {
                    "action": "store",
                    "type": "choice",
                    "choices": ["arm", "x86"],
                    "dest": "emulator",
                    "default": None,
                    "help": "Use an emulator for testing",
                },
            ],
            [
                ["--test-manifest"],
                {
                    "action": "store",
                    "dest": "test_manifest",
                    "default": "unit-tests.ini",
                    "help": "Path to test manifest to run relative to the Marionette "
                    "tests directory",
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
                ["--setpref"],
                {
                    "action": "append",
                    "metavar": "PREF=VALUE",
                    "dest": "extra_prefs",
                    "default": [],
                    "help": "Extra user prefs.",
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
                ["--allow-software-gl-layers"],
                {
                    "action": "store_true",
                    "dest": "allow_software_gl_layers",
                    "default": False,
                    "help": "Permits a software GL implementation (such as LLVMPipe) to use the GL compositor.",  # NOQA: E501
                },
            ],
            [
                ["--disable-fission"],
                {
                    "action": "store_true",
                    "dest": "disable_fission",
                    "default": False,
                    "help": "Run the browser without fission enabled",
                },
            ],
        ]
        + copy.deepcopy(testing_config_options)
        + copy.deepcopy(code_coverage_config_options)
    )

    repos = []

    def __init__(self, require_config_file=False):
        super(MarionetteTest, self).__init__(
            config_options=self.config_options,
            all_actions=[
                "clobber",
                "pull",
                "download-and-extract",
                "create-virtualenv",
                "install",
                "run-tests",
            ],
            default_actions=[
                "clobber",
                "pull",
                "download-and-extract",
                "create-virtualenv",
                "install",
                "run-tests",
            ],
            require_config_file=require_config_file,
            config={"require_test_zip": True},
        )

        # these are necessary since self.config is read only
        c = self.config
        self.installer_url = c.get("installer_url")
        self.installer_path = c.get("installer_path")
        self.binary_path = c.get("binary_path")
        self.test_url = c.get("test_url")
        self.test_packages_url = c.get("test_packages_url")

        self.test_suite = self._get_test_suite(c.get("emulator"))
        if self.test_suite not in self.config["suite_definitions"]:
            self.fatal("{} is not defined in the config!".format(self.test_suite))

        if c.get("structured_output"):
            self.parser_class = StructuredOutputParser
        else:
            self.parser_class = TestSummaryOutputParserHelper

    def _pre_config_lock(self, rw_config):
        super(MarionetteTest, self)._pre_config_lock(rw_config)
        if not self.config.get("emulator") and not self.config.get(
            "marionette_address"
        ):
            self.fatal(
                "You need to specify a --marionette-address for non-emulator tests! "
                "(Try --marionette-address localhost:2828 )"
            )

    def _query_tests_dir(self):
        dirs = self.query_abs_dirs()
        test_dir = self.config["suite_definitions"][self.test_suite]["testsdir"]

        return os.path.join(dirs["abs_test_install_dir"], test_dir)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(MarionetteTest, self).query_abs_dirs()
        dirs = {}
        dirs["abs_test_install_dir"] = os.path.join(abs_dirs["abs_work_dir"], "tests")
        dirs["abs_marionette_dir"] = os.path.join(
            dirs["abs_test_install_dir"], "marionette", "harness", "marionette_harness"
        )
        dirs["abs_marionette_tests_dir"] = os.path.join(
            dirs["abs_test_install_dir"],
            "marionette",
            "tests",
            "testing",
            "marionette",
            "harness",
            "marionette_harness",
            "tests",
        )
        dirs["abs_gecko_dir"] = os.path.join(abs_dirs["abs_work_dir"], "gecko")
        dirs["abs_emulator_dir"] = os.path.join(abs_dirs["abs_work_dir"], "emulator")

        dirs["abs_blob_upload_dir"] = os.path.join(
            abs_dirs["abs_work_dir"], "blobber_upload_dir"
        )

        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    @PreScriptAction("create-virtualenv")
    def _configure_marionette_virtualenv(self, action):
        dirs = self.query_abs_dirs()
        requirements = os.path.join(
            dirs["abs_test_install_dir"], "config", "marionette_requirements.txt"
        )
        if not os.path.isfile(requirements):
            self.fatal(
                "Could not find marionette requirements file: {}".format(requirements)
            )

        self.register_virtualenv_module(requirements=[requirements], two_pass=True)

    def _get_test_suite(self, is_emulator):
        """
        Determine which in tree options group to use and return the
        appropriate key.
        """
        platform = "emulator" if is_emulator else "desktop"
        # Currently running marionette on an emulator means webapi
        # tests. This method will need to change if this does.
        testsuite = "webapi" if is_emulator else "marionette"
        return "{}_{}".format(testsuite, platform)

    def download_and_extract(self):
        super(MarionetteTest, self).download_and_extract()

        if self.config.get("emulator"):
            dirs = self.query_abs_dirs()

            self.mkdir_p(dirs["abs_emulator_dir"])
            tar = self.query_exe("tar", return_type="list")
            self.run_command(
                tar + ["zxf", self.installer_path],
                cwd=dirs["abs_emulator_dir"],
                error_list=TarErrorList,
                halt_on_failure=True,
                fatal_exit_code=3,
            )

    def install(self):
        if self.config.get("emulator"):
            self.info("Emulator tests; skipping.")
        else:
            super(MarionetteTest, self).install()

    def run_tests(self):
        """
        Run the Marionette tests
        """
        dirs = self.query_abs_dirs()

        raw_log_file = os.path.join(dirs["abs_blob_upload_dir"], "marionette_raw.log")
        error_summary_file = os.path.join(
            dirs["abs_blob_upload_dir"], "marionette_errorsummary.log"
        )
        html_report_file = os.path.join(dirs["abs_blob_upload_dir"], "report.html")

        config_fmt_args = {
            # emulator builds require a longer timeout
            "timeout": 60000 if self.config.get("emulator") else 10000,
            "profile": os.path.join(dirs["abs_work_dir"], "profile"),
            "xml_output": os.path.join(dirs["abs_work_dir"], "output.xml"),
            "html_output": os.path.join(dirs["abs_blob_upload_dir"], "output.html"),
            "logcat_dir": dirs["abs_work_dir"],
            "emulator": "arm",
            "symbols_path": self.symbols_path,
            "binary": self.binary_path,
            "address": self.config.get("marionette_address"),
            "raw_log_file": raw_log_file,
            "error_summary_file": error_summary_file,
            "html_report_file": html_report_file,
            "gecko_log": dirs["abs_blob_upload_dir"],
            "this_chunk": self.config.get("this_chunk", 1),
            "total_chunks": self.config.get("total_chunks", 1),
        }

        self.info("The emulator type: %s" % config_fmt_args["emulator"])
        # build the marionette command arguments
        python = self.query_python_path("python")

        cmd = [python, "-u", os.path.join(dirs["abs_marionette_dir"], "runtests.py")]

        manifest = os.path.join(
            dirs["abs_marionette_tests_dir"], self.config["test_manifest"]
        )

        if self.config.get("app_arg"):
            config_fmt_args["app_arg"] = self.config["app_arg"]

        cmd.extend(["--setpref={}".format(p) for p in self.config["extra_prefs"]])

        cmd.append("--gecko-log=-")

        if self.config.get("structured_output"):
            cmd.append("--log-raw=-")

        if self.config["disable_fission"]:
            cmd.append("--disable-fission")
            cmd.extend(["--setpref=fission.autostart=false"])

        for arg in self.config["suite_definitions"][self.test_suite]["options"]:
            cmd.append(arg % config_fmt_args)

        if self.mkdir_p(dirs["abs_blob_upload_dir"]) == -1:
            # Make sure that the logging directory exists
            self.fatal("Could not create blobber upload directory")

        test_paths = json.loads(os.environ.get("MOZHARNESS_TEST_PATHS", '""'))
        confirm_paths = json.loads(os.environ.get("MOZHARNESS_CONFIRM_PATHS", '""'))

        suite = "marionette"
        if test_paths and suite in test_paths:
            suite_test_paths = test_paths[suite]
            if confirm_paths and suite in confirm_paths and confirm_paths[suite]:
                suite_test_paths = confirm_paths[suite]

            paths = [
                os.path.join(dirs["abs_test_install_dir"], "marionette", "tests", p)
                for p in suite_test_paths
            ]
            cmd.extend(paths)
        else:
            cmd.append(manifest)

        try_options, try_tests = self.try_args("marionette")
        cmd.extend(self.query_tests_args(try_tests, str_format_values=config_fmt_args))

        env = {}
        if self.query_minidump_stackwalk():
            env["MINIDUMP_STACKWALK"] = self.minidump_stackwalk_path
        env["MOZ_UPLOAD_DIR"] = self.query_abs_dirs()["abs_blob_upload_dir"]
        env["MINIDUMP_SAVE_PATH"] = self.query_abs_dirs()["abs_blob_upload_dir"]
        env["RUST_BACKTRACE"] = "full"

        if self.config["allow_software_gl_layers"]:
            env["MOZ_LAYERS_ALLOW_SOFTWARE_GL"] = "1"

        if self.config["headless"]:
            env["MOZ_HEADLESS"] = "1"
            env["MOZ_HEADLESS_WIDTH"] = self.config["headless_width"]
            env["MOZ_HEADLESS_HEIGHT"] = self.config["headless_height"]

        if not os.path.isdir(env["MOZ_UPLOAD_DIR"]):
            self.mkdir_p(env["MOZ_UPLOAD_DIR"])

        # Causes Firefox to crash when using non-local connections.
        env["MOZ_DISABLE_NONLOCAL_CONNECTIONS"] = "1"

        env = self.query_env(partial_env=env)

        try:
            cwd = self._query_tests_dir()
        except Exception as e:
            self.fatal(
                "Don't know how to run --test-suite '{0}': {1}!".format(
                    self.test_suite, e
                )
            )

        marionette_parser = self.parser_class(
            config=self.config,
            log_obj=self.log_obj,
            error_list=BaseErrorList + HarnessErrorList,
            strict=False,
        )
        return_code = self.run_command(
            cmd, cwd=cwd, output_timeout=1000, output_parser=marionette_parser, env=env
        )
        level = INFO
        tbpl_status, log_level, summary = marionette_parser.evaluate_parser(
            return_code=return_code
        )
        marionette_parser.append_tinderboxprint_line("marionette")

        qemu = os.path.join(dirs["abs_work_dir"], "qemu.log")
        if os.path.isfile(qemu):
            self.copyfile(qemu, os.path.join(dirs["abs_blob_upload_dir"], "qemu.log"))

        # dump logcat output if there were failures
        if self.config.get("emulator"):
            if (
                marionette_parser.failed != "0"
                or "T-FAIL" in marionette_parser.tsummary
            ):
                logcat = os.path.join(dirs["abs_work_dir"], "emulator-5554.log")
                if os.access(logcat, os.F_OK):
                    self.info("dumping logcat")
                    self.run_command(["cat", logcat], error_list=LogcatErrorList)
                else:
                    self.info("no logcat file found")
        else:
            # .. or gecko.log if it exists
            gecko_log = os.path.join(self.config["base_work_dir"], "gecko.log")
            if os.access(gecko_log, os.F_OK):
                self.info("dumping gecko.log")
                self.run_command(["cat", gecko_log])
                self.rmtree(gecko_log)
            else:
                self.info("gecko.log not found")

        marionette_parser.print_summary("marionette")

        self.log(
            "Marionette exited with return code %s: %s" % (return_code, tbpl_status),
            level=level,
        )
        self.record_status(tbpl_status)


if __name__ == "__main__":
    marionetteTest = MarionetteTest()
    marionetteTest.run_and_exit()
