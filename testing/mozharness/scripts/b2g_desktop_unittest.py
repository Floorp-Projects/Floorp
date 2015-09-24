#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import copy
import os
import re
import sys

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.errors import BaseErrorList
from mozharness.base.log import ERROR, WARNING
from mozharness.base.script import PreScriptAction
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.blob_upload import BlobUploadMixin, blobupload_config_options
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options


class B2GDesktopTest(BlobUploadMixin, TestingMixin, MercurialScript):
    test_suites = ('mochitest',
                   'reftest',)
    config_options = [
        [["--type"],
        {"action": "store",
         "dest": "test_type",
         "default": "browser",
         "help": "The type of tests to run",
        }],
        [["--browser-arg"],
        {"action": "store",
         "dest": "browser_arg",
         "default": None,
         "help": "Optional command-line argument to pass to the browser",
        }],
        [["--test-manifest"],
        {"action": "store",
         "dest": "test_manifest",
         "default": None,
         "help": "Path to test manifest to run",
        }],
        [["--test-suite"],
        {"action": "store",
         "dest": "test_suite",
         "type": "choice",
         "choices": test_suites,
         "help": "Which test suite to run",
        }],
        [["--total-chunks"],
        {"action": "store",
         "dest": "total_chunks",
         "help": "Number of total chunks",
        }],
        [["--this-chunk"],
        {"action": "store",
         "dest": "this_chunk",
         "help": "Number of this chunk",
        }]] + copy.deepcopy(testing_config_options) \
            + copy.deepcopy(blobupload_config_options)

    error_list = [
        {'substr': 'FAILED (errors=', 'level': ERROR},
        {'substr': r'''Could not successfully complete transport of message to Gecko, socket closed''', 'level': ERROR},
        {'substr': r'''Could not communicate with Marionette server. Is the Gecko process still running''', 'level': ERROR},
        {'substr': r'''Connection to Marionette server is lost. Check gecko''', 'level': ERROR},
        {'substr': 'Timeout waiting for marionette on port', 'level': ERROR},
        {'regex': re.compile(r'''(Timeout|NoSuchAttribute|Javascript|NoSuchElement|XPathLookup|NoSuchWindow|StaleElement|ScriptTimeout|ElementNotVisible|NoSuchFrame|InvalidElementState|NoAlertPresent|InvalidCookieDomain|UnableToSetCookie|InvalidSelector|MoveTargetOutOfBounds)Exception'''), 'level': ERROR},
    ]

    def __init__(self, options=[], require_config_file=False):
        super(B2GDesktopTest, self).__init__(
            config_options=self.config_options + copy.deepcopy(options),
            all_actions=['clobber',
                         'read-buildbot-config',
                         'pull',
                         'download-and-extract',
                         'create-virtualenv',
                         'install',
                         'run-tests'],
            default_actions=['clobber',
                             'download-and-extract',
                             'create-virtualenv',
                             'install',
                             'run-tests'],
            require_config_file=require_config_file,
            config={
                'require_test_zip': True,
            }
        )

        # these are necessary since self.config is read only
        c = self.config
        self.installer_url = c.get('installer_url')
        self.installer_path = c.get('installer_path')
        self.test_url = c.get('test_url')
        self.test_packages_url = c.get('test_packages_url')
        self.test_manifest = c.get('test_manifest')

        suite = self.config['test_suite']
        if suite not in self.test_suites:
            self.fatal("Don't know how to run --test-suite '%s'!" % suite)

    # TODO detect required config items and fail if not set

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(B2GDesktopTest, self).query_abs_dirs()
        dirs = {}
        dirs['abs_blob_upload_dir'] = os.path.join(
                abs_dirs['abs_work_dir'], 'blobber_upload_dir')
        dirs['abs_tests_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'tests')
        for d in self.test_suites + ('config', 'certs'):
            dirs['abs_%s_dir' % d] = os.path.join(
                    dirs['abs_tests_dir'], d)

        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    @PreScriptAction('create-virtualenv')
    def _pre_create_virtualenv(self, action):
        dirs = self.query_abs_dirs()
        requirements = os.path.join(dirs['abs_config_dir'],
                                    'marionette_requirements.txt')
        self.register_virtualenv_module(requirements=[requirements],
                                        two_pass=True)

    def _query_abs_base_cmd(self, suite):
        dirs = self.query_abs_dirs()
        cmd = [self.query_python_path('python')]
        cmd.append(self.config["run_file_names"][suite])

        raw_log_file = os.path.join(dirs['abs_blob_upload_dir'],
                                    '%s_raw.log' % suite)
        error_summary_file = os.path.join(dirs['abs_blob_upload_dir'],
                                          '%s_errorsummary.log' % suite)
        str_format_values = {
            'application': self.binary_path,
            'test_manifest': self.test_manifest,
            'symbols_path': self.symbols_path,
            'gaia_profile': self.gaia_profile,
            'utility_path': os.path.join(dirs['abs_tests_dir'], 'bin'),
            'total_chunks': self.config.get('total_chunks'),
            'this_chunk': self.config.get('this_chunk'),
            'cert_path': dirs['abs_certs_dir'],
            'browser_arg': self.config.get('browser_arg'),
            'raw_log_file': raw_log_file,
            'error_summary_file': error_summary_file,
        }

        if suite not in self.config["suite_definitions"]:
            self.fatal("'%s' not defined in the config!" % suite)

        options = self.config["suite_definitions"][suite]["options"]
        if options:
            for option in options:
                option = option % str_format_values
                if not option.endswith('None'):
                    cmd.append(option)

        tests = self.config["suite_definitions"][suite].get("tests")
        if tests:
            cmd.extend(tests)

        return cmd

    def download_and_extract(self):
        suite_category = self.config['test_suite']
        super(B2GDesktopTest, self).download_and_extract(suite_categories=[suite_category])

    def preflight_run_tests(self):
        super(B2GDesktopTest, self).preflight_run_tests()
        suite = self.config['test_suite']
        # set default test manifest by suite if none specified
        if not self.test_manifest:
            if suite == 'reftest':
                self.test_manifest = os.path.join('tests', 'layout',
                                                  'reftests', 'reftest.list')

        # set the gaia_profile
        self.gaia_profile = os.path.join(os.path.dirname(self.binary_path),
                                         'gaia', 'profile')

    def run_tests(self):
        """
        Run the tests
        """
        dirs = self.query_abs_dirs()

        error_list = self.error_list
        error_list.extend(BaseErrorList)

        suite = self.config['test_suite']
        if suite not in self.test_suites:
            self.fatal("Don't know how to run --test-suite '%s'!" % suite)

        cmd = self._query_abs_base_cmd(suite)
        cmd = self.append_harness_extra_args(cmd)

        cwd = dirs['abs_%s_dir' % suite]

        # TODO we probably have to move some of the code in
        # scripts/desktop_unittest.py and scripts/marionette.py to
        # mozharness.mozilla.testing.unittest so we can share it.
        # In the short term, I'm ok with some duplication of code if it
        # expedites things; please file bugs to merge if that happens.

        suite_name = [x for x in self.test_suites if x in self.config['test_suite']][0]
        if self.config.get('this_chunk'):
            suite = '%s-%s' % (suite_name, self.config['this_chunk'])
        else:
            suite = suite_name

        # bug 773703
        success_codes = None
        if suite_name == 'xpcshell':
            success_codes = [0, 1]

        env = {}
        if self.query_minidump_stackwalk():
            env['MINIDUMP_STACKWALK'] = self.minidump_stackwalk_path
        env['MOZ_UPLOAD_DIR'] = dirs['abs_blob_upload_dir']
        if not os.path.isdir(env['MOZ_UPLOAD_DIR']):
            self.mkdir_p(env['MOZ_UPLOAD_DIR'])
        env = self.query_env(partial_env=env)

        parser = self.get_test_output_parser(suite_name,
                                             config=self.config,
                                             log_obj=self.log_obj,
                                             error_list=error_list)

        return_code = self.run_command(cmd, cwd=cwd, env=env,
                                       output_timeout=1000,
                                       output_parser=parser,
                                       success_codes=success_codes)

        tbpl_status, log_level = parser.evaluate_parser(return_code,
                                                        success_codes=success_codes)
        parser.append_tinderboxprint_line(suite_name)

        self.buildbot_status(tbpl_status, level=log_level)
        self.log("The %s suite: %s ran with return status: %s" %
                 (suite_name, suite, tbpl_status), level=log_level)

if __name__ == '__main__':
    desktopTest = B2GDesktopTest()
    desktopTest.run_and_exit()
