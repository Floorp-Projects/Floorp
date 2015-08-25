#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import copy
import getpass
import os
import re
import signal
import subprocess
import sys
import time
import socket

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.mozilla.buildbot import TBPL_SUCCESS, BuildbotMixin
from mozharness.base.errors import BaseErrorList
from mozharness.base.log import INFO, ERROR, FATAL
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.blob_upload import BlobUploadMixin, blobupload_config_options
from mozharness.mozilla.mozbase import MozbaseMixin
from mozharness.mozilla.testing.mozpool import MozpoolMixin
from mozharness.mozilla.testing.device import SUTDeviceMozdeviceMixin
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options

SUITE_CATEGORIES = ['mochitest', 'reftest', 'crashtest', 'jsreftest', 'robocop', 'instrumentation', 'xpcshell', 'jittest', 'cppunittest']


class PandaTest(TestingMixin, MercurialScript, BlobUploadMixin, MozpoolMixin, BuildbotMixin, SUTDeviceMozdeviceMixin, MozbaseMixin):
    test_suites = SUITE_CATEGORIES
    config_options = [
        [["--mozpool-api-url"], {
            "dest": "mozpool_api_url",
            "help": "Override mozpool api url",
        }],
        [["--mozpool-device"], {
            "dest": "mozpool_device",
            "help": "Set Panda device to run tests on",
        }],
        [["--mozpool-assignee"], {
            "dest": "mozpool_assignee",
            "help": "Set mozpool assignee (requestor name, free-form)",
        }],
        [["--total-chunks"], {
            "action": "store",
            "dest": "total_chunks",
            "help": "Number of total chunks",
        }],
        [["--this-chunk"], {
            "action": "store",
            "dest": "this_chunk",
            "help": "Number of this chunk",
        }],
        [["--extra-args"], {
            "action": "store",
            "dest": "extra_args",
            "help": "Extra arguments",
        }],
        [['--mochitest-suite', ], {
            "action": "extend",
            "dest": "specified_mochitest_suites",
            "type": "string",
            "help": "Specify which mochi suite to run. "
                    "Suites are defined in the config file.\n"
                    "Examples: 'all', 'plain1', 'plain5', 'chrome', or 'a11y'"}
         ],
        [['--reftest-suite', ], {
            "action": "extend",
            "dest": "specified_reftest_suites",
            "type": "string",
            "help": "Specify which reftest suite to run. "
                    "Suites are defined in the config file.\n"
                    "Examples: 'all', 'crashplan', or 'jsreftest'"}
         ],
        [['--crashtest-suite', ], {
            "action": "extend",
            "dest": "specified_crashtest_suites",
            "type": "string",
            "help": "Specify which crashtest suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'crashtest'"}
         ],
        [['--jsreftest-suite', ], {
            "action": "extend",
            "dest": "specified_jsreftest_suites",
            "type": "string",
            "help": "Specify which jsreftest suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'jsreftest'"}
         ],
        [['--robocop-suite', ], {
            "action": "extend",
            "dest": "specified_robocop_suites",
            "type": "string",
            "help": "Specify which robocop suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'robocop'"}
         ],
        [['--instrumentation-suite', ], {
            "action": "extend",
            "dest": "specified_instrumentation_suites",
            "type": "string",
            "help": "Specify which instrumentation suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'browser', 'background'"}
         ],
         [['--xpcshell-suite', ], {
            "action": "extend",
            "dest": "specified_xpcshell_suites",
            "type": "string",
            "help": "Specify which xpcshell suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'xpcshell'"}
         ],
         [['--jittest-suite', ], {
            "action": "extend",
            "dest": "specified_jittest_suites",
            "type": "string",
            "help": "Specify which jittest suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'jittest'"}
         ],
        [['--cppunittest-suite', ], {
            "action": "extend",
            "dest": "specified_cppunittest_suites",
            "type": "string",
            "help": "Specify which cpp unittest suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'cppunittest'"}
         ],
        [['--run-all-suites', ], {
            "action": "store_true",
            "dest": "run_all_suites",
            "default": False,
            "help": "This will run all suites that are specified "
                    "in the config file. You do not need to specify "
                    "any other suites. Beware, this may take a while ;)"}
         ],
    ] + copy.deepcopy(testing_config_options) + \
        copy.deepcopy(blobupload_config_options)

    error_list = []
    mozpool_handler = None

    virtualenv_modules = [
        'mozpoolclient',
    ]

    def __init__(self, require_config_file=False):
        super(PandaTest, self).__init__(
            config_options=self.config_options,
            all_actions=['clobber',
                         'read-buildbot-config',
                         'download-and-extract',
                         'create-virtualenv',
                         'request-device',
                         'run-test',
                         'close-request'],
            default_actions=['clobber',
                             'read-buildbot-config',
                             'download-and-extract',
                             'create-virtualenv',
                             'request-device',
                             'run-test',
                             'close-request'],
            require_config_file=require_config_file,
            config={'virtualenv_modules': self.virtualenv_modules})

        self.mozpool_assignee = self.config.get('mozpool_assignee', getpass.getuser())
        self.request_url = None
        self.installer_url = self.config.get("installer_url")
        self.test_url = self.config.get("test_url")
        self.mozpool_device = self.config.get("mozpool_device")
        self.symbols_url = self.config.get('symbols_url')

    def postflight_read_buildbot_config(self):
        super(PandaTest, self).postflight_read_buildbot_config()
        self.mozpool_device = self.config.get('mozpool_device', self.buildbot_config.get('properties')["slavename"])
        dirs = self.query_abs_dirs()
        #touch the shutdown file
        shutdown_file = os.path.join(dirs['shutdown_dir'], 'shutdown.stamp')
        try:
            self.info("*** Touching the shutdown file **")
            open(shutdown_file, 'w').close()
        except Exception, e:
            self.warning("We failed to create the shutdown file: str(%s)" % str(e))

    def request_device(self):
        self.retrieve_android_device(b2gbase="")
        env = self.query_env()
        cmd = [self.query_exe('python'), self.config.get("verify_path")]
        if self.run_command(cmd, env=env):
            self.critical("Preparing to abort run due to failed verify check.")
            self.close_request()
            self.fatal("Dying due to failing verification")
        else:
            self.info("Successfully verified the device")

    def _sut_prep_steps(self):
        device_time = self.set_device_epoch_time()
        self.info("Current time on device: %s - %s" %
                  (device_time, time.strftime("%x %H:%M:%S", time.gmtime(float(device_time)))))

    def download_and_extract(self):
        """
        Provides the target suite categories to TestingMixin.download_
        """
        if self.config.get('run_all_suites'):
            target_categories = SUITE_CATEGORIES
        else:
            target_categories = [cat for cat in SUITE_CATEGORIES
                                 if self._query_specified_suites(cat) is not None]
        super(PandaTest, self).download_and_extract(suite_categories=target_categories)

    def _run_category_suites(self, suite_category, preflight_run_method=None):
        """run suite(s) to a specific category"""

        env = self.query_env(partial_env={'DM_TRANS': "sut", 'TEST_DEVICE': self.mozpool_device})
        self.info("Running tests...")

        suites = self._query_specified_suites(suite_category)
        level = INFO

        if preflight_run_method:
            preflight_run_method(suites)
        if suites:
            self.info('#### Running %s suites' % suite_category)
            for suite in suites:
                dirs = self.query_abs_dirs()
                self._download_unzip_hostutils()
                abs_base_cmd = self._query_abs_base_cmd(suite_category, suite)

                should_install_app = True
                if 'cppunittest' in suite:
                    should_install_app = False
                if 'robocop' in suite:
                    self._download_robocop_apk()
                if 'jittest' in suite:
                    should_install_app = False

                if should_install_app:
                    self._install_app()
                cmd = abs_base_cmd[:]
                replace_dict = {}
                for arg in suites[suite]:
                    cmd.append(arg % replace_dict)

                cmd = self.append_harness_extra_args(cmd)

                tbpl_status, log_level = None, None
                error_list = BaseErrorList + [{
                    'regex': re.compile(r"(?:TEST-UNEXPECTED-FAIL|PROCESS-CRASH) \| .* \| (application crashed|missing output line for total leaks!|negative leaks caught!|\d+ bytes leaked)"),
                    'level': ERROR,
                }]
                c = self.config
                if c.get('minidump_stackwalk_path'):
                    env['MINIDUMP_STACKWALK'] = c['minidump_stackwalk_path']
                env['MOZ_UPLOAD_DIR'] = self.query_abs_dirs()['abs_blob_upload_dir']
                env['MINIDUMP_SAVE_PATH'] = self.query_abs_dirs()['abs_blob_upload_dir']

                env = self.query_env(partial_env=env, log_level=INFO)
                if env.has_key('PYTHONPATH'):
                    del env['PYTHONPATH']

                parser = self.get_test_output_parser(suite_category,
                                                     config=self.config,
                                                     error_list=error_list,
                                                     log_obj=self.log_obj)

                return_code = self.run_command(cmd,
                                               cwd=dirs['abs_test_install_dir'],
                                               env=env,
                                               output_parser=parser)

                tbpl_status, log_level = parser.evaluate_parser(return_code)

                if tbpl_status != TBPL_SUCCESS:
                    self.info("Output logcat...")
                    try:
                        lines = self.get_logcat()
                        self.info("*** STARTING LOGCAT ***")
                        for l in lines:
                            self.info(l)
                        self.info("*** END LOGCAT ***")
                    except Exception, e:
                        self.warning("We failed to run logcat: str(%s)" % str(e))

                parser.append_tinderboxprint_line(suite)
                self.buildbot_status(tbpl_status, level=level)

                self.log("The %s suite: %s ran with return status: %s" %
                         (suite_category, suite, tbpl_status), level=log_level)

    def _query_specified_suites(self, category):
        # logic goes: if at least one '--{category}-suite' was given,
        # then run only that(those) given suite(s). Elif no suites were
        # specified and the --run-all-suites flag was given,
        # run all {category} suites. Anything else, run no suites.
        c = self.config
        all_suites = c.get('all_%s_suites' % (category))
        specified_suites = c.get('specified_%s_suites' % (category))  # list

        suites = None

        if specified_suites:
            if 'all' in specified_suites:
                # useful if you want a quick way of saying run all suites
                # of a specific category.
                suites = all_suites
            else:
                # suites gets a dict of everything from all_suites where a key
                # is also in specified_suites
                suites = dict((key, all_suites.get(key)) for key in
                              specified_suites if key in all_suites.keys())
        else:
            if c.get('run_all_suites'):  # needed if you dont specify any suites
                suites = all_suites

        return suites

    def run_test(self):
       # do we need to set the device time? command doesn't work anyways
       # self._sut_prep_steps()
        env = self.query_env()
        env["DM_TRANS"] = "sut"
        env["TEST_DEVICE"] = self.mozpool_device
        self.mkdir_p(self.abs_dirs['abs_blob_upload_dir'])

        self._start_logcat()

        self.info("Running tests...")

        for category in SUITE_CATEGORIES:
            self._run_category_suites(category)

        self._stop_logcat()

    def _start_logcat(self):
        # Start logcat.py as a separate process continuously pulling logcat from
        # the device and writing to a file. Output is written directly to
        # the blobber upload directory so that it is uploaded automatically
        # at the end of the job.
        device_ip = socket.gethostbyname(self.mozpool_device)
        logcat_path = os.path.join(self.abs_dirs['abs_blob_upload_dir'], 'logcat.log')
        logcat_cmd = ['python', '-u', self.config.get("logcat_path"), \
            device_ip, logcat_path, '-v time']
        self.info('Starting logcat: %s' % str(logcat_cmd))
        self.logcat_proc = subprocess.Popen(logcat_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def _stop_logcat(self):
        # Signal logcat.py so that it can cleanup (kill the device logcat process)
        self.logcat_proc.send_signal(signal.SIGINT)
        self.logcat_proc.kill()
        out, err = self.logcat_proc.communicate()
        self.info("logcat.py output:\n%s\n%s\n" % (out, err))

    def _download_unzip_hostutils(self):
        c = self.config
        dirs = self.query_abs_dirs()
        self.host_utils_url = c['hostutils_url']
        #create the hostutils dir, get the zip and extract it
        self.mkdir_p(dirs['abs_hostutils_dir'])
        self._download_unzip(self.host_utils_url, dirs['abs_hostutils_dir'])

    def _install_app(self):
        c = self.config
        base_work_dir = c['base_work_dir']
        cmd = ['python', self.config.get("install_app_path"), self.device_ip, 'build/' + str(self.filename_apk), self.app_name]
        self.run_command(cmd, cwd=base_work_dir, halt_on_failure=True, fatal_exit_code=3)

    def _download_robocop_apk(self):
        dirs = self.query_abs_dirs()
        self.apk_url = self.installer_url[:self.installer_url.rfind('/')]
        robocop_url = self.apk_url + '/robocop.apk'
        self.info("Downloading robocop...")
        self.download_file(robocop_url, 'robocop.apk', dirs['abs_work_dir'], error_level=FATAL)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(PandaTest, self).query_abs_dirs()
        dirs = {}
        dirs['abs_test_install_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'tests')
        dirs['abs_test_bin_dir'] = os.path.join(dirs['abs_test_install_dir'], 'bin')
        dirs['abs_mochitest_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'mochitest')
        dirs['abs_reftest_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'reftest')
        dirs['abs_crashtest_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'reftest')
        dirs['abs_jsreftest_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'reftest')
        dirs['abs_xpcshell_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'xpcshell')
        dirs['abs_xre_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'xre')
        dirs['abs_utility_path'] = os.path.join(
            abs_dirs['abs_work_dir'], 'bin')
        dirs['abs_certificate_path'] = os.path.join(
            abs_dirs['abs_work_dir'], 'certs')
        dirs['abs_hostutils_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'hostutils')
        dirs['abs_robocop_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'mochitest')
        dirs['abs_instrumentation_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'instrumentation')
        dirs['abs_blob_upload_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'blobber_upload_dir')
        dirs['abs_jittest_dir'] = os.path.join(dirs['abs_test_install_dir'], "jit-test", "jit-test")
        dirs['shutdown_dir'] = abs_dirs['abs_work_dir'].rsplit("/", 2)[0]
        dirs['abs_cppunittest_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'cppunittest')
        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def _query_symbols_url(self):
        """query the full symbols URL based upon binary URL"""
        # may break with name convention changes but is one less 'input' for script
        if self.symbols_url:
            return self.symbols_url

    def _query_abs_base_cmd(self, suite_category, suite):
        #check for apk first with if ?
        c = self.config
        dirs = self.query_abs_dirs()
        options = []
        run_file = c["suite_definitions"][suite_category]["run_filename"]
        base_cmd = ['python', '-u']
        base_cmd.append(os.path.join((dirs["abs_%s_dir" % suite_category]), run_file))
        self.device_ip = socket.gethostbyname(self.mozpool_device)
        #applies to mochitest, reftest, jsreftest
        # TestingMixin._download_and_extract_symbols() will set
        # self.symbols_path when downloading/extracting.
        hostnumber = 0
        mozpool_device_list = self.mozpool_device.split('-')
        if len(mozpool_device_list) == 2:
            hostnumber = int(mozpool_device_list[1])
        http_port = '30%03i' % hostnumber
        ssl_port = '31%03i' % hostnumber
        #get filename from installer_url
        self.filename_apk = self.installer_url.split('/')[-1]
        #find appname from package-name.txt - assumes download-and-extract has completed successfully
        apk_dir = self.abs_dirs['abs_work_dir']
        self.apk_path = os.path.join(apk_dir, self.filename_apk)
        unzip = self.query_exe("unzip")
        package_path = os.path.join(apk_dir, 'package-name.txt')
        unzip_cmd = [unzip, '-q', '-o',  self.apk_path]
        self.run_command(unzip_cmd, cwd=apk_dir, halt_on_failure=True, fatal_exit_code=3)
        self.app_name = str(self.read_from_file(package_path, verbose=True)).rstrip()

        raw_log_file = os.path.join(dirs['abs_blob_upload_dir'],
                                    '%s_raw.log' % suite)
        error_summary_file = os.path.join(dirs['abs_blob_upload_dir'],
                                          '%s_errorsummary.log' % suite)
        str_format_values = {
            'device_ip': self.device_ip,
            'hostname': self.mozpool_device,
            'symbols_path': self._query_symbols_url(),
            'http_port': http_port,
            'ssl_port':  ssl_port,
            'app_name':  self.app_name,
            'apk_name':  self.filename_apk,
            'apk_path':  self.apk_path,
            'raw_log_file': raw_log_file,
            'error_summary_file': error_summary_file,
        }
        if "suite_definitions" in c and \
                suite_category in c["suite_definitions"]: # new in-tree format
            for option in c["suite_definitions"][suite_category]["options"]:
                options.append(option % str_format_values)
            abs_base_cmd = base_cmd + options
            return abs_base_cmd
        else:
            self.warning("Suite options for %s could not be determined."
                         "\nIf you meant to have options for this suite, "
                         "please make sure they are specified in your "
                         "config." % suite_category)

    ###### helper methods
    def _pre_config_lock(self, rw_config):
        super(PandaTest, self)._pre_config_lock(rw_config)
        c = self.config
        if not c.get('run_all_suites'):
            return  # configs are valid
        for category in SUITE_CATEGORIES:
            specific_suites = c.get('specified_%s_suites' % (category))
            if specific_suites:
                if specific_suites != 'all':
                    self.fatal("Config options are not valid. Please ensure"
                               " that if the '--run-all-suites' flag was enabled,"
                               " then do not specify to run only specific suites "
                               "like:\n '--mochitest-suite browser-chrome'")

    def close_request(self):
        if self.request_url:
            mph = self.query_mozpool_handler(self.mozpool_device)
            mph.close_request(self.request_url)
            self.info("Request '%s' deleted on cleanup" % self.request_url)
            self.request_url = None
        else:
            self.info("request_url doesn't exist. Already closed?")

    def _build_arg(self, option, value):
        """
        Build a command line argument
        """
        if not value:
            return []
        return [str(option), str(value)]

if __name__ == '__main__':
    pandaTest = PandaTest()
    pandaTest.run_and_exit()
