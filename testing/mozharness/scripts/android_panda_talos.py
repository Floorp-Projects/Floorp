#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import copy
import getpass
import os
import pprint
import sys
import time
import socket

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.mozilla.buildbot import BuildbotMixin
from mozharness.base.config import parse_config_file
from mozharness.base.log import INFO, FATAL
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.blob_upload import BlobUploadMixin, blobupload_config_options
from mozharness.mozilla.testing.mozpool import MozpoolMixin
from mozharness.mozilla.testing.device import SUTDeviceMozdeviceMixin
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.mozilla.testing.talos import TalosOutputParser, TalosErrorList

SUITE_CATEGORIES = ['talos', ]


class PandaTalosTest(TestingMixin, MercurialScript, BlobUploadMixin, MozpoolMixin, BuildbotMixin, SUTDeviceMozdeviceMixin):
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
        [['--talos-suite', ], {
            "action": "extend",
            "dest": "specified_talos_suites",
            "type": "string",
            "help": "Specify which talos suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'remote-trobocheck', 'remote-trobocheck2', 'remote-trobopan', 'remote-troboprovider', 'remote-tsvg', 'tpn', 'ts'",
        }],
        [["--branch-name"], {
            "action": "store",
            "dest": "talos_branch",
            "help": "Graphserver branch to report to"
        }],
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
        'mozcrash'
    ]

    def __init__(self, require_config_file=False):
        super(PandaTalosTest, self).__init__(
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
        self.test_url = self.config.get("test_url")
        self.mozpool_device = self.config.get("mozpool_device")
        self.talos_branch = self.config.get("talos_branch")

        self.read_buildbot_config()
        self.revision = self.config.get('revision',
                                        self.buildbot_config.get('properties')["revision"])
        self.repo_path = self.config.get('repo_path',
                                         self.buildbot_config.get('properties')["repo_path"])
        self.talos_json_url = (self.config.get("talos_json_url") % (self.repo_path, self.revision))
        self.talos_json_config = None

    def postflight_read_buildbot_config(self):
        super(PandaTalosTest, self).postflight_read_buildbot_config()
        self.mozpool_device = self.config.get('mozpool_device', self.buildbot_config.get('properties')["slavename"])
        dirs = self.query_abs_dirs()
        #touch the shutdown file
        shutdown_file = os.path.join(dirs['shutdown_dir'], 'shutdown.stamp')
        try:
            self.info("*** Touching the shutdown file **")
            open(shutdown_file, 'w').close()
        except Exception, e:
            self.warning("We failed to create the shutdown file: str(%s)" % str(e))

    def query_talos_json_config(self):
        if self.talos_json_config:
            return self.talos_json_config

        dirs = self.query_abs_dirs()
        self.talos_json = self.download_file(self.talos_json_url,
                                             parent_dir=dirs['abs_talosdata_dir'],
                                             error_level=FATAL)
        self.talos_json_config = parse_config_file(self.talos_json)
        self.info(pprint.pformat(self.talos_json_config))

        return self.talos_json_config

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

    def preflight_talos(self, suite_category, suites):
        """preflight perf config etc"""
        env = self.query_env(partial_env={'DM_TRANS': "sut", 'TEST_DEVICE': self.mozpool_device})
        self.info("Running preflight...")
        preflight_category = "preflight_" + str(suite_category)
        dirs = self.query_abs_dirs()
        abs_base_cmd = self._query_abs_base_cmd(preflight_category)
        cmd = abs_base_cmd[:]
        replace_dict = {}
        for suite in suites:
            for arg in suites[suite]:
                cmd.append(arg % replace_dict)
        self._install_app()
        self.run_command(cmd, dirs['abs_talosdatatalos_dir'], env=env, halt_on_failure=True, fatal_exit_code=suites.get('fatal_exit_code', 3))

    def _run_category_suites(self, suite_category, preflight_run_method=None):
        """run suite(s) to a specific category"""

        env = self.query_env(partial_env={'DM_TRANS': "sut", 'TEST_DEVICE': self.mozpool_device})
        self.info("Running tests...")

        suites = self._query_specified_suites(suite_category)

        if preflight_run_method:
            preflight_run_method(suite_category, suites)
        if suites:
            self.info('#### Running %s suites' % suite_category)
            for suite in suites:
                dirs = self.query_abs_dirs()
                abs_base_cmd = self._query_abs_base_cmd(suite_category)
                cmd = abs_base_cmd[:]
                tbpl_status, log_level = None, None
                c = self.config
                if c.get('minidump_stackwalk_path'):
                    env['MINIDUMP_STACKWALK'] = c['minidump_stackwalk_path']
                env['MOZ_UPLOAD_DIR'] = self.query_abs_dirs()['abs_blob_upload_dir']
                env['MINIDUMP_SAVE_PATH'] = self.query_abs_dirs()['abs_blob_upload_dir']
                if not os.path.isdir(env['MOZ_UPLOAD_DIR']):
                    self.mkdir_p(env['MOZ_UPLOAD_DIR'])
                env = self.query_env(partial_env=env, log_level=INFO)

                parser = TalosOutputParser(config=self.config, log_obj=self.log_obj,
                                           error_list=TalosErrorList)

                if parser.minidump_output:
                    self.info("Looking at the minidump files for debugging purposes...")
                    for item in parser.minidump_output:
                        self.run_command(["ls", "-l", item])
                return_code = self.run_command(cmd, dirs['abs_talosdatatalos_dir'], env=env, output_parser=parser)
                if return_code != 0:
                    self.fatal("Failed talos " + str(cmd) + " command run.")

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
        self.info("Running tests...")

        for category in SUITE_CATEGORIES:
            self._run_category_suites(category, preflight_run_method=self.preflight_talos)

    def _install_app(self):
        dirs = self.query_abs_dirs()
        cmd = ['python', self.config.get("install_app_path"), self.device_ip, os.path.join(dirs['abs_talosdata_dir'], self.filename_apk), self.app_name]
        self.run_command(cmd, dirs['abs_talosdata_dir'], halt_on_failure=True, fatal_exit_code=3)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(PandaTalosTest, self).query_abs_dirs()
        dirs = {}
        dirs['shutdown_dir'] = abs_dirs['abs_work_dir'].rsplit("/", 2)[0]
        dirs['abs_fennec_dir'] = os.path.join(
            dirs['shutdown_dir'], 'talos-data/fennec')
        dirs['abs_talosdata_dir'] = os.path.join(
            dirs['shutdown_dir'], 'talos-data')
        dirs['abs_symbols_dir'] = os.path.join(
            dirs['abs_talosdata_dir'], 'symbols')
        dirs['abs_talosdatatalos_dir'] = os.path.join(
            dirs['shutdown_dir'], 'talos-data/talos')
        dirs['abs_talosbuild_dir'] = os.path.join(
            dirs['shutdown_dir'], 'talos-data/build')
        dirs['abs_talos_dir'] = dirs['abs_talosdatatalos_dir']
        dirs['abs_preflight_talos_dir'] = dirs['abs_talosdatatalos_dir']
        dirs['abs_blob_upload_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'blobber_upload_dir')
        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def download_and_extract(self):
        dirs = self.query_abs_dirs()

        #mkdir talos in  /builds/panda-0052/test/../talos-data
        fennec_ids_url = self.installer_url.rsplit("/", 1)[0] + "/fennec_ids.txt"
        self.mkdir_p(dirs['abs_talosbuild_dir'])
        robocop_url = self.installer_url.rsplit("/", 1)[0] + "/robocop.apk"
        self.mkdir_p(dirs['abs_talosdatatalos_dir'])

        #download and extract apk to /builds/panda-0nnn/talos-data
        self.rmtree(dirs['abs_talosdata_dir'])
        self.mkdir_p(dirs['abs_talosdata_dir'])
        self.mkdir_p(dirs['abs_symbols_dir'])
        self.mkdir_p(dirs['abs_fennec_dir'])
        self._download_unzip(self.installer_url,
                             dirs['abs_fennec_dir'])
        #this is ugly but you can't specify a file in download_unzip to extract the file to, by default it's the abs_work_dir
        #should think of a better way
        self.download_file(self.installer_url,
                           parent_dir=dirs['abs_talosdata_dir'],
                           error_level=FATAL)

        #download and extract fennec_ids.txt to /builds/panda-0nnn/talos-data
        self.download_file(fennec_ids_url, file_name='fennec_ids.txt',
                           parent_dir=dirs['abs_talosdata_dir'],
                           error_level=FATAL)
        #download and extract robocop.apk to /builds/panda-0nnn/talos-data/build
        self.download_file(robocop_url, file_name='robocop.apk',
                           parent_dir=dirs['abs_talosbuild_dir'],
                           error_level=FATAL)
        self.symbols_url = self.query_symbols_url()

        self._download_unzip(self.symbols_url,
                             dirs['abs_symbols_dir'])

        self._download_unzip(self.config['retry_url'],
                             dirs['abs_talosdata_dir'])

        taloscode = self.config.get("talos_from_code_url")

        talos_from_code_url = (taloscode % (self.repo_path, self.revision))

        self.download_file(talos_from_code_url, file_name='talos_from_code.py',
                           parent_dir=dirs['abs_talosdata_dir'],
                           error_level=FATAL)

        talos_base_cmd = ['python']
        talos_code_path = (os.path.join(dirs['abs_talosdata_dir'], "talos_from_code.py"))
        talos_zip_path = (os.path.join(dirs['abs_talosdata_dir'], "talos.zip"))
        talos_base_cmd.append(talos_code_path)
        talos_base_cmd.append("--talos-json-url")
        talos_base_cmd.append(self.talos_json_url)
        env = self.query_env()
        self.run_command(talos_base_cmd, dirs['abs_talosdata_dir'], env=env, halt_on_failure=True, fatal_exit_code=3)
        unzip = self.query_exe("unzip")
        unzip_cmd = [unzip, '-q', '-o',  talos_zip_path]
        self.run_command(unzip_cmd, cwd=dirs['abs_talosdata_dir'], halt_on_failure=True, fatal_exit_code=3)

    def _query_abs_base_cmd(self, suite_category):
        dirs = self.query_abs_dirs()
        options = []
        run_file = self.config['run_file_names'][suite_category]
        base_cmd = ['python', '-u']
        base_cmd.append(os.path.join((dirs["abs_%s_dir" % suite_category]), run_file))
        self.device_ip = socket.gethostbyname(self.mozpool_device)
        hostnumber = int(self.mozpool_device.split('-')[1])
        http_port = '30%03i' % hostnumber
        ssl_port = '31%03i' % hostnumber
        #get filename from installer_url
        self.filename_apk = self.installer_url.split('/')[-1]
        #find appname from package-name.txt - assumes download-and-extract has completed successfully
        apk_dir = self.abs_dirs['abs_work_dir']
        self.apk_path = os.path.join(apk_dir, self.filename_apk)
        unzip = self.query_exe("unzip")
        package_path = os.path.join(dirs['abs_fennec_dir'], 'package-name.txt')
        unzip_cmd = [unzip, '-q', '-o',  self.apk_path]
        self.run_command(unzip_cmd, cwd=dirs['abs_fennec_dir'], halt_on_failure=True, fatal_exit_code=3)
        self.app_name = str(self.read_from_file(package_path, verbose=True)).rstrip()

        str_format_values = {
            'device_ip': self.device_ip,
            'hostname': self.mozpool_device,
            'http_port': http_port,
            'ssl_port':  ssl_port,
            'app_name':  self.app_name,
            'talos_branch':  self.talos_branch,
        }
        talos_json_config = self.query_talos_json_config()
        if talos_json_config.get('extra_options') and \
           talos_json_config['extra_options'].get('android'):
            for option in self.talos_json_config['extra_options']['android']:
                options.append(option % {
                    'apk_path': self.apk_path })

        if self.config['%s_options' % suite_category]:
            for option in self.config['%s_options' % suite_category]:
                options.append(option % str_format_values)
            abs_base_cmd = base_cmd + options
            return abs_base_cmd
        else:
            self.warning("Suite options for %s could not be determined."
                         "\nIf you meant to have options for this suite, "
                         "please make sure they are specified in your "
                         "config under %s_options" %
                         (suite_category, suite_category))

    ###### helper methods
    def _pre_config_lock(self, rw_config):
        super(PandaTalosTest, self)._pre_config_lock(rw_config)
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

    def _post_fatal(self, message=None, exit_code=None):
        """ After we call fatal(), run this method before exiting.
            """
        self.close_request()

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
    pandaTalosTest = PandaTalosTest()
    pandaTalosTest.run_and_exit()
