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

from mozharness.base.script import PreScriptAction
from mozharness.base.log import INFO, ERROR
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.blob_upload import BlobUploadMixin, blobupload_config_options
from mozharness.mozilla.tooltool import TooltoolMixin
from mozharness.mozilla.structuredlog import StructuredOutputParser


class AWSY(TestingMixin, MercurialScript, BlobUploadMixin, TooltoolMixin):
    config_options = [
        [["--e10s"],
        {"action": "store_true",
         "dest": "e10s",
         "default": False,
         "help": "Run tests with multiple processes. (Desktop builds only)",
         }]
    ] + testing_config_options + copy.deepcopy(blobupload_config_options)

    error_list = [
        {'regex': re.compile(r'''(TEST-UNEXPECTED|PROCESS-CRASH)'''), 'level': ERROR},
    ]

    def __init__(self, **kwargs):

        kwargs.setdefault('config_options', self.config_options)
        kwargs.setdefault('all_actions', ['clobber',
                                          'download-and-extract',
                                          'populate-webroot',
                                          'create-virtualenv',
                                          'install',
                                          'run-tests',
                                          ])
        kwargs.setdefault('default_actions', ['clobber',
                                              'download-and-extract',
                                              'populate-webroot',
                                              'create-virtualenv',
                                              'install',
                                              'run-tests',
                                              ])
        kwargs.setdefault('config', {})
        super(AWSY, self).__init__(**kwargs)
        self.installer_url = self.config.get("installer_url")
        self.tests = None

        abs_work_dir = self.query_abs_dirs()['abs_work_dir']
        self.testdir = os.path.join(abs_work_dir, 'tests')
        self.awsy_path = os.path.join(self.testdir, 'awsy')
        self.awsy_libdir = os.path.join(self.awsy_path, 'awsy')
        self.webroot_dir = os.path.join(self.testdir, 'html')
        self.results_dir = os.path.join(self.testdir, 'results')
        self.binary_path = self.config.get('binary_path')

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(AWSY, self).query_abs_dirs()

        dirs = {}
        dirs['abs_blob_upload_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'blobber_upload_dir')
        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def download_and_extract(self, extract_dirs=None, suite_categories=None):
        ret = super(AWSY, self).download_and_extract(
            suite_categories=['common', 'awsy']
        )
        return ret

    @PreScriptAction('create-virtualenv')
    def _pre_create_virtualenv(self, action):
        requirements_files = [os.path.join(self.testdir,
                                           'config',
                                           'marionette_requirements.txt')]

        for requirements_file in requirements_files:
            self.register_virtualenv_module(requirements=[requirements_file],
                                            two_pass=True)

        self.register_virtualenv_module('awsy', self.awsy_path)


    def populate_webroot(self):
        """Populate the production test slaves' webroots"""
        self.info("Downloading pageset with tooltool...")
        manifest_file = os.path.join(self.awsy_path, 'tp5n-pageset.manifest')
        page_load_test_dir = os.path.join(self.webroot_dir, 'page_load_test')
        if not os.path.isdir(page_load_test_dir):
            self.mkdir_p(page_load_test_dir)
        self.tooltool_fetch(
            manifest_file,
            output_dir=page_load_test_dir,
            cache=self.config.get('tooltool_cache')
        )
        archive = os.path.join(page_load_test_dir, 'tp5n.zip')
        unzip = self.query_exe('unzip')
        unzip_cmd = [unzip, '-q', '-o', archive, '-d', page_load_test_dir]
        self.run_command(unzip_cmd, halt_on_failure=True)
        self.run_command("ls %s" % page_load_test_dir)


    def run_tests(self, args=None, **kw):
        '''
        AWSY test should be implemented here
        '''
        dirs = self.abs_dirs
        env = {}
        error_summary_file = os.path.join(dirs['abs_blob_upload_dir'],
                                          'marionette_errorsummary.log')

        runtime_testvars = {'webRootDir': self.webroot_dir,
                            'resultsDir': self.results_dir}
        runtime_testvars_path = os.path.join(self.awsy_path, 'runtime-testvars.json')
        runtime_testvars_file = open(runtime_testvars_path, 'wb')
        runtime_testvars_file.write(json.dumps(runtime_testvars, indent=2))
        runtime_testvars_file.close()

        cmd = ['marionette']
        cmd.append("--preferences=%s" % os.path.join(self.awsy_path, "conf", "prefs.json"))
        cmd.append("--testvars=%s" % os.path.join(self.awsy_path, "conf", "testvars.json"))
        cmd.append("--testvars=%s" % runtime_testvars_path)
        cmd.append("--log-raw=-")
        cmd.append("--log-errorsummary=%s" % error_summary_file)
        cmd.append("--binary=%s" % self.binary_path)
        cmd.append("--profile=%s" % (os.path.join(dirs['abs_work_dir'], 'profile')))
        if not self.config['e10s']:
            cmd.append('--disable-e10s')
        cmd.append('--gecko-log=%s' % os.path.join(dirs["abs_blob_upload_dir"],
                                                   'gecko.log'))
        # TestingMixin._download_and_extract_symbols() should set
        # self.symbols_path
        cmd.append('--symbols-path=%s' % self.symbols_path)

        test_file = os.path.join(self.awsy_libdir, 'test_memory_usage.py')
        cmd.append(test_file)

        env['MOZ_UPLOAD_DIR'] = dirs['abs_blob_upload_dir']
        if not os.path.isdir(env['MOZ_UPLOAD_DIR']):
            self.mkdir_p(env['MOZ_UPLOAD_DIR'])
        if self.query_minidump_stackwalk():
            env['MINIDUMP_STACKWALK'] = self.minidump_stackwalk_path
        env['MINIDUMP_SAVE_PATH'] = dirs['abs_blob_upload_dir']
        env['RUST_BACKTRACE'] = '1'
        env = self.query_env(partial_env=env)
        parser = StructuredOutputParser(config=self.config,
                                        log_obj=self.log_obj,
                                        error_list=self.error_list,
                                        strict=False)
        return_code = self.run_command(command=cmd,
                                       cwd=self.awsy_path,
                                       output_timeout=self.config.get("cmd_timeout"),
                                       env=env,
                                       output_parser=parser)

        level = INFO
        tbpl_status, log_level = parser.evaluate_parser(
            return_code=return_code)

        self.log("AWSY exited with return code %s: %s" % (return_code, tbpl_status),
                 level=level)
        self.buildbot_status(tbpl_status)


if __name__ == '__main__':
    awsy_test = AWSY()
    awsy_test.run_and_exit()
