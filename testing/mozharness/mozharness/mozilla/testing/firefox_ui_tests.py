#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****


import copy
import os
import sys

from mozharness.base.log import FATAL, WARNING
from mozharness.base.python import PostScriptRun, PreScriptAction
from mozharness.mozilla.structuredlog import StructuredOutputParser
from mozharness.mozilla.testing.testbase import (
    TestingMixin,
    testing_config_options,
)
from mozharness.mozilla.vcstools import VCSToolsScript


# General command line arguments for Firefox ui tests
firefox_ui_tests_config_options = [
    [["--allow-software-gl-layers"], {
        "action": "store_true",
        "dest": "allow_software_gl_layers",
        "default": False,
        "help": "Permits a software GL implementation (such as LLVMPipe) to use the GL compositor.",
    }],
    [['--dry-run'], {
        'dest': 'dry_run',
        'default': False,
        'help': 'Only show what was going to be tested.',
    }],
    [["--e10s"], {
        'dest': 'e10s',
        'action': 'store_true',
        'default': False,
        'help': 'Enable multi-process (e10s) mode when running tests.',
    }],
    [['--symbols-path=SYMBOLS_PATH'], {
        'dest': 'symbols_path',
        'help': 'absolute path to directory containing breakpad '
                'symbols, or the url of a zip file containing symbols.',
    }],
    [['--tag=TAG'], {
        'dest': 'tag',
        'help': 'Subset of tests to run (local, remote).',
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

    # Needs to be overwritten in sub classes
    cli_script = None

    def __init__(self, config_options=None,
                 all_actions=None, default_actions=None,
                 *args, **kwargs):
        config_options = config_options or firefox_ui_tests_config_options
        actions = [
            'clobber',
            'download-and-extract',
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

        # Code which doesn't run on buildbot has to include the following properties
        self.binary_path = self.config.get('binary_path')
        self.installer_path = self.config.get('installer_path')
        self.installer_url = self.config.get('installer_url')
        self.test_packages_url = self.config.get('test_packages_url')
        self.test_url = self.config.get('test_url')

        if not self.test_url and not self.test_packages_url:
            self.fatal(
                'You must use --test-url, or --test-packages-url')

    @PreScriptAction('create-virtualenv')
    def _pre_create_virtualenv(self, action):
        dirs = self.query_abs_dirs()

        requirements = os.path.join(dirs['abs_test_install_dir'],
                                    'config', 'firefox_ui_requirements.txt')
        self.register_virtualenv_module(requirements=[requirements], two_pass=True)

    def download_and_extract(self):
        """Override method from TestingMixin for more specific behavior."""
        extract_dirs = ['config/*',
                        'firefox-ui/*',
                        'marionette/*',
                        'mozbase/*',
                        'tools/wptserve/*',
                        ]
        super(FirefoxUITests, self).download_and_extract(extract_dirs=extract_dirs)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs

        abs_dirs = super(FirefoxUITests, self).query_abs_dirs()
        abs_tests_install_dir = os.path.join(abs_dirs['abs_work_dir'], 'tests')

        dirs = {
            'abs_blob_upload_dir': os.path.join(abs_dirs['abs_work_dir'], 'blobber_upload_dir'),
            'abs_test_install_dir': abs_tests_install_dir,
            'abs_fxui_dir': os.path.join(abs_tests_install_dir, 'firefox-ui'),
        }

        for key in dirs:
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs

        return self.abs_dirs

    def query_harness_args(self, extra_harness_config_options=None):
        """Collects specific test related command line arguments.

        Sub classes should override this method for their own specific arguments.
        """
        config_options = extra_harness_config_options or []

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

    def run_test(self, binary_path, env=None, marionette_port=2828):
        """All required steps for running the tests against an installer."""
        dirs = self.query_abs_dirs()

        # Import the harness to retrieve the location of the cli scripts
        import firefox_ui_harness

        cmd = [
            self.query_python_path(),
            os.path.join(os.path.dirname(firefox_ui_harness.__file__),
                         self.cli_script),
            '--binary', binary_path,
            '--address', 'localhost:{}'.format(marionette_port),

            # Resource files to serve via local webserver
            '--server-root', os.path.join(dirs['abs_fxui_dir'], 'resources'),

            # Use the work dir to get temporary data stored
            '--workspace', dirs['abs_work_dir'],

            # logging options
            '--gecko-log=-',  # output from the gecko process redirected to stdout
            '--log-raw=-',  # structured log for output parser redirected to stdout

            # additional reports helpful for Jenkins and inpection via Treeherder
            '--log-html', os.path.join(dirs['abs_blob_upload_dir'], 'report.html'),
            '--log-xunit', os.path.join(dirs['abs_blob_upload_dir'], 'report.xml'),

            # Enable tracing output to log transmission protocol
            '-vv',
        ]

        # Collect all pass-through harness options to the script
        cmd.extend(self.query_harness_args())

        # Translate deprecated --e10s flag
        if not self.config.get('e10s'):
            cmd.append('--disable-e10s')

        if self.symbols_url:
            cmd.extend(['--symbols-path', self.symbols_url])

        if self.config.get('tag'):
            cmd.extend(['--tag', self.config['tag']])

        parser = StructuredOutputParser(config=self.config,
                                        log_obj=self.log_obj,
                                        strict=False)

        # Add the default tests to run
        tests = [os.path.join(dirs['abs_fxui_dir'], 'tests', test) for test in self.default_tests]
        cmd.extend(tests)

        # Set further environment settings
        env = env or self.query_env()
        env.update({'MINIDUMP_SAVE_PATH': dirs['abs_blob_upload_dir']})
        if self.query_minidump_stackwalk():
            env.update({'MINIDUMP_STACKWALK': self.minidump_stackwalk_path})

        if self.config['allow_software_gl_layers']:
            env['MOZ_LAYERS_ALLOW_SOFTWARE_GL'] = '1'

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


class FirefoxUIFunctionalTests(FirefoxUITests):

    cli_script = 'cli_functional.py'
    default_tests = [
        os.path.join('puppeteer', 'manifest.ini'),
        os.path.join('functional', 'manifest.ini'),
    ]


class FirefoxUIUpdateTests(FirefoxUITests):

    cli_script = 'cli_update.py'
    default_tests = [
        os.path.join('update', 'manifest.ini')
    ]

    def __init__(self, config_options=None, *args, **kwargs):
        config_options = config_options or firefox_ui_update_config_options

        super(FirefoxUIUpdateTests, self).__init__(
            config_options=config_options,
            *args, **kwargs
        )

    def query_harness_args(self):
        """Collects specific update test related command line arguments."""
        return super(FirefoxUIUpdateTests, self).query_harness_args(
            firefox_ui_update_harness_config_options)
