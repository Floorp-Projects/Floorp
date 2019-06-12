#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import copy
import datetime
import json
import os
import sys
import subprocess

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.log import FATAL
from mozharness.base.script import BaseScript, PreScriptAction
from mozharness.mozilla.automation import TBPL_RETRY
from mozharness.mozilla.mozbase import MozbaseMixin
from mozharness.mozilla.testing.android import AndroidMixin
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.mozilla.testing.codecoverage import (
    CodeCoverageMixin,
    code_coverage_config_options
)

SUITE_DEFAULT_E10S = ['geckoview-junit', 'mochitest', 'reftest']
SUITE_NO_E10S = ['cppunittest', 'geckoview-junit', 'xpcshell']


class AndroidEmulatorTest(TestingMixin, BaseScript, MozbaseMixin, CodeCoverageMixin,
                          AndroidMixin):
    """
       A mozharness script for Android functional tests (like mochitests and reftests)
       run on an Android emulator. This script starts and manages an Android emulator
       for the duration of the required tests. This is like desktop_unittest.py, but
       for Android emulator test platforms.
    """
    config_options = [[
        ["--test-suite"],
        {"action": "store",
         "dest": "test_suite",
         "default": None
         }
    ], [
        ["--total-chunk"],
        {"action": "store",
         "dest": "total_chunks",
         "default": None,
         "help": "Number of total chunks",
         }
    ], [
        ["--this-chunk"],
        {"action": "store",
         "dest": "this_chunk",
         "default": None,
         "help": "Number of this chunk",
         }
    ], [
        ["--gpu-required"],
        {"action": "store_true",
         "dest": "gpu_required",
         "default": False,
         "help": "Run additional verification on modified tests using gpu instances.",
         }
    ], [
        ["--log-raw-level"],
        {"action": "store",
         "dest": "log_raw_level",
         "default": "info",
         "help": "Set log level (debug|info|warning|error|critical|fatal)",
         }
    ], [
        ["--log-tbpl-level"],
        {"action": "store",
         "dest": "log_tbpl_level",
         "default": "info",
         "help": "Set log level (debug|info|warning|error|critical|fatal)",
         }
    ], [
        ['--disable-e10s', ],
        {"action": "store_false",
         "dest": "e10s",
         "default": True,
         "help": "Run tests without multiple processes (e10s).",
         }
    ], [
        ['--enable-webrender'],
        {"action": "store_true",
         "dest": "enable_webrender",
         "default": False,
         "help": "Run with WebRender enabled.",
         }
    ], [
        ['--disable-webrender'],
        {"action": "store_true",
         "dest": "disable_webrender",
         "default": False,
         "help": "Run with WebRender force-disabled.",
         }
    ]] + copy.deepcopy(testing_config_options) + \
        copy.deepcopy(code_coverage_config_options)

    def __init__(self, require_config_file=False):
        super(AndroidEmulatorTest, self).__init__(
            config_options=self.config_options,
            all_actions=['clobber',
                         'setup-avds',
                         'start-emulator',
                         'download-and-extract',
                         'create-virtualenv',
                         'verify-device',
                         'install',
                         'run-tests',
                         ],
            require_config_file=require_config_file,
            config={
                'virtualenv_modules': [],
                'virtualenv_requirements': [],
                'require_test_zip': True,
            }
        )

        # these are necessary since self.config is read only
        c = self.config
        abs_dirs = self.query_abs_dirs()
        self.installer_url = c.get('installer_url')
        self.installer_path = c.get('installer_path')
        self.test_url = c.get('test_url')
        self.test_packages_url = c.get('test_packages_url')
        self.test_manifest = c.get('test_manifest')
        self.robocop_path = os.path.join(abs_dirs['abs_work_dir'], "robocop.apk")
        suite = c.get('test_suite')
        if suite and '-chunked' in suite:
            suite = suite[:suite.index('-chunked')]
        self.test_suite = suite
        self.this_chunk = c.get('this_chunk')
        self.total_chunks = c.get('total_chunks')
        self.xre_path = None
        self.device_serial = 'emulator-5554'
        self.log_raw_level = c.get('log_raw_level')
        self.log_tbpl_level = c.get('log_tbpl_level')
        self.e10s = c.get('e10s')
        self.enable_webrender = c.get('enable_webrender')
        self.disable_webrender = c.get('disable_webrender')
        if self.enable_webrender:
            # AndroidMixin uses this when launching the emulator. We only want
            # GLES3 if we're running WebRender
            self.use_gles3 = True

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(AndroidEmulatorTest, self).query_abs_dirs()
        dirs = {}
        dirs['abs_test_install_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'tests')
        dirs['abs_test_bin_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'tests', 'bin')
        dirs['abs_xre_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'hostutils')
        dirs['abs_modules_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'modules')
        dirs['abs_blob_upload_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'blobber_upload_dir')
        dirs['abs_mochitest_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'mochitest')
        dirs['abs_reftest_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'reftest')
        dirs['abs_xpcshell_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'xpcshell')
        dirs['abs_marionette_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'marionette', 'harness', 'marionette_harness')
        dirs['abs_marionette_tests_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'marionette', 'tests', 'testing',
            'marionette', 'harness', 'marionette_harness', 'tests')
        dirs['abs_avds_dir'] = self.config.get("avds_dir", "/home/cltbld/.android")

        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def _query_tests_dir(self, test_suite):
        dirs = self.query_abs_dirs()
        try:
            test_dir = self.config["suite_definitions"][test_suite]["testsdir"]
        except Exception:
            test_dir = test_suite
        return os.path.join(dirs['abs_test_install_dir'], test_dir)

    def _get_mozharness_test_paths(self, suite):
        test_paths = os.environ.get('MOZHARNESS_TEST_PATHS')
        if not test_paths:
            return

        return json.loads(test_paths).get(suite)

    def _build_command(self):
        c = self.config
        dirs = self.query_abs_dirs()

        if self.test_suite not in self.config["suite_definitions"]:
            self.fatal("Key '%s' not defined in the config!" % self.test_suite)

        cmd = [
            self.query_python_path('python'),
            '-u',
            os.path.join(
                self._query_tests_dir(self.test_suite),
                self.config["suite_definitions"][self.test_suite]["run_filename"]
            ),
        ]

        raw_log_file = os.path.join(dirs['abs_blob_upload_dir'],
                                    '%s_raw.log' % self.test_suite)

        error_summary_file = os.path.join(dirs['abs_blob_upload_dir'],
                                          '%s_errorsummary.log' % self.test_suite)
        str_format_values = {
            'device_serial': self.device_serial,
            # IP address of the host as seen from the emulator
            'remote_webserver': '10.0.2.2',
            'xre_path': self.xre_path,
            'utility_path': self.xre_path,
            'http_port': '8854',  # starting http port  to use for the mochitest server
            'ssl_port': '4454',  # starting ssl port to use for the server
            'certs_path': os.path.join(dirs['abs_work_dir'], 'tests/certs'),
            # TestingMixin._download_and_extract_symbols() will set
            # self.symbols_path when downloading/extracting.
            'symbols_path': self.symbols_path,
            'modules_dir': dirs['abs_modules_dir'],
            'installer_path': self.installer_path,
            'raw_log_file': raw_log_file,
            'log_tbpl_level': self.log_tbpl_level,
            'log_raw_level': self.log_raw_level,
            'error_summary_file': error_summary_file,
            'gecko_log': '-',
            # marionette options
            'address': c.get('marionette_address'),
            'marionette_extra': c.get('marionette_extra', ''),
            'xpcshell_extra': c.get('xpcshell_extra', ''),
            'test_manifest': os.path.join(
                dirs['abs_marionette_tests_dir'],
                self.config.get('marionette_test_manifest', '')
            ),
            'gtest_dir': os.path.join(dirs['abs_test_install_dir'], 'gtest'),
        }

        user_paths = self._get_mozharness_test_paths(self.test_suite)

        for option in self.config["suite_definitions"][self.test_suite]["options"]:
            opt = option.split('=')[0]
            # override configured chunk options with script args, if specified
            if opt in ('--this-chunk', '--total-chunks'):
                if user_paths or getattr(self, opt.replace('-', '_').strip('_'), None) is not None:
                    continue

            if '%(app)' in option:
                # only query package name if requested
                cmd.extend([option % {'app': self.query_package_name()}])
            else:
                option = option % str_format_values
                if option:
                    cmd.extend([option])

        if 'mochitest' in self.test_suite:
            category = 'mochitest'
        elif 'reftest' in self.test_suite or 'crashtest' in self.test_suite:
            category = 'reftest'
        else:
            category = self.test_suite
        if category not in SUITE_NO_E10S:
            if category in SUITE_DEFAULT_E10S and not self.e10s:
                cmd.extend(['--disable-e10s'])
            elif category not in SUITE_DEFAULT_E10S and self.e10s:
                cmd.extend(['--e10s'])

        if not (self.verify_enabled or self.per_test_coverage):
            if user_paths:
                cmd.extend(user_paths)
            elif not (self.verify_enabled or self.per_test_coverage):
                if self.this_chunk is not None:
                    cmd.extend(['--this-chunk', self.this_chunk])
                if self.total_chunks is not None:
                    cmd.extend(['--total-chunks', self.total_chunks])

        if self.disable_webrender:
            cmd.extend(['--setenv', 'MOZ_WEBRENDER=0'])
        elif self.enable_webrender:
            cmd.extend(['--setenv', 'MOZ_WEBRENDER=1'])

        try_options, try_tests = self.try_args(self.test_suite)
        cmd.extend(try_options)
        if not self.verify_enabled and not self.per_test_coverage:
            cmd.extend(self.query_tests_args(
                self.config["suite_definitions"][self.test_suite].get("tests"),
                None,
                try_tests))

        if self.java_code_coverage_enabled:
            cmd.extend(['--enable-coverage',
                        '--coverage-output-dir', self.java_coverage_output_dir])

        return cmd

    def _query_suites(self):
        if self.test_suite:
            return [(self.test_suite, self.test_suite)]
        # per-test mode: determine test suites to run

        # For each test category, provide a list of supported sub-suites and a mapping
        # between the per_test_base suite name and the android suite name.
        all = [('mochitest', {'mochitest-plain': 'mochitest-plain',
                              'mochitest-chrome': 'mochitest-chrome',
                              'mochitest-media': 'mochitest-media',
                              'mochitest-plain-gpu': 'mochitest-plain-gpu'}),
               ('reftest', {'reftest': 'reftest',
                            'crashtest': 'crashtest',
                            'jsreftest': 'jsreftest'}),
               ('xpcshell', {'xpcshell': 'xpcshell'})]
        suites = []
        for (category, all_suites) in all:
            cat_suites = self.query_per_test_category_suites(category, all_suites)
            for k in cat_suites.keys():
                suites.append((k, cat_suites[k]))
        return suites

    def _query_suite_categories(self):
        if self.test_suite:
            categories = [self.test_suite]
        else:
            # per-test mode
            categories = ['mochitest', 'reftest', 'xpcshell']
        return categories

    ##########################################
    # Actions for AndroidEmulatorTest        #
    ##########################################

    def preflight_install(self):
        # in the base class, this checks for mozinstall, but we don't use it
        pass

    @PreScriptAction('create-virtualenv')
    def pre_create_virtualenv(self, action):
        dirs = self.query_abs_dirs()
        requirements = None
        suites = self._query_suites()
        if ('mochitest-media', 'mochitest-media') in suites:
            # mochitest-media is the only thing that needs this
            requirements = os.path.join(dirs['abs_mochitest_dir'],
                                        'websocketprocessbridge',
                                        'websocketprocessbridge_requirements.txt')
        elif ('marionette', 'marionette') in suites:
            requirements = os.path.join(dirs['abs_test_install_dir'],
                                        'config', 'marionette_requirements.txt')
        if requirements:
            self.register_virtualenv_module(requirements=[requirements],
                                            two_pass=True)

    def download_and_extract(self):
        """
        Download and extract fennec APK, tests.zip, host utils, and robocop (if required).
        """
        super(AndroidEmulatorTest, self).download_and_extract(
            suite_categories=self._query_suite_categories())
        dirs = self.query_abs_dirs()
        if self.test_suite and self.test_suite.startswith('robocop'):
            robocop_url = self.installer_url[:self.installer_url.rfind('/')] + '/robocop.apk'
            self.info("Downloading robocop...")
            self.download_file(robocop_url, 'robocop.apk', dirs['abs_work_dir'], error_level=FATAL)
        self.xre_path = self.download_hostutils(dirs['abs_xre_dir'])

    def install(self):
        """
        Install APKs on the device.
        """
        install_needed = (not self.test_suite) or \
            self.config["suite_definitions"][self.test_suite].get("install")
        if install_needed is False:
            self.info("Skipping apk installation for %s" % self.test_suite)
            return
        assert self.installer_path is not None, \
            "Either add installer_path to the config or use --installer-path."
        self.install_apk(self.installer_path)
        # Install Robocop if required
        if self.test_suite and self.test_suite.startswith('robocop'):
            self.install_apk(self.robocop_path)
        self.info("Finished installing apps for %s" % self.device_serial)

    def run_tests(self):
        """
        Run the tests
        """
        self.start_time = datetime.datetime.now()
        max_per_test_time = datetime.timedelta(minutes=60)

        per_test_args = []
        suites = self._query_suites()
        minidump = self.query_minidump_stackwalk()
        for (per_test_suite, suite) in suites:
            self.test_suite = suite

            cmd = self._build_command()

            try:
                cwd = self._query_tests_dir(self.test_suite)
            except Exception:
                self.fatal("Don't know how to run --test-suite '%s'!" % self.test_suite)

            env = self.query_env()
            if minidump:
                env['MINIDUMP_STACKWALK'] = minidump
            env['MOZ_UPLOAD_DIR'] = self.query_abs_dirs()['abs_blob_upload_dir']
            env['MINIDUMP_SAVE_PATH'] = self.query_abs_dirs()['abs_blob_upload_dir']
            env['RUST_BACKTRACE'] = 'full'

            summary = {}
            for per_test_args in self.query_args(per_test_suite):
                if (datetime.datetime.now() - self.start_time) > max_per_test_time:
                    # Running tests has run out of time. That is okay! Stop running
                    # them so that a task timeout is not triggered, and so that
                    # (partial) results are made available in a timely manner.
                    self.info("TinderboxPrint: Running tests took too long: "
                              "Not all tests were executed.<br/>")
                    # Signal per-test time exceeded, to break out of suites and
                    # suite categories loops also.
                    return

                final_cmd = copy.copy(cmd)
                if len(per_test_args) > 0:
                    # in per-test mode, remove any chunk arguments from command
                    for arg in final_cmd:
                        if 'total-chunk' in arg or 'this-chunk' in arg:
                            final_cmd.remove(arg)
                final_cmd.extend(per_test_args)

                self.info("Running the command %s" % subprocess.list2cmdline(final_cmd))
                self.info("##### %s log begins" % self.test_suite)

                suite_category = self.test_suite
                parser = self.get_test_output_parser(
                    suite_category,
                    config=self.config,
                    log_obj=self.log_obj,
                    error_list=[])
                self.run_command(final_cmd, cwd=cwd, env=env, output_parser=parser)
                tbpl_status, log_level, summary = parser.evaluate_parser(0,
                                                                         previous_summary=summary)
                parser.append_tinderboxprint_line(self.test_suite)

                self.info("##### %s log ends" % self.test_suite)

                if len(per_test_args) > 0:
                    self.record_status(tbpl_status, level=log_level)
                    self.log_per_test_status(per_test_args[-1], tbpl_status, log_level)
                    if tbpl_status == TBPL_RETRY:
                        self.info("Per-test run abandoned due to RETRY status")
                        return
                else:
                    self.record_status(tbpl_status, level=log_level)
                    self.log("The %s suite: %s ran with return status: %s" %
                             (suite_category, suite, tbpl_status), level=log_level)


if __name__ == '__main__':
    test = AndroidEmulatorTest()
    test.run_and_exit()
