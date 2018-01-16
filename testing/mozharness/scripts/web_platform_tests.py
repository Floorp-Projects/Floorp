#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
import copy
import os
import sys

from datetime import datetime, timedelta

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.errors import BaseErrorList
from mozharness.base.script import PreScriptAction
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.blob_upload import BlobUploadMixin, blobupload_config_options
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.mozilla.testing.codecoverage import (
    CodeCoverageMixin,
    code_coverage_config_options
)
from mozharness.mozilla.testing.errors import HarnessErrorList

from mozharness.mozilla.structuredlog import StructuredOutputParser
from mozharness.base.log import INFO


class WebPlatformTest(TestingMixin, MercurialScript, BlobUploadMixin, CodeCoverageMixin):
    config_options = [
        [['--test-type'], {
            "action": "extend",
            "dest": "test_type",
            "help": "Specify the test types to run."}
         ],
        [['--e10s'], {
            "action": "store_true",
            "dest": "e10s",
            "default": False,
            "help": "Run with e10s enabled"}
         ],
        [["--total-chunks"], {
            "action": "store",
            "dest": "total_chunks",
            "help": "Number of total chunks"}
         ],
        [["--this-chunk"], {
            "action": "store",
            "dest": "this_chunk",
            "help": "Number of this chunk"}
         ],
        [["--allow-software-gl-layers"], {
            "action": "store_true",
            "dest": "allow_software_gl_layers",
            "default": False,
            "help": "Permits a software GL implementation (such as LLVMPipe) "
                    "to use the GL compositor."}
         ],
        [["--enable-webrender"], {
            "action": "store_true",
            "dest": "enable_webrender",
            "default": False,
            "help": "Tries to enable the WebRender compositor."}
         ],
        [["--headless"], {
            "action": "store_true",
            "dest": "headless",
            "default": False,
            "help": "Run tests in headless mode."}
         ],
        [["--headless-width"], {
            "action": "store",
            "dest": "headless_width",
            "default": "1600",
            "help": "Specify headless fake screen width (default: 1600)."}
         ],
        [["--headless-height"], {
            "action": "store",
            "dest": "headless_height",
            "default": "1200",
            "help": "Specify headless fake screen height (default: 1200)."}
         ],
        [["--single-stylo-traversal"], {
            "action": "store_true",
            "dest": "single_stylo_traversal",
            "default": False,
            "help": "Forcibly enable single thread traversal in Stylo with STYLO_THREADS=1"}
         ],
        [["--enable-stylo"], {
            "action": "store_true",
            "dest": "enable_stylo",
            "default": False,
            "help": "Run tests with Stylo enabled"}
         ],
        [["--disable-stylo"], {
            "action": "store_true",
            "dest": "disable_stylo",
            "default": False,
            "help": "Run tests with Stylo disabled"}
         ],
    ] + copy.deepcopy(testing_config_options) + \
        copy.deepcopy(blobupload_config_options) + \
        copy.deepcopy(code_coverage_config_options)

    def __init__(self, require_config_file=True):
        super(WebPlatformTest, self).__init__(
            config_options=self.config_options,
            all_actions=[
                'clobber',
                'read-buildbot-config',
                'download-and-extract',
                'create-virtualenv',
                'pull',
                'install',
                'run-tests',
            ],
            require_config_file=require_config_file,
            config={'require_test_zip': True})

        # Surely this should be in the superclass
        c = self.config
        self.installer_url = c.get('installer_url')
        self.test_url = c.get('test_url')
        self.test_packages_url = c.get('test_packages_url')
        self.installer_path = c.get('installer_path')
        self.binary_path = c.get('binary_path')
        self.abs_app_dir = None

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
        dirs['abs_app_install_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'application')
        dirs['abs_test_install_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'tests')
        dirs['abs_test_bin_dir'] = os.path.join(dirs['abs_test_install_dir'], 'bin')
        dirs["abs_wpttest_dir"] = os.path.join(dirs['abs_test_install_dir'], "web-platform")
        dirs['abs_blob_upload_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'blobber_upload_dir')

        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs

        return self.abs_dirs

    @PreScriptAction('create-virtualenv')
    def _pre_create_virtualenv(self, action):
        dirs = self.query_abs_dirs()

        requirements = os.path.join(dirs['abs_test_install_dir'],
                                    'config',
                                    'marionette_requirements.txt')

        self.register_virtualenv_module(requirements=[requirements],
                                        two_pass=True)

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
        dirs = self.query_abs_dirs()
        abs_app_dir = self.query_abs_app_dir()
        run_file_name = "runtests.py"

        cmd = [self.query_python_path('python'), '-u']
        cmd.append(os.path.join(dirs["abs_wpttest_dir"], run_file_name))

        # Make sure that the logging directory exists
        if self.mkdir_p(dirs["abs_blob_upload_dir"]) == -1:
            self.fatal("Could not create blobber upload directory")
            # Exit

        cmd += ["--log-raw=-",
                "--log-raw=%s" % os.path.join(dirs["abs_blob_upload_dir"],
                                              "wpt_raw.log"),
                "--log-wptreport=%s" % os.path.join(dirs["abs_blob_upload_dir"],
                                                    "wptreport.json"),
                "--log-errorsummary=%s" % os.path.join(dirs["abs_blob_upload_dir"],
                                                       "wpt_errorsummary.log"),
                "--binary=%s" % self.binary_path,
                "--symbols-path=%s" % self.query_symbols_url(),
                "--stackwalk-binary=%s" % self.query_minidump_stackwalk(),
                "--stackfix-dir=%s" % os.path.join(dirs["abs_test_install_dir"], "bin"),
                "--run-by-dir=3",
                "--no-pause-after-test"]

        if not sys.platform.startswith("linux"):
            cmd += ["--exclude=css"]

        for test_type in test_types:
            cmd.append("--test-type=%s" % test_type)

        if not c["e10s"]:
            cmd.append("--disable-e10s")

        if c["single_stylo_traversal"]:
            cmd.append("--stylo-threads=1")
        else:
            cmd.append("--stylo-threads=4")

        for opt in ["total_chunks", "this_chunk"]:
            val = c.get(opt)
            if val:
                cmd.append("--%s=%s" % (opt.replace("_", "-"), val))

        if "wdspec" in test_types:
            geckodriver_path = self._query_geckodriver()
            if not geckodriver_path or not os.path.isfile(geckodriver_path):
                self.fatal("Unable to find geckodriver binary "
                           "in common test package: %s" % str(geckodriver_path))
            cmd.append("--webdriver-binary=%s" % geckodriver_path)

        options = list(c.get("options", []))

        str_format_values = {
            'binary_path': self.binary_path,
            'test_path': dirs["abs_wpttest_dir"],
            'test_install_path': dirs["abs_test_install_dir"],
            'abs_app_dir': abs_app_dir,
            'abs_work_dir': dirs["abs_work_dir"]
            }

        test_type_suite = {
            "testharness": "web-platform-tests",
            "reftest": "web-platform-tests-reftests",
            "wdspec": "web-platform-tests-wdspec",
        }
        for test_type in test_types:
            try_options, try_tests = self.try_args(test_type_suite[test_type])

            cmd.extend(self.query_options(options,
                                          try_options,
                                          str_format_values=str_format_values))
            cmd.extend(self.query_tests_args(try_tests,
                                             str_format_values=str_format_values))

        return cmd

    def download_and_extract(self):
        super(WebPlatformTest, self).download_and_extract(
            extract_dirs=["mach",
                          "bin/*",
                          "config/*",
                          "mozbase/*",
                          "marionette/*",
                          "tools/*",
                          "web-platform/*"],
            suite_categories=["web-platform"])

    def _install_fonts(self):
        # Ensure the Ahem font is available
        dirs = self.query_abs_dirs()

        if not sys.platform.startswith("darwin"):
            font_path = os.path.join(os.path.dirname(self.binary_path), "fonts")
        else:
            font_path = os.path.join(os.path.dirname(self.binary_path), os.pardir,
                                     "Resources", "res", "fonts")
        if not os.path.exists(font_path):
            os.makedirs(font_path)
        ahem_src = os.path.join(dirs["abs_wpttest_dir"], "tests", "fonts", "Ahem.ttf")
        ahem_dest = os.path.join(font_path, "Ahem.ttf")
        with open(ahem_src, "rb") as src, open(ahem_dest, "wb") as dest:
            dest.write(src.read())

    def run_tests(self):
        dirs = self.query_abs_dirs()

        self._install_fonts()

        parser = StructuredOutputParser(config=self.config,
                                        log_obj=self.log_obj,
                                        log_compact=True,
                                        error_list=BaseErrorList + HarnessErrorList)

        env = {'MINIDUMP_SAVE_PATH': dirs['abs_blob_upload_dir']}
        env['RUST_BACKTRACE'] = 'full'

        if self.config['allow_software_gl_layers']:
            env['MOZ_LAYERS_ALLOW_SOFTWARE_GL'] = '1'
        if self.config['enable_webrender']:
            env['MOZ_WEBRENDER'] = '1'
            env['MOZ_ACCELERATED'] = '1'
        if self.config['headless']:
            env['MOZ_HEADLESS'] = '1'
            env['MOZ_HEADLESS_WIDTH'] = self.config['headless_width']
            env['MOZ_HEADLESS_HEIGHT'] = self.config['headless_height']

        if self.config['disable_stylo']:
            if self.config['single_stylo_traversal']:
                self.fatal("--disable-stylo conflicts with --single-stylo-traversal")
            if self.config['enable_stylo']:
                self.fatal("--disable-stylo conflicts with --enable-stylo")

        if self.config['single_stylo_traversal']:
            env['STYLO_THREADS'] = '1'
        else:
            env['STYLO_THREADS'] = '4'

        if self.config['enable_stylo']:
            env['STYLO_FORCE_ENABLED'] = '1'
        if self.config['disable_stylo']:
            env['STYLO_FORCE_DISABLED'] = '1'

        env = self.query_env(partial_env=env, log_level=INFO)

        start_time = datetime.now()
        max_verify_time = timedelta(minutes=60)
        max_verify_tests = 10
        verified_tests = 0

        if self.config.get("verify") is True:
            verify_suites = self.query_verify_category_suites(None, None)
            if "wdspec" in verify_suites:
                # geckodriver is required for wdspec, but not always available
                geckodriver_path = self._query_geckodriver()
                if not geckodriver_path or not os.path.isfile(geckodriver_path):
                    verify_suites.remove("wdspec")
                    self.info("Test verification skipping 'wdspec' tests - no geckodriver")
        else:
            test_types = self.config.get("test_type", [])
            verify_suites = [None]
        for verify_suite in verify_suites:
            if verify_suite:
                test_types = [verify_suite]
            for verify_args in self.query_verify_args(verify_suite):
                if (datetime.now() - start_time) > max_verify_time:
                    # Verification has run out of time. That is okay! Stop running
                    # tests so that a task timeout is not triggered, and so that
                    # (partial) results are made available in a timely manner.
                    self.info("TinderboxPrint: Verification too long: Not all tests "
                              "were verified.<br/>")
                    return
                if verified_tests >= max_verify_tests:
                    # When changesets are merged between trees or many tests are
                    # otherwise updated at once, there probably is not enough time
                    # to verify all tests, and attempting to do so may cause other
                    # problems, such as generating too much log output.
                    self.info("TinderboxPrint: Too many modified tests: Not all tests "
                              "were verified.<br/>")
                    return
                verified_tests = verified_tests + 1

                cmd = self._query_cmd(test_types)
                cmd.extend(verify_args)

                return_code = self.run_command(cmd,
                                               cwd=dirs['abs_work_dir'],
                                               output_timeout=1000,
                                               output_parser=parser,
                                               env=env)

                tbpl_status, log_level = parser.evaluate_parser(return_code)
                self.buildbot_status(tbpl_status, level=log_level)

                if len(verify_args) > 0:
                    self.log_verify_status(verify_args[-1], tbpl_status, log_level)


# main {{{1
if __name__ == '__main__':
    web_platform_tests = WebPlatformTest()
    web_platform_tests.run_and_exit()
