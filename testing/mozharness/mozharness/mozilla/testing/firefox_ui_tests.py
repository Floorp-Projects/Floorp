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

from mozharness.base.python import (
    PreScriptAction,
)
from mozharness.mozilla.testing.testbase import (
    TestingMixin,
    testing_config_options,
)
from mozharness.mozilla.vcstools import VCSToolsScript


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
] + copy.deepcopy(testing_config_options)

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
            'checkout',
            'create-virtualenv',
            'query_minidump_stackwalk',
            'run-tests',
        ]

        super(FirefoxUITests, self).__init__(
            config_options=config_options,
            all_actions=all_actions or actions,
            default_actions=default_actions or actions,
            *args, **kwargs)

        self.firefox_ui_repo = self.config['firefox_ui_repo']
        self.firefox_ui_branch = self.config.get('firefox_ui_branch')

        if not self.firefox_ui_branch:
            self.fatal(
                'Please specify --firefox-ui-branch. Valid values can be found '
                'in here https://github.com/mozilla/firefox-ui-tests/branches')

        # As long as we don't run on buildbot the installers are not handled by TestingMixin
        self.installer_url = self.config.get('installer_url')
        self.installer_path = self.config.get('installer_path')

        if self.installer_path:
            self.installer_path = os.path.abspath(self.installer_path)

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
            vcs='gittool'
        )

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs

        abs_dirs = VCSToolsScript.query_abs_dirs(self)
        abs_dirs.update({
            'fx_ui_dir': os.path.join(abs_dirs['abs_work_dir'], 'firefox_ui_tests'),
        })
        self.abs_dirs = abs_dirs

        return self.abs_dirs

    def query_extra_cmd_args(self):
        """Collects specific update test related command line arguments.

        Sub classes should override this method for their own specific arguments.
        """
        return []

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

    def run_test(self, installer_path, env=None, cleanup=True, marionette_port=2828):
        """All required steps for running the tests against an installer."""
        dirs = self.query_abs_dirs()

        gecko_log = os.path.join(dirs['abs_log_dir'], 'gecko.log')

        cmd = [
            self.query_python_path(),
            os.path.join(dirs['fx_ui_dir'], 'firefox_ui_harness', self.cli_script),
            '--installer', installer_path,
            # Log to stdout until tests are stable.
            # '--gecko-log', gecko_log,
            '--gecko-log=-',
            '--address', 'localhost:{}'.format(marionette_port),
            # Use the work dir to get temporary data stored
            '--workspace', dirs['abs_work_dir'],
        ]

        # Collect all pass-through harness options to the script
        cmd.extend(self.query_extra_cmd_args())

        # Set further environment settings
        env = env or self.query_env()
        if self.minidump_stackwalk_path:
            env['MINIDUMP_STACKWALK'] = self.minidump_stackwalk_path

            if self.query_symbols_url():
                cmd += ['--symbols-path', self.symbols_url]

        return_code = self.run_command(cmd, cwd=dirs['abs_work_dir'],
                                       output_timeout=300, env=env)

        # Return more output if we fail
        if return_code:
            if os.path.exists(gecko_log):
                contents = self.read_from_file(gecko_log, verbose=False)
                self.warning('== Dumping gecko output ==')
                self.warning(contents)
                self.warning('== End of gecko output ==')
            else:
                # We're outputting to stdout with --gecko-log=- so there is not log to
                # complaing about. Remove the commented line below when changing
                # this behaviour.
                # self.warning('No gecko.log was found: %s' % gecko_log)
                pass

        if cleanup:
            for filepath in (installer_path, gecko_log):
                if os.path.exists(filepath):
                    self.debug('Removing {}'.format(filepath))
                    os.remove(filepath)

        return return_code

    @PreScriptAction('run-tests')
    def _pre_run_tests(self, action):
        if not self.installer_path and not self.installer_url:
            self.critical('Please specify an installer via --installer-path or --installer-url.')
            sys.exit(1)

    def run_tests(self):
        dirs = self.query_abs_dirs()

        if self.installer_url:
            self.installer_path = self.download_file(
                self.installer_url,
                parent_dir=dirs['abs_work_dir']
            )

        return self.run_test(
            installer_path=self.installer_path,
            env=self.query_env(),
            cleanup=False,
        )


class FirefoxUIUpdateTests(FirefoxUITests):

    cli_script = 'cli_update.py'

    def __init__(self, config_options=None, *args, **kwargs):
        config_options = config_options or firefox_ui_update_config_options

        FirefoxUITests.__init__(self, config_options=config_options,
                                *args, **kwargs)

    def query_extra_cmd_args(self):
        """Collects specific update test related command line arguments."""
        args = []

        for option in firefox_ui_update_harness_config_options:
            dest = option[1]['dest']
            name = self.config.get(dest)

            if name:
                if type(name) is bool:
                    args.append(option[0][0])
                else:
                    args.extend([option[0][0], self.config[dest]])

        return args
