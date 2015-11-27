#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""firefox_ui_updates.py

Author: Armen Zambrano G.
        Henrik Skupin
"""
import copy
import os
import sys
import urlparse

from mozharness.base.log import FATAL
from mozharness.base.python import PostScriptRun, PreScriptAction
from mozharness.mozilla.structuredlog import StructuredOutputParser
from mozharness.mozilla.testing.testbase import (
    TestingMixin,
    testing_config_options,
)
from mozharness.mozilla.vcstools import VCSToolsScript

# Command line arguments for firefox ui tests
firefox_ui_tests_harness_config_options = [
    [["--e10s"], {
        'dest': 'e10s',
        'action': 'store_true',
        'default': False,
        'help': 'Enable multi-process (e10s) mode when running tests.',
    }],
]

# General command line arguments for Firefox ui tests
firefox_ui_tests_config_options = [
    [['--dry-run'], {
        'dest': 'dry_run',
        'default': False,
        'help': 'Only show what was going to be tested.',
    }],
    [['--firefox-ui-branch'], {
        'dest': 'firefox_ui_branch',
        'help': 'which branch to use for firefox_ui_tests',
    }],
    [['--firefox-ui-repo'], {
        'dest': 'firefox_ui_repo',
        'default': 'https://github.com/mozilla/firefox-ui-tests.git',
        'help': 'which firefox_ui_tests repo to use',
    }],
    [['--symbols-path=SYMBOLS_PATH'], {
        'dest': 'symbols_path',
        'help': 'absolute path to directory containing breakpad '
                'symbols, or the url of a zip file containing symbols.',
    }],
] + firefox_ui_tests_harness_config_options \
    + copy.deepcopy(testing_config_options)

# Command line arguments for update tests
firefox_ui_update_harness_config_options = [
    [['--update-allow-mar-channel'], {
        'dest': 'update_allow_mar_channel',
        'help': 'Additional MAR channel to be allowed for updates, e.g. '
                '"firefox-mozilla-beta" for updating a release build to '
                'the latest beta build.',
    }],
    [['--update-channel'], {
        'dest': 'update_channel',
        'help': 'Update channel to use.',
    }],
    [['--update-direct-only'], {
        'action': 'store_true',
        'dest': 'update_direct_only',
        'help': 'Only perform a direct update.',
    }],
    [['--update-fallback-only'], {
        'action': 'store_true',
        'dest': 'update_fallback_only',
        'help': 'Only perform a fallback update.',
    }],
    [['--update-override-url'], {
        'dest': 'update_override_url',
        'help': 'Force specified URL to use for update checks.',
    }],
    [['--update-target-buildid'], {
        'dest': 'update_target_buildid',
        'help': 'Build ID of the updated build',
    }],
    [['--update-target-version'], {
        'dest': 'update_target_version',
        'help': 'Version of the updated build.',
    }],
]

firefox_ui_update_config_options = firefox_ui_update_harness_config_options \
    + copy.deepcopy(firefox_ui_tests_config_options)


class FirefoxUITests(TestingMixin, VCSToolsScript):

    cli_script = 'runtests.py'

    def __init__(self, config_options=None,
                 all_actions=None, default_actions=None,
                 *args, **kwargs):
        config_options = config_options or firefox_ui_tests_config_options
        actions = [
            'clobber',
            'download-and-extract',
            'checkout',  # keep until firefox-ui-tests are located in tree
            'create-virtualenv',
            'install',
            'run-tests',
            'uninstall',
        ]

        super(FirefoxUITests, self).__init__(
            config_options=config_options,
            all_actions=all_actions or actions,
            default_actions=default_actions or actions,
            *args, **kwargs)

        self.reports = {'html': 'report.html', 'xunit': 'report.xml'}

        self.firefox_ui_repo = self.config['firefox_ui_repo']
        self.firefox_ui_branch = self.config.get('firefox_ui_branch')

        if not self.firefox_ui_branch:
            self.fatal(
                'Please specify --firefox-ui-branch. Valid values can be found '
                'in here https://github.com/mozilla/firefox-ui-tests/branches')

        # As long as we don't run on buildbot the installers are not handled by TestingMixin
        self.installer_url = self.config.get('installer_url')
        self.installer_path = self.config.get('installer_path')

    @PreScriptAction('create-virtualenv')
    def _pre_create_virtualenv(self, action):
        dirs = self.query_abs_dirs()

        # List of exact versions of mozbase packages which are known to work
        requirements_file = os.path.join(dirs['fx_ui_dir'], 'requirements.txt')
        if os.path.isfile(requirements_file):
            self.register_virtualenv_module(requirements=[requirements_file])

        # Optional packages to be installed, e.g. for Jenkins
        if self.config.get('virtualenv_modules'):
            for module in self.config['virtualenv_modules']:
                self.register_virtualenv_module(module)

        self.register_virtualenv_module('firefox-ui-tests', url=dirs['fx_ui_dir'])

    @PreScriptAction('checkout')
    def _pre_checkout(self, action):
        if not self.firefox_ui_branch:
            self.fatal(
                'Please specify --firefox-ui-branch. Valid values can be found '
                'in here https://github.com/mozilla/firefox-ui-tests/branches')

    def checkout(self):
        """
        We checkout firefox_ui_tests and update to the right branch
        for it.
        """
        dirs = self.query_abs_dirs()

        self.vcs_checkout(
            repo=self.firefox_ui_repo,
            dest=dirs['fx_ui_dir'],
            branch=self.firefox_ui_branch,
            vcs='gittool',
            env=self.query_env(),
        )

    def clobber(self):
        """Delete the working directory"""
        super(FirefoxUITests, self).clobber()

        # Also ensure to delete the reports directory to get rid of old files
        dirs = self.query_abs_dirs()
        self.rmtree(dirs['abs_reports_dir'], error_level=FATAL)

    def copy_reports_to_upload_dir(self):
        self.info("Copying reports to upload dir...")

        dirs = self.query_abs_dirs()
        for report in self.reports:
            self.copy_to_upload_dir(os.path.join(dirs['abs_reports_dir'], self.reports[report]),
                                    dest=os.path.join('reports', self.reports[report]),
                                    short_desc='%s log' % self.reports[report],
                                    long_desc='%s log' % self.reports[report],
                                    max_backups=self.config.get("log_max_rotate", 0))

    def download_and_extract(self):
        """Overriding method from TestingMixin until firefox-ui-tests are in tree.

        Right now we only care about the installer and symbolds.

        """
        self._download_installer()

        if self.config.get('download_symbols'):
            self._download_and_extract_symbols()

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs

        abs_dirs = VCSToolsScript.query_abs_dirs(self)
        abs_dirs.update({
            'abs_reports_dir': os.path.join(abs_dirs['base_work_dir'], 'reports'),
            'fx_ui_dir': os.path.join(abs_dirs['abs_work_dir'], 'firefox_ui_tests'),
        })
        self.abs_dirs = abs_dirs

        return self.abs_dirs

    def query_harness_args(self, extra_harness_config_options=None):
        """Collects specific update test related command line arguments.

        Sub classes should override this method for their own specific arguments.
        """
        extra_harness_config_options = extra_harness_config_options or []
        config_options = firefox_ui_tests_harness_config_options + extra_harness_config_options

        args = []
        for option in config_options:
            dest = option[1]['dest']
            name = self.config.get(dest)

            if name:
                if type(name) is bool:
                    args.append(option[0][0])
                else:
                    args.extend([option[0][0], self.config[dest]])

        return args

    def query_minidump_stackwalk(self):
        """We don't have an extracted test package available to get the manifest file.

        So we have to explicitely download the latest version of the manifest from the
        mozilla-central repository and feed it into the query_minidump_stackwalk() method.

        We can remove this whole method once our tests are part of the tree.

        """
        manifest_path = None

        if self.config.get('download_minidump_stackwalk'):
            tooltool_manifest = self.query_minidump_tooltool_manifest()
            url_base = 'https://hg.mozilla.org/mozilla-central/raw-file/default/testing/'

            dirs = self.query_abs_dirs()
            manifest_path = os.path.join(dirs['abs_work_dir'], 'releng.manifest')
            try:
                self.download_file(urlparse.urljoin(url_base, tooltool_manifest),
                                   manifest_path)
            except Exception as e:
                self.fatal('Download of tooltool manifest file failed: %s' % e.message)

        super(FirefoxUITests, self).query_minidump_stackwalk(manifest=manifest_path)

    @PostScriptRun
    def _upload_reports_post_run(self):
        if self.config.get("copy_reports_post_run", True):
            self.copy_reports_to_upload_dir()

    def run_test(self, binary_path, env=None, marionette_port=2828):
        """All required steps for running the tests against an installer."""
        dirs = self.query_abs_dirs()

        cmd = [
            self.query_python_path(),
            os.path.join(dirs['fx_ui_dir'], 'firefox_ui_harness', self.cli_script),
            '--binary', binary_path,
            '--address', 'localhost:{}'.format(marionette_port),

            # Use the work dir to get temporary data stored
            '--workspace', dirs['abs_work_dir'],

            # logging options
            '--gecko-log=-',  # output from the gecko process redirected to stdout
            '--log-raw=-',  # structured log for output parser redirected to stdout

            # additional reports helpful for Jenkins and inpection via Treeherder
            '--log-html', os.path.join(dirs["abs_reports_dir"], self.reports['html']),
            '--log-xunit', os.path.join(dirs["abs_reports_dir"], self.reports['xunit']),
        ]

        # Collect all pass-through harness options to the script
        cmd.extend(self.query_harness_args())

        # Set further environment settings
        env = env or self.query_env()

        if self.symbols_url:
            cmd.extend(['--symbols-path', self.symbols_url])

        if self.query_minidump_stackwalk():
            env['MINIDUMP_STACKWALK'] = self.minidump_stackwalk_path

        parser = StructuredOutputParser(config=self.config,
                                        log_obj=self.log_obj,
                                        strict=False)

        return_code = self.run_command(cmd,
                                       cwd=dirs['abs_work_dir'],
                                       output_timeout=300,
                                       output_parser=parser,
                                       env=env)

        tbpl_status, log_level = parser.evaluate_parser(return_code)
        self.buildbot_status(tbpl_status, level=log_level)

        return return_code

    @PreScriptAction('run-tests')
    def _pre_run_tests(self, action):
        if not self.installer_path and not self.installer_url:
            self.critical('Please specify an installer via --installer-path or --installer-url.')
            sys.exit(1)

    def run_tests(self):
        """Run all the tests"""
        return self.run_test(
            binary_path=self.binary_path,
            env=self.query_env(),
        )


class FirefoxUIUpdateTests(FirefoxUITests):

    cli_script = 'cli_update.py'

    def __init__(self, config_options=None, *args, **kwargs):
        config_options = config_options or firefox_ui_update_config_options

        FirefoxUITests.__init__(self, config_options=config_options,
                                *args, **kwargs)

    def query_harness_args(self):
        """Collects specific update test related command line arguments."""
        return FirefoxUITests.query_harness_args(self,
                                                 firefox_ui_update_harness_config_options)
