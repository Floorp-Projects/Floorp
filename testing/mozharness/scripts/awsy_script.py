#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""
run awsy tests in a virtualenv
"""

import copy
import json
import os
import re
import sys

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

import mozharness
import mozinfo
from mozharness.base.log import ERROR, INFO
from mozharness.base.script import PreScriptAction
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.structuredlog import StructuredOutputParser
from mozharness.mozilla.testing.codecoverage import (
    CodeCoverageMixin,
    code_coverage_config_options,
)
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.mozilla.tooltool import TooltoolMixin

PY2 = sys.version_info.major == 2
scripts_path = os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__)))
external_tools_path = os.path.join(scripts_path, "external_tools")


class AWSY(TestingMixin, MercurialScript, TooltoolMixin, CodeCoverageMixin):
    config_options = (
        [
            [
                ["--disable-e10s"],
                {
                    "action": "store_false",
                    "dest": "e10s",
                    "default": True,
                    "help": "Run tests without multiple processes (e10s). (Desktop builds only)",
                },
            ],
            [
                ["--setpref"],
                {
                    "action": "append",
                    "dest": "extra_prefs",
                    "default": [],
                    "help": "Extra user prefs.",
                },
            ],
            [
                ["--base"],
                {
                    "action": "store_true",
                    "dest": "test_about_blank",
                    "default": False,
                    "help": "Runs the about:blank base case memory test.",
                },
            ],
            [
                ["--dmd"],
                {
                    "action": "store_true",
                    "dest": "dmd",
                    "default": False,
                    "help": "Runs tests with DMD enabled.",
                },
            ],
            [
                ["--tp6"],
                {
                    "action": "store_true",
                    "dest": "tp6",
                    "default": False,
                    "help": "Runs tests with the tp6 pageset.",
                },
            ],
        ]
        + testing_config_options
        + copy.deepcopy(code_coverage_config_options)
    )

    error_list = [
        {"regex": re.compile(r"""(TEST-UNEXPECTED|PROCESS-CRASH)"""), "level": ERROR},
    ]

    def __init__(self, **kwargs):
        kwargs.setdefault("config_options", self.config_options)
        kwargs.setdefault(
            "all_actions",
            [
                "clobber",
                "download-and-extract",
                "populate-webroot",
                "create-virtualenv",
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
                "install",
                "run-tests",
            ],
        )
        kwargs.setdefault("config", {})
        super(AWSY, self).__init__(**kwargs)
        self.installer_url = self.config.get("installer_url")
        self.tests = None

        self.testdir = self.query_abs_dirs()["abs_test_install_dir"]
        self.awsy_path = os.path.join(self.testdir, "awsy")
        self.awsy_libdir = os.path.join(self.awsy_path, "awsy")
        self.webroot_dir = os.path.join(self.testdir, "html")
        self.results_dir = os.path.join(self.testdir, "results")
        self.binary_path = self.config.get("binary_path")

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(AWSY, self).query_abs_dirs()

        dirs = {}
        dirs["abs_blob_upload_dir"] = os.path.join(
            abs_dirs["abs_work_dir"], "blobber_upload_dir"
        )
        dirs["abs_test_install_dir"] = os.path.join(abs_dirs["abs_work_dir"], "tests")
        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def download_and_extract(self, extract_dirs=None, suite_categories=None):
        ret = super(AWSY, self).download_and_extract(
            suite_categories=["common", "awsy"]
        )
        return ret

    @PreScriptAction("create-virtualenv")
    def _pre_create_virtualenv(self, action):
        requirements_files = [
            os.path.join(self.testdir, "config", "marionette_requirements.txt")
        ]

        for requirements_file in requirements_files:
            self.register_virtualenv_module(
                requirements=[requirements_file], two_pass=True
            )

        self.register_virtualenv_module("awsy", self.awsy_path)

    def populate_webroot(self):
        """Populate the production test machines' webroots"""
        self.info("Downloading pageset with tooltool...")
        manifest_file = os.path.join(self.awsy_path, "tp5n-pageset.manifest")
        page_load_test_dir = os.path.join(self.webroot_dir, "page_load_test")
        if not os.path.isdir(page_load_test_dir):
            self.mkdir_p(page_load_test_dir)
        self.tooltool_fetch(
            manifest_file,
            output_dir=page_load_test_dir,
            cache=self.config.get("tooltool_cache"),
        )
        archive = os.path.join(page_load_test_dir, "tp5n.zip")
        unzip = self.query_exe("unzip")
        unzip_cmd = [unzip, "-q", "-o", archive, "-d", page_load_test_dir]
        self.run_command(unzip_cmd, halt_on_failure=False)
        self.run_command("ls %s" % page_load_test_dir)

    def run_tests(self, args=None, **kw):
        """
        AWSY test should be implemented here
        """
        dirs = self.abs_dirs
        env = {}
        error_summary_file = os.path.join(
            dirs["abs_blob_upload_dir"], "marionette_errorsummary.log"
        )

        runtime_testvars = {
            "webRootDir": self.webroot_dir,
            "resultsDir": self.results_dir,
            "bin": self.binary_path,
        }

        # Check if this is a DMD build and if so enable it.
        dmd_enabled = False
        dmd_py_lib_dir = os.path.dirname(self.binary_path)
        if mozinfo.os == "mac":
            # On mac binary is in MacOS and dmd.py is in Resources, ie:
            #   Name.app/Contents/MacOS/libdmd.dylib
            #   Name.app/Contents/Resources/dmd.py
            dmd_py_lib_dir = os.path.join(dmd_py_lib_dir, "../Resources/")

        dmd_path = os.path.join(dmd_py_lib_dir, "dmd.py")
        if self.config["dmd"] and os.path.isfile(dmd_path):
            dmd_enabled = True
            runtime_testvars["dmd"] = True

            # Allow the child process to import dmd.py
            python_path = os.environ.get("PYTHONPATH")

            if python_path:
                os.environ["PYTHONPATH"] = "%s%s%s" % (
                    python_path,
                    os.pathsep,
                    dmd_py_lib_dir,
                )
            else:
                os.environ["PYTHONPATH"] = dmd_py_lib_dir

            env["DMD"] = "--mode=dark-matter --stacks=full"

        runtime_testvars["tp6"] = self.config["tp6"]
        if self.config["tp6"]:
            # mitmproxy needs path to mozharness when installing the cert, and tooltool
            env["SCRIPTSPATH"] = scripts_path
            env["EXTERNALTOOLSPATH"] = external_tools_path

        runtime_testvars_path = os.path.join(self.awsy_path, "runtime-testvars.json")
        runtime_testvars_file = open(runtime_testvars_path, "wb" if PY2 else "w")
        runtime_testvars_file.write(json.dumps(runtime_testvars, indent=2))
        runtime_testvars_file.close()

        cmd = ["marionette"]

        test_vars_file = None
        if self.config["test_about_blank"]:
            test_vars_file = "base-testvars.json"
        else:
            if self.config["tp6"]:
                test_vars_file = "tp6-testvars.json"
            else:
                test_vars_file = "testvars.json"

        cmd.append(
            "--testvars=%s" % os.path.join(self.awsy_path, "conf", test_vars_file)
        )
        cmd.append("--testvars=%s" % runtime_testvars_path)
        cmd.append("--log-raw=-")
        cmd.append("--log-errorsummary=%s" % error_summary_file)
        cmd.append("--binary=%s" % self.binary_path)
        cmd.append("--profile=%s" % (os.path.join(dirs["abs_work_dir"], "profile")))
        if not self.config["e10s"]:
            cmd.append("--disable-e10s")
        cmd.extend(["--setpref={}".format(p) for p in self.config["extra_prefs"]])
        cmd.append(
            "--gecko-log=%s" % os.path.join(dirs["abs_blob_upload_dir"], "gecko.log")
        )
        # TestingMixin._download_and_extract_symbols() should set
        # self.symbols_path
        cmd.append("--symbols-path=%s" % self.symbols_path)

        if self.config["test_about_blank"]:
            test_file = os.path.join(self.awsy_libdir, "test_base_memory_usage.py")
            prefs_file = "base-prefs.json"
        else:
            test_file = os.path.join(self.awsy_libdir, "test_memory_usage.py")
            if self.config["tp6"]:
                prefs_file = "tp6-prefs.json"
            else:
                prefs_file = "prefs.json"

        cmd.append(
            "--preferences=%s" % os.path.join(self.awsy_path, "conf", prefs_file)
        )
        if dmd_enabled:
            cmd.append("--setpref=security.sandbox.content.level=0")
        cmd.append("--setpref=layout.css.stylo-threads=4")

        cmd.append(test_file)

        env["MOZ_UPLOAD_DIR"] = dirs["abs_blob_upload_dir"]
        if not os.path.isdir(env["MOZ_UPLOAD_DIR"]):
            self.mkdir_p(env["MOZ_UPLOAD_DIR"])
        if self.query_minidump_stackwalk():
            env["MINIDUMP_STACKWALK"] = self.minidump_stackwalk_path
        env["MINIDUMP_SAVE_PATH"] = dirs["abs_blob_upload_dir"]
        env["RUST_BACKTRACE"] = "1"
        env = self.query_env(partial_env=env)
        parser = StructuredOutputParser(
            config=self.config,
            log_obj=self.log_obj,
            error_list=self.error_list,
            strict=False,
        )
        return_code = self.run_command(
            command=cmd,
            cwd=self.awsy_path,
            output_timeout=self.config.get("cmd_timeout"),
            env=env,
            output_parser=parser,
        )

        level = INFO
        tbpl_status, log_level, summary = parser.evaluate_parser(
            return_code=return_code
        )

        self.log(
            "AWSY exited with return code %s: %s" % (return_code, tbpl_status),
            level=level,
        )
        self.record_status(tbpl_status)


if __name__ == "__main__":
    awsy_test = AWSY()
    awsy_test.run_and_exit()
