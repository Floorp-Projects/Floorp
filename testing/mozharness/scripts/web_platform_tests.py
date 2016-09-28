#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
import os
import sys
import copy

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.script import PreScriptAction
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.blob_upload import BlobUploadMixin, blobupload_config_options
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options

from mozharness.mozilla.structuredlog import StructuredOutputParser
from mozharness.base.log import INFO

class WebPlatformTest(TestingMixin, MercurialScript, BlobUploadMixin):
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
            "help": "Permits a software GL implementation (such as LLVMPipe) to use the GL compositor."}]
    ] + copy.deepcopy(testing_config_options) + \
        copy.deepcopy(blobupload_config_options)

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

    def _query_cmd(self):
        if not self.binary_path:
            self.fatal("Binary path could not be determined")
            #And exit

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
                "--log-errorsummary=%s" % os.path.join(dirs["abs_blob_upload_dir"],
                                                       "wpt_errorsummary.log"),
                "--binary=%s" % self.binary_path,
                "--symbols-path=%s" % self.query_symbols_url(),
                "--stackwalk-binary=%s" % self.query_minidump_stackwalk()]

        for test_type in c.get("test_type", []):
            cmd.append("--test-type=%s" % test_type)

        if not c["e10s"]:
            cmd.append("--disable-e10s")

        for opt in ["total_chunks", "this_chunk"]:
            val = c.get(opt)
            if val:
                cmd.append("--%s=%s" % (opt.replace("_", "-"), val))

        options = list(c.get("options", []))

        str_format_values = {
            'binary_path': self.binary_path,
            'test_path': dirs["abs_wpttest_dir"],
            'test_install_path': dirs["abs_test_install_dir"],
            'abs_app_dir': abs_app_dir,
            'abs_work_dir': dirs["abs_work_dir"]
            }

        try_options, try_tests = self.try_args("web-platform-tests")

        cmd.extend(self.query_options(options,
                                      try_options,
                                      str_format_values=str_format_values))
        cmd.extend(self.query_tests_args(try_tests,
                                         str_format_values=str_format_values))

        return cmd

    def download_and_extract(self):
        super(WebPlatformTest, self).download_and_extract(
            extract_dirs=["bin/*",
                          "config/*",
                          "mozbase/*",
                          "marionette/*",
                          "tools/wptserve/*",
                          "web-platform/*"],
            suite_categories=["web-platform"])

    def run_tests(self):
        dirs = self.query_abs_dirs()
        cmd = self._query_cmd()

        parser = StructuredOutputParser(config=self.config,
                                        log_obj=self.log_obj,
                                        log_compact=True)

        env = {'MINIDUMP_SAVE_PATH': dirs['abs_blob_upload_dir']}

        if self.config['allow_software_gl_layers']:
            env['MOZ_LAYERS_ALLOW_SOFTWARE_GL'] = '1'

        env = self.query_env(partial_env=env, log_level=INFO)

        return_code = self.run_command(cmd,
                                       cwd=dirs['abs_work_dir'],
                                       output_timeout=1000,
                                       output_parser=parser,
                                       env=env)

        tbpl_status, log_level = parser.evaluate_parser(return_code)

        self.buildbot_status(tbpl_status, level=log_level)


# main {{{1
if __name__ == '__main__':
    web_platform_tests = WebPlatformTest()
    web_platform_tests.run_and_exit()
