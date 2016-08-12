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
import imp

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.errors import BaseErrorList
from mozharness.base.log import INFO, ERROR
from mozharness.base.script import PreScriptAction
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.blob_upload import BlobUploadMixin, blobupload_config_options
from mozharness.mozilla.mozbase import MozbaseMixin
from mozharness.mozilla.testing.codecoverage import (
    CodeCoverageMixin,
    code_coverage_config_options
)
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options

SUITE_CATEGORIES = ['gtest', 'cppunittest', 'jittest', 'mochitest', 'reftest', 'xpcshell', 'mozbase', 'mozmill']
SUITE_DEFAULT_E10S = ['mochitest', 'reftest']

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
        [['--gtest-suite', ], {
            "action": "extend",
            "dest": "specified_gtest_suites",
            "type": "string",
            "help": "Specify which gtest suite to run. "
                    "Suites are defined in the config file\n."
                    "Examples: 'gtest'"}
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
                'stage-files',
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

        # Construct an identifier to be used to identify Perfherder data
        # for resource monitoring recording. This attempts to uniquely
        # identify this test invocation configuration.
        perfherder_parts = []
        perfherder_options = []
        suites = (
            ('specified_mochitest_suites', 'mochitest'),
            ('specified_reftest_suites', 'reftest'),
            ('specified_xpcshell_suites', 'xpcshell'),
            ('specified_cppunittest_suites', 'cppunit'),
            ('specified_gtest_suites', 'gtest'),
            ('specified_jittest_suites', 'jittest'),
            ('specified_mozbase_suites', 'mozbase'),
            ('specified_mozmill_suites', 'mozmill'),
        )
        for s, prefix in suites:
            if s in c:
                perfherder_parts.append(prefix)
                perfherder_parts.extend(c[s])

        if 'this_chunk' in c:
            perfherder_parts.append(c['this_chunk'])

        if c['e10s']:
            perfherder_options.append('e10s')

        self.resource_monitor_perfherder_id = ('.'.join(perfherder_parts),
                                               perfherder_options)

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
        dirs['abs_reftest_dir'] = os.path.join(dirs['abs_test_install_dir'], "reftest")
        dirs['abs_xpcshell_dir'] = os.path.join(dirs['abs_test_install_dir'], "xpcshell")
        dirs['abs_cppunittest_dir'] = os.path.join(dirs['abs_test_install_dir'], "cppunittest")
        dirs['abs_gtest_dir'] = os.path.join(dirs['abs_test_install_dir'], "gtest")
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

        self.register_virtualenv_module(name='pip>=1.5')
        self.register_virtualenv_module('psutil==3.1.1', method='pip')
        self.register_virtualenv_module(name='mock')
        self.register_virtualenv_module(name='simplejson')

        requirements_files = [
                os.path.join(dirs['abs_test_install_dir'],
                    'config',
                    'marionette_requirements.txt')]

        if os.path.isdir(dirs['abs_mochitest_dir']):
            # mochitest is the only thing that needs this
            requirements_files.append(
                os.path.join(dirs['abs_mochitest_dir'],
                             'websocketprocessbridge',
                             'websocketprocessbridge_requirements.txt'))

        for requirements_file in requirements_files:
            self.register_virtualenv_module(requirements=[requirements_file],
                                            two_pass=True)

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
                'gtest_dir': os.path.join(dirs['abs_test_install_dir'],
                                          'gtest'),
            }

            # TestingMixin._download_and_extract_symbols() will set
            # self.symbols_path when downloading/extracting.
            if self.symbols_path:
                str_format_values['symbols_path'] = self.symbols_path

            if suite_category in SUITE_DEFAULT_E10S and not c['e10s']:
                base_cmd.append('--disable-e10s')
            elif suite_category not in SUITE_DEFAULT_E10S and c['e10s']:
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

            if suite in ('browser-chrome-coverage', 'xpcshell-coverage'):
                base_cmd.append('--jscov-dir-prefix=%s' %
                                dirs['abs_blob_upload_dir'])

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


            for option in options:
                option = option % str_format_values
                if not option.endswith('None'):
                    base_cmd.append(option)

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

    def _query_try_flavor(self, category, suite):
        flavors = {
            "mochitest": [("plain.*", "mochitest"),
                          ("browser-chrome.*", "browser-chrome"),
                          ("mochitest-devtools-chrome.*", "devtools-chrome"),
                          ("chrome", "chrome")],
            "xpcshell": [("xpcshell", "xpcshell")],
            "reftest": [("reftest", "reftest"),
                        ("crashtest", "crashtest")]
        }
        for suite_pattern, flavor in flavors.get(category, []):
            if re.compile(suite_pattern).match(suite):
                return flavor

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

        extract_dirs = None
        if c['specific_tests_zip_dirs']:
            extract_dirs = list(c['minimum_tests_zip_dirs'])
            for category in c['specific_tests_zip_dirs'].keys():
                if c['run_all_suites'] or self._query_specified_suites(category) \
                        or 'run-tests' not in self.actions:
                    extract_dirs.extend(c['specific_tests_zip_dirs'][category])

        if c.get('run_all_suites'):
            target_categories = SUITE_CATEGORIES
        else:
            target_categories = [cat for cat in SUITE_CATEGORIES
                                 if self._query_specified_suites(cat) is not None]
        super(DesktopUnittest, self).download_and_extract(extract_dirs=extract_dirs,
                                                          suite_categories=target_categories)

    def stage_files(self):
        for category in SUITE_CATEGORIES:
            suites = self._query_specified_suites(category)
            stage = getattr(self, '_stage_{}'.format(category), None)
            if suites and stage:
                stage(suites)

    def _stage_files(self, bin_name=None):
        dirs = self.query_abs_dirs()
        abs_app_dir = self.query_abs_app_dir()

        # For mac these directories are in Contents/Resources, on other
        # platforms abs_res_dir will point to abs_app_dir.
        abs_res_dir = self.query_abs_res_dir()
        abs_res_components_dir = os.path.join(abs_res_dir, 'components')
        abs_res_plugins_dir = os.path.join(abs_res_dir, 'plugins')
        abs_res_extensions_dir = os.path.join(abs_res_dir, 'extensions')

        if bin_name:
            self.info('copying %s to %s' % (os.path.join(dirs['abs_test_bin_dir'],
                      bin_name), os.path.join(abs_app_dir, bin_name)))
            shutil.copy2(os.path.join(dirs['abs_test_bin_dir'], bin_name),
                         os.path.join(abs_app_dir, bin_name))

        self.copytree(dirs['abs_test_bin_components_dir'],
                      abs_res_components_dir,
                      overwrite='overwrite_if_exists')
        self.mkdir_p(abs_res_plugins_dir)
        self.copytree(dirs['abs_test_bin_plugins_dir'],
                      abs_res_plugins_dir,
                      overwrite='overwrite_if_exists')
        if os.path.isdir(dirs['abs_test_extensions_dir']):
            self.mkdir_p(abs_res_extensions_dir)
            self.copytree(dirs['abs_test_extensions_dir'],
                          abs_res_extensions_dir,
                          overwrite='overwrite_if_exists')

    def _stage_xpcshell(self, suites):
        self._stage_files(self.config['xpcshell_name'])

    def _stage_cppunittest(self, suites):
        abs_res_dir = self.query_abs_res_dir()
        dirs = self.query_abs_dirs()
        abs_cppunittest_dir = dirs['abs_cppunittest_dir']

        # move manifest and js fils to resources dir, where tests expect them
        files = glob.glob(os.path.join(abs_cppunittest_dir, '*.js'))
        files.extend(glob.glob(os.path.join(abs_cppunittest_dir, '*.manifest')))
        for f in files:
            self.move(f, abs_res_dir)

    def _stage_gtest(self, suites):
        abs_res_dir = self.query_abs_res_dir()
        abs_app_dir = self.query_abs_app_dir()
        dirs = self.query_abs_dirs()
        abs_gtest_dir = dirs['abs_gtest_dir']
        dirs['abs_test_bin_dir'] = os.path.join(dirs['abs_test_install_dir'], 'bin')

        files = glob.glob(os.path.join(dirs['abs_test_bin_plugins_dir'], 'gmp-*'))
        files.append(os.path.join(abs_gtest_dir, 'dependentlibs.list.gtest'))
        for f in files:
            self.move(f, abs_res_dir)

        self.copytree(os.path.join(abs_gtest_dir, 'gtest_bin'),
                      os.path.join(abs_app_dir))

    def _stage_mozmill(self, suites):
        self._stage_files()
        dirs = self.query_abs_dirs()
        modules = ['jsbridge', 'mozmill']
        for module in modules:
            self.install_module(module=os.path.join(dirs['abs_mozmill_dir'],
                                                    'resources',
                                                    module))


    # pull defined in VCSScript.
    # preflight_run_tests defined in TestingMixin.

    def run_tests(self):
        for category in SUITE_CATEGORIES:
            self._run_category_suites(category)

    def get_timeout_for_category(self, suite_category):
        if suite_category == 'cppunittest':
            return 2500
        return self.config["suite_definitions"][suite_category].get('run_timeout', 1000)

    def _run_category_suites(self, suite_category):
        """run suite(s) to a specific category"""
        dirs = self.query_abs_dirs()
        suites = self._query_specified_suites(suite_category)
        abs_app_dir = self.query_abs_app_dir()
        abs_res_dir = self.query_abs_res_dir()

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
                    options_list = suites[suite].get('options', [])
                    tests_list = suites[suite].get('tests', [])
                    env = copy.deepcopy(suites[suite].get('env', {}))
                else:
                    options_list = suites[suite]
                    tests_list = []

                flavor = self._query_try_flavor(suite_category, suite)
                try_options, try_tests = self.try_args(flavor)

                cmd.extend(self.query_options(options_list,
                                              try_options,
                                              str_format_values=replace_dict))
                cmd.extend(self.query_tests_args(tests_list,
                                                 try_tests,
                                                 str_format_values=replace_dict))

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

                if suite_category == "reftest":
                    ref_formatter = imp.load_source(
                        "ReftestFormatter",
                        os.path.abspath(
                            os.path.join(dirs["abs_reftest_dir"], "output.py")))
                    parser.formatter = ref_formatter.ReftestFormatter()

                if self.query_minidump_stackwalk():
                    env['MINIDUMP_STACKWALK'] = self.minidump_stackwalk_path
                env['MOZ_UPLOAD_DIR'] = self.query_abs_dirs()['abs_blob_upload_dir']
                env['MINIDUMP_SAVE_PATH'] = self.query_abs_dirs()['abs_blob_upload_dir']
                if not os.path.isdir(env['MOZ_UPLOAD_DIR']):
                    self.mkdir_p(env['MOZ_UPLOAD_DIR'])
                env = self.query_env(partial_env=env, log_level=INFO)
                cmd_timeout = self.get_timeout_for_category(suite_category)
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
                if self._is_windows() and suite_category != 'gtest':
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
