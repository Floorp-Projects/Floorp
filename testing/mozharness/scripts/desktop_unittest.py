#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""desktop_unittest.py
The goal of this is to extract desktop unittesting from buildbot's factory.py

author: Jordan Lund
"""

import os
import re
import sys
import copy
import shutil
import glob

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.errors import BaseErrorList
from mozharness.base.log import INFO, ERROR, WARNING
from mozharness.base.script import PreScriptAction
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.blob_upload import BlobUploadMixin, blobupload_config_options
from mozharness.mozilla.mozbase import MozbaseMixin
from mozharness.mozilla.testing.codecoverage import (
    CodeCoverageMixin,
    code_coverage_config_options
)
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.mozilla.buildbot import TBPL_WARNING

SUITE_CATEGORIES = ['cppunittest', 'jittest', 'mochitest', 'reftest', 'xpcshell', 'mozbase', 'mozmill', 'webapprt']


# DesktopUnittest {{{1
class DesktopUnittest(TestingMixin, MercurialScript, BlobUploadMixin, MozbaseMixin, CodeCoverageMixin):
    config_options = [
        [['--mochitest-suite', ], {
            "action": "extend",
            "dest": "specified_mochitest_suites",
            "type": "string",
            "help": "Specify which mochi suite to run. "
                    "Suites are defined in the config file.\n"
                    "Examples: 'all', 'plain1', 'plain5', 'chrome', or 'a11y'"}
         ],
        [['--webapprt-suite', ], {
            "action": "extend",
            "dest": "specified_webapprt_suites",
            "type": "string",
            "help": "Specify which webapprt suite to run. "
                    "Suites are defined in the config file.\n"
                    "Examples: 'content', 'chrome'"}
         ],
        [['--reftest-suite', ], {
            "action": "extend",
            "dest": "specified_reftest_suites",
            "type": "string",
            "help": "Specify which reftest suite to run. "
                    "Suites are defined in the config file.\n"
                    "Examples: 'all', 'crashplan', or 'jsreftest'"}
         ],
        [['--xpcshell-suite', ], {
            "action": "extend",
            "dest": "specified_xpcshell_suites",
            "type": "string",
            "help": "Specify which xpcshell suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'xpcshell'"}
         ],
        [['--cppunittest-suite', ], {
            "action": "extend",
            "dest": "specified_cppunittest_suites",
            "type": "string",
            "help": "Specify which cpp unittest suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'cppunittest'"}
         ],
        [['--jittest-suite', ], {
            "action": "extend",
            "dest": "specified_jittest_suites",
            "type": "string",
            "help": "Specify which jit-test suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'jittest'"}
         ],
        [['--mozbase-suite', ], {
            "action": "extend",
            "dest": "specified_mozbase_suites",
            "type": "string",
            "help": "Specify which mozbase suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'mozbase'"}
         ],
        [['--mozmill-suite', ], {
            "action": "extend",
            "dest": "specified_mozmill_suites",
            "type": "string",
            "help": "Specify which mozmill suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'mozmill'"}
         ],
        [['--run-all-suites', ], {
            "action": "store_true",
            "dest": "run_all_suites",
            "default": False,
            "help": "This will run all suites that are specified "
                    "in the config file. You do not need to specify "
                    "any other suites.\nBeware, this may take a while ;)"}
         ],
        [['--e10s', ], {
            "action": "store_true",
            "dest": "e10s",
            "default": False,
            "help": "Run tests with multiple processes."}
         ],
        [['--strict-content-sandbox', ], {
            "action": "store_true",
            "dest": "strict_content_sandbox",
            "default": False,
            "help": "Run tests with a more strict content sandbox (Windows only)."}
         ],
        [['--no-random', ], {
            "action": "store_true",
            "dest": "no_random",
            "default": False,
            "help": "Run tests with no random intermittents and bisect in case of real failure."}
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
    ] + copy.deepcopy(testing_config_options) + \
        copy.deepcopy(blobupload_config_options) + \
        copy.deepcopy(code_coverage_config_options)

    def __init__(self, require_config_file=True):
        # abs_dirs defined already in BaseScript but is here to make pylint happy
        self.abs_dirs = None
        super(DesktopUnittest, self).__init__(
            config_options=self.config_options,
            all_actions=[
                'clobber',
                'read-buildbot-config',
                'download-and-extract',
                'create-virtualenv',
                'install',
                'run-tests',
            ],
            require_config_file=require_config_file,
            config={'require_test_zip': True})

        c = self.config
        self.global_test_options = []
        self.installer_url = c.get('installer_url')
        self.test_url = c.get('test_url')
        self.test_packages_url = c.get('test_packages_url')
        self.symbols_url = c.get('symbols_url')
        # this is so mozinstall in install() doesn't bug out if we don't run
        # the download_and_extract action
        self.installer_path = c.get('installer_path')
        self.binary_path = c.get('binary_path')
        self.abs_app_dir = None
        self.abs_res_dir = None

    # helper methods {{{2
    def _pre_config_lock(self, rw_config):
        super(DesktopUnittest, self)._pre_config_lock(rw_config)
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

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(DesktopUnittest, self).query_abs_dirs()

        c = self.config
        dirs = {}
        dirs['abs_app_install_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'application')
        dirs['abs_test_install_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'tests')
        dirs['abs_test_extensions_dir'] = os.path.join(dirs['abs_test_install_dir'], 'extensions')
        dirs['abs_test_bin_dir'] = os.path.join(dirs['abs_test_install_dir'], 'bin')
        dirs['abs_test_bin_plugins_dir'] = os.path.join(dirs['abs_test_bin_dir'],
                                                        'plugins')
        dirs['abs_test_bin_components_dir'] = os.path.join(dirs['abs_test_bin_dir'],
                                                           'components')
        dirs['abs_mochitest_dir'] = os.path.join(dirs['abs_test_install_dir'], "mochitest")
        dirs['abs_webapprt_dir'] = os.path.join(dirs['abs_test_install_dir'], "mochitest")
        dirs['abs_reftest_dir'] = os.path.join(dirs['abs_test_install_dir'], "reftest")
        dirs['abs_xpcshell_dir'] = os.path.join(dirs['abs_test_install_dir'], "xpcshell")
        dirs['abs_cppunittest_dir'] = os.path.join(dirs['abs_test_install_dir'], "cppunittest")
        dirs['abs_blob_upload_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'blobber_upload_dir')
        dirs['abs_jittest_dir'] = os.path.join(dirs['abs_test_install_dir'], "jit-test", "jit-test")
        dirs['abs_mozbase_dir'] = os.path.join(dirs['abs_test_install_dir'], "mozbase")
        dirs['abs_mozmill_dir'] = os.path.join(dirs['abs_test_install_dir'], "mozmill")

        if os.path.isabs(c['virtualenv_path']):
            dirs['abs_virtualenv_dir'] = c['virtualenv_path']
        else:
            dirs['abs_virtualenv_dir'] = os.path.join(abs_dirs['abs_work_dir'],
                                                      c['virtualenv_path'])
        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs

        return self.abs_dirs

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

    def query_abs_res_dir(self):
        """The directory containing resources like plugins and extensions. On
        OSX this is Contents/Resources, on all other platforms its the same as
        the app dir.

        As with the app dir, we can't set this in advance, because OSX install
        directories change depending on branding and opt/debug.
        """
        if self.abs_res_dir:
            return self.abs_res_dir

        abs_app_dir = self.query_abs_app_dir()
        if self._is_darwin():
            res_subdir = self.config.get("mac_res_subdir", "Resources")
            self.abs_res_dir = os.path.join(os.path.dirname(abs_app_dir), res_subdir)
        else:
            self.abs_res_dir = abs_app_dir
        return self.abs_res_dir

    @PreScriptAction('create-virtualenv')
    def _pre_create_virtualenv(self, action):
        dirs = self.query_abs_dirs()
        self.register_virtualenv_module(name='mock')
        self.register_virtualenv_module(name='simplejson')

        requirements = os.path.join(dirs['abs_test_install_dir'],
                                    'config',
                                    'mozbase_requirements.txt')
        if os.path.isfile(requirements):
            self.register_virtualenv_module(requirements=[requirements],
                                            two_pass=True)
            return

    def _query_symbols_url(self):
        """query the full symbols URL based upon binary URL"""
        # may break with name convention changes but is one less 'input' for script
        if self.symbols_url:
            return self.symbols_url

        symbols_url = None
        self.info("finding symbols_url based upon self.installer_url")
        if self.installer_url:
            for ext in ['.zip', '.dmg', '.tar.bz2']:
                if ext in self.installer_url:
                    symbols_url = self.installer_url.replace(
                        ext, '.crashreporter-symbols.zip')
            if not symbols_url:
                self.fatal("self.installer_url was found but symbols_url could \
                        not be determined")
        else:
            self.fatal("self.installer_url was not found in self.config")
        self.info("setting symbols_url as %s" % (symbols_url))
        self.symbols_url = symbols_url
        return self.symbols_url

    def get_webapprt_path(self, res_dir, mochitest_dir):
        """Get the path to the webapp runtime binary.
        On Mac, we copy the stub from the resources dir to the test app bundle,
        since we have to run it from the executable directory of a bundle
        in order for its windows to appear.  Ideally, the build system would do
        this for us at build time, and we should find a way for it to do that.
        """
        exe_suffix = self.config.get('exe_suffix', '')
        app_name = 'webapprt-stub' + exe_suffix
        app_path = os.path.join(res_dir, app_name)
        if self._is_darwin():
            mac_dir_name = os.path.join(
                mochitest_dir,
                'webapprtChrome',
                'webapprt',
                'test',
                'chrome',
                'TestApp.app',
                'Contents',
                'MacOS')
            mac_app_name = 'webapprt' + exe_suffix
            mac_app_path = os.path.join(mac_dir_name, mac_app_name)
            self.copyfile(app_path, mac_app_path, copystat=True)
            return mac_app_path
        return app_path

    def _query_abs_base_cmd(self, suite_category, suite):
        if self.binary_path:
            c = self.config
            dirs = self.query_abs_dirs()
            run_file = c['run_file_names'][suite_category]
            base_cmd = [self.query_python_path('python'), '-u']
            base_cmd.append(os.path.join(dirs["abs_%s_dir" % suite_category], run_file))
            abs_app_dir = self.query_abs_app_dir()
            abs_res_dir = self.query_abs_res_dir()

            raw_log_file = os.path.join(dirs['abs_blob_upload_dir'],
                                        '%s_raw.log' % suite)

            error_summary_file = os.path.join(dirs['abs_blob_upload_dir'],
                                              '%s_errorsummary.log' % suite)
            str_format_values = {
                'binary_path': self.binary_path,
                'symbols_path': self._query_symbols_url(),
                'abs_app_dir': abs_app_dir,
                'abs_res_dir': abs_res_dir,
                'raw_log_file': raw_log_file,
                'error_summary_file': error_summary_file,
            }
            # TestingMixin._download_and_extract_symbols() will set
            # self.symbols_path when downloading/extracting.
            if self.symbols_path:
                str_format_values['symbols_path'] = self.symbols_path

            if suite_category == 'webapprt':
                str_format_values['app_path'] = self.get_webapprt_path(abs_res_dir, dirs['abs_mochitest_dir'])

            if c['e10s']:
                base_cmd.append('--e10s')

            if c.get('strict_content_sandbox'):
                if suite_category == "mochitest":
                    base_cmd.append('--strict-content-sandbox')
                else:
                    self.fatal("--strict-content-sandbox only works with mochitest suites.")

            if c.get('total_chunks') and c.get('this_chunk'):
                base_cmd.extend(['--total-chunks', c['total_chunks'],
                                 '--this-chunk', c['this_chunk']])

            if c['no_random']:
                if suite_category == "mochitest":
                    base_cmd.append('--bisect-chunk=default')
                else:
                    self.warning("--no-random does not currently work with suites other than mochitest.")

            # set pluginsPath
            abs_res_plugins_dir = os.path.join(abs_res_dir, 'plugins')
            str_format_values['test_plugin_path'] = abs_res_plugins_dir

            if suite_category not in c["suite_definitions"]:
                self.fatal("'%s' not defined in the config!")

            options = c["suite_definitions"][suite_category]["options"]
            if options:
                for option in options:
                    option = option % str_format_values
                    if not option.endswith('None'):
                        base_cmd.append(option)
                return base_cmd
            else:
                self.warning("Suite options for %s could not be determined."
                             "\nIf you meant to have options for this suite, "
                             "please make sure they are specified in your "
                             "config under %s_options" %
                             (suite_category, suite_category))
                return base_cmd
        else:
            self.fatal("'binary_path' could not be determined.\n This should "
                       "be like '/path/build/application/firefox/firefox'"
                       "\nIf you are running this script without the 'install' "
                       "action (where binary_path is set), please ensure you are"
                       " either:\n(1) specifying it in the config file under "
                       "binary_path\n(2) specifying it on command line with the"
                       " '--binary-path' flag")

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

    # Actions {{{2

    # clobber defined in BaseScript, deletes mozharness/build if exists
    # read_buildbot_config is in BuildbotMixin.
    # postflight_read_buildbot_config is in TestingMixin.
    # preflight_download_and_extract is in TestingMixin.
    # create_virtualenv is in VirtualenvMixin.
    # preflight_install is in TestingMixin.
    # install is in TestingMixin.
    # upload_blobber_files is in BlobUploadMixin

    def download_and_extract(self):
        """
        download and extract test zip / download installer
        optimizes which subfolders to extract from tests zip
        """
        c = self.config

        target_unzip_dirs = None
        if c['specific_tests_zip_dirs']:
            target_unzip_dirs = list(c['minimum_tests_zip_dirs'])
            for category in c['specific_tests_zip_dirs'].keys():
                if c['run_all_suites'] or self._query_specified_suites(category) \
                        or 'run-tests' not in self.actions:
                    target_unzip_dirs.extend(c['specific_tests_zip_dirs'][category])

        if c.get('run_all_suites'):
            target_categories = SUITE_CATEGORIES
        else:
            target_categories = [cat for cat in SUITE_CATEGORIES
                                 if self._query_specified_suites(cat) is not None]
        super(DesktopUnittest, self).download_and_extract(target_unzip_dirs=target_unzip_dirs,
                                                          suite_categories=target_categories)

    # pull defined in VCSScript.
    # preflight_run_tests defined in TestingMixin.

    def run_tests(self):
        self._run_category_suites('mochitest')
        self._run_category_suites('reftest')
        self._run_category_suites('webapprt')
        self._run_category_suites('xpcshell',
                                  preflight_run_method=self.preflight_xpcshell)
        self._run_category_suites('cppunittest',
                                  preflight_run_method=self.preflight_cppunittest)
        self._run_category_suites('jittest')
        self._run_category_suites('mozbase')
        self._run_category_suites('mozmill',
                                  preflight_run_method=self.preflight_mozmill)

    def preflight_xpcshell(self, suites):
        c = self.config
        dirs = self.query_abs_dirs()
        abs_app_dir = self.query_abs_app_dir()

        # For mac these directories are in Contents/Resources, on other
        # platforms abs_res_dir will point to abs_app_dir.
        abs_res_dir = self.query_abs_res_dir()
        abs_res_components_dir = os.path.join(abs_res_dir, 'components')
        abs_res_plugins_dir = os.path.join(abs_res_dir, 'plugins')
        abs_res_extensions_dir = os.path.join(abs_res_dir, 'extensions')

        if suites:  # there are xpcshell suites to run
            self.mkdir_p(abs_res_plugins_dir)
            self.info('copying %s to %s' % (os.path.join(dirs['abs_test_bin_dir'],
                      c['xpcshell_name']), os.path.join(abs_app_dir,
                                                        c['xpcshell_name'])))
            shutil.copy2(os.path.join(dirs['abs_test_bin_dir'], c['xpcshell_name']),
                         os.path.join(abs_app_dir, c['xpcshell_name']))
            self.copytree(dirs['abs_test_bin_components_dir'],
                          abs_res_components_dir,
                          overwrite='overwrite_if_exists')
            self.copytree(dirs['abs_test_bin_plugins_dir'],
                          abs_res_plugins_dir,
                          overwrite='overwrite_if_exists')
            if os.path.isdir(dirs['abs_test_extensions_dir']):
                self.mkdir_p(abs_res_extensions_dir)
                self.copytree(dirs['abs_test_extensions_dir'],
                              abs_res_extensions_dir,
                              overwrite='overwrite_if_exists')

    def preflight_cppunittest(self, suites):
        abs_res_dir = self.query_abs_res_dir()
        dirs = self.query_abs_dirs()
        abs_cppunittest_dir = dirs['abs_cppunittest_dir']

        # move manifest and js fils to resources dir, where tests expect them
        files = glob.glob(os.path.join(abs_cppunittest_dir, '*.js'))
        files.extend(glob.glob(os.path.join(abs_cppunittest_dir, '*.manifest')))
        for f in files:
            self.move(f, abs_res_dir)

    def preflight_mozmill(self, suites):
        c = self.config
        dirs = self.query_abs_dirs()
        abs_app_dir = self.query_abs_app_dir()
        abs_app_plugins_dir = os.path.join(abs_app_dir, 'plugins')
        abs_app_extensions_dir = os.path.join(abs_app_dir, 'extensions')
        if suites:  # there are mozmill suites to run
            self.mkdir_p(abs_app_plugins_dir)
            self.copytree(dirs['abs_test_bin_plugins_dir'],
                          abs_app_plugins_dir,
                          overwrite='overwrite_if_exists')
            if os.path.isdir(dirs['abs_test_extensions_dir']):
                self.copytree(dirs['abs_test_extensions_dir'],
                              abs_app_extensions_dir,
                              overwrite='overwrite_if_exists')
            modules = ['jsbridge', 'mozmill']
            for module in modules:
                self.install_module(module=os.path.join(dirs['abs_mozmill_dir'],
                                                        'resources',
                                                        module))

    def _run_category_suites(self, suite_category, preflight_run_method=None):
        """run suite(s) to a specific category"""
        c = self.config
        dirs = self.query_abs_dirs()
        suites = self._query_specified_suites(suite_category)
        abs_app_dir = self.query_abs_app_dir()
        abs_res_dir = self.query_abs_res_dir()

        if preflight_run_method:
            preflight_run_method(suites)
        if suites:
            self.info('#### Running %s suites' % suite_category)
            for suite in suites:
                abs_base_cmd = self._query_abs_base_cmd(suite_category, suite)
                cmd = abs_base_cmd[:]
                replace_dict = {
                    'abs_app_dir': abs_app_dir,

                    # Mac specific, but points to abs_app_dir on other
                    # platforms.
                    'abs_res_dir': abs_res_dir,
                }
                options_list = []
                env = {}
                if isinstance(suites[suite], dict):
                    options_list = suites[suite]['options']
                    env = copy.deepcopy(suites[suite]['env'])
                else:
                    options_list = suites[suite]

                for arg in options_list:
                    cmd.append(arg % replace_dict)

                cmd = self.append_harness_extra_args(cmd)

                suite_name = suite_category + '-' + suite
                tbpl_status, log_level = None, None
                error_list = BaseErrorList + [{
                    'regex': re.compile(r'''PROCESS-CRASH.*application crashed'''),
                    'level': ERROR,
                }]
                parser = self.get_test_output_parser(suite_category,
                                                     config=self.config,
                                                     error_list=error_list,
                                                     log_obj=self.log_obj)

                if self.query_minidump_stackwalk():
                    env['MINIDUMP_STACKWALK'] = self.minidump_stackwalk_path
                env['MOZ_UPLOAD_DIR'] = self.query_abs_dirs()['abs_blob_upload_dir']
                env['MINIDUMP_SAVE_PATH'] = self.query_abs_dirs()['abs_blob_upload_dir']
                if not os.path.isdir(env['MOZ_UPLOAD_DIR']):
                    self.mkdir_p(env['MOZ_UPLOAD_DIR'])
                env = self.query_env(partial_env=env, log_level=INFO)
                cmd_timeout = 2500 if suite_category == 'cppunittest' else 1000
                return_code = self.run_command(cmd, cwd=dirs['abs_work_dir'],
                                               output_timeout=cmd_timeout,
                                               output_parser=parser,
                                               env=env)

                # mochitest, reftest, and xpcshell suites do not return
                # appropriate return codes. Therefore, we must parse the output
                # to determine what the tbpl_status and worst_log_level must
                # be. We do this by:
                # 1) checking to see if our mozharness script ran into any
                #    errors itself with 'num_errors' <- OutputParser
                # 2) if num_errors is 0 then we look in the subclassed 'parser'
                #    findings for harness/suite errors <- DesktopUnittestOutputParser
                # 3) checking to see if the return code is in success_codes

                success_codes = None
                if self._is_windows():
                    # bug 1120644
                    success_codes = [0, 1]

                tbpl_status, log_level = parser.evaluate_parser(return_code,
                                                                success_codes=success_codes)
                parser.append_tinderboxprint_line(suite_name)

                self.buildbot_status(tbpl_status, level=log_level)
                self.log("The %s suite: %s ran with return status: %s" %
                         (suite_category, suite, tbpl_status), level=log_level)
        else:
            self.debug('There were no suites to run for %s' % suite_category)


# main {{{1
if __name__ == '__main__':
    desktop_unittest = DesktopUnittest()
    desktop_unittest.run_and_exit()
