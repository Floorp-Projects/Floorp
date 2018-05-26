#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****


import copy
import os
import sys

# load modules from parent dir
sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

GECKO_SRCDIR = os.path.join(os.path.expanduser('~'), 'checkouts', 'gecko')

TELEMETRY_TEST_HOME = os.path.join(GECKO_SRCDIR, 'toolkit', 'components', 'telemetry',
                                   'tests', 'marionette')

from mozharness.base.python import PreScriptAction
from mozharness.mozilla.structuredlog import StructuredOutputParser
from mozharness.mozilla.testing.testbase import (
    TestingMixin,
    testing_config_options,
)
from mozharness.mozilla.testing.codecoverage import (
    CodeCoverageMixin,
    code_coverage_config_options
)
from mozharness.mozilla.vcstools import VCSToolsScript

# General command line arguments for Firefox ui tests
telemetry_tests_config_options = [
    [["--allow-software-gl-layers"], {
        "action": "store_true",
        "dest": "allow_software_gl_layers",
        "default": False,
        "help": "Permits a software GL implementation (such as LLVMPipe) "
                "to use the GL compositor.",
    }],
    [["--enable-webrender"], {
        "action": "store_true",
        "dest": "enable_webrender",
        "default": False,
        "help": "Tries to enable the WebRender compositor.",
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
] + copy.deepcopy(testing_config_options) \
  + copy.deepcopy(code_coverage_config_options)


class TelemetryTests(TestingMixin, VCSToolsScript, CodeCoverageMixin):
    def __init__(self, config_options=None,
                 all_actions=None, default_actions=None,
                 *args, **kwargs):
        config_options = config_options or telemetry_tests_config_options
        actions = [
            'clobber',
            'download-and-extract',
            'create-virtualenv',
            'install',
            'run-tests',
            'uninstall',
        ]

        super(TelemetryTests, self).__init__(
            config_options=config_options,
            all_actions=all_actions or actions,
            default_actions=default_actions or actions,
            *args, **kwargs)

        # Code which runs in automation has to include the following properties
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

        requirements = os.path.join(GECKO_SRCDIR, 'testing',
                                    'config', 'telemetry_tests_requirements.txt')
        self.register_virtualenv_module(requirements=[requirements], two_pass=True)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs

        abs_dirs = super(TelemetryTests, self).query_abs_dirs()

        dirs = {
            'abs_blob_upload_dir': os.path.join(abs_dirs['abs_work_dir'], 'blobber_upload_dir'),
            'abs_telemetry_dir': TELEMETRY_TEST_HOME,
        }

        for key in dirs:
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs

        return self.abs_dirs

    def run_test(self, binary_path, env=None, marionette_port=2828):
        """All required steps for running the tests against an installer."""
        dirs = self.query_abs_dirs()

        # Import the harness to retrieve the location of the cli scripts
        import telemetry_harness

        cmd = [
            self.query_python_path(),
            os.path.join(os.path.dirname(telemetry_harness.__file__),
                         self.cli_script),
            '--binary', binary_path,
            '--address', 'localhost:{}'.format(marionette_port),

            # Resource files to serve via local webserver
            '--server-root', os.path.join(dirs['abs_telemetry_dir'], 'harness', 'www'),
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

        parser = StructuredOutputParser(config=self.config,
                                        log_obj=self.log_obj,
                                        strict=False)

        # Add the default tests to run
        tests = [os.path.join(dirs['abs_telemetry_dir'], 'tests', test)
                 for test in self.default_tests]
        cmd.extend(tests)

        # Set further environment settings
        env = env or self.query_env()
        env.update({'MINIDUMP_SAVE_PATH': dirs['abs_blob_upload_dir']})
        if self.query_minidump_stackwalk():
            env.update({'MINIDUMP_STACKWALK': self.minidump_stackwalk_path})
        env['RUST_BACKTRACE'] = '1'

        # If code coverage is enabled, set GCOV_PREFIX env variable
        if self.config.get('code_coverage'):
            env['GCOV_PREFIX'] = self.gcov_dir

        return_code = self.run_command(cmd,
                                       cwd=dirs['abs_work_dir'],
                                       output_timeout=300,
                                       output_parser=parser,
                                       env=env)

        tbpl_status, log_level = parser.evaluate_parser(return_code)
        self.record_status(tbpl_status, level=log_level)

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


class TelemetryClientTests(TelemetryTests):
    cli_script = 'runtests.py'
    default_tests = [
        os.path.join('client', 'manifest.ini'),
        os.path.join('unit', 'manifest.ini'),
    ]


if __name__ == '__main__':
    myScript = TelemetryClientTests()
    myScript.run_and_exit()
