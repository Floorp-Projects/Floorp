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

        # As long as we don't run on buildbot the following properties have be set on our own
        self.binary_path = self.config.get('binary_path')
        self.installer_path = self.config.get('installer_path')
        self.installer_url = self.config.get('installer_url')
        self.test_packages_url = self.config.get('test_packages_url')
        self.test_url = self.config.get('test_url')

        self.reports = {'html': 'report.html', 'xunit': 'report.xml'}

        self.firefox_ui_repo = self.config['firefox_ui_repo']
        self.firefox_ui_branch = self.config.get('firefox_ui_branch')

        if not self.test_url and not self.test_packages_url and not self.firefox_ui_branch:
            self.fatal(
                'You must use --test-url, --test-packages-url, or --firefox-ui-branch (valid '
                'values can be found at: https://github.com/mozilla/firefox-ui-tests/branches)')

    @PreScriptAction('create-virtualenv')
    def _pre_create_virtualenv(self, action):
        dirs = self.query_abs_dirs()

        # If tests are used from common.tests.zip install every Python package
        # via the single requirements file
        if self.test_packages_url or self.test_url:
            requirements = os.path.join(dirs['abs_test_install_dir'],
                                        'config', 'firefox_ui_requirements.txt')
            self.register_virtualenv_module(requirements=[requirements], two_pass=True)

        # We have a non-packaged version of Firefox UI tests. So install requirements
        # and the firefox-ui-tests package separately
        # TODO - Can be removed when the github repository is no longer needed
        else:
            # Register all modules for firefox-ui-tests including all dependencies
            # as strict versions to ensure newer releases won't break something
            requirements = os.path.join(dirs['abs_test_install_dir'],
                                        'requirements.txt')
            self.register_virtualenv_module(requirements=[requirements])

    def checkout(self):
        """Clone the firefox-ui-tests repository."""
        dirs = self.query_abs_dirs()

        self.vcs_checkout(
            repo=self.firefox_ui_repo,
            dest=dirs['abs_test_install_dir'],
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

    def download_and_extract(self):
        """Overriding method from TestingMixin for more specific behavior.

        We use the test_packages_url command line argument to check where to get the
        harness, puppeteer, and tests from and how to set them up.

        """
        if self.test_packages_url or self.test_url:
            target_unzip_dirs = ['config/*',
                                 'firefox-ui/*',
                                 'marionette/*',
                                 'mozbase/*',
                                 'puppeteer/*',
                                 'tools/wptserve/*',
                                 ]
            super(FirefoxUITests, self).download_and_extract(target_unzip_dirs=target_unzip_dirs)

        else:
            self.checkout()
            self._download_installer()

            if self.config.get('download_symbols'):
                self._download_and_extract_symbols()

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs

        abs_dirs = super(FirefoxUITests, self).query_abs_dirs()
        abs_tests_install_dir = os.path.join(abs_dirs['abs_work_dir'], 'tests')

        dirs = {
            'abs_reports_dir': os.path.join(abs_dirs['base_work_dir'], 'reports'),
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

    def query_minidump_stackwalk(self):
        """Download the minidump stackwalk binary.

        We can remove this whole method once we no longer need the github repository.

        """
        # If the test package is available use it
        if self.test_packages_url or self.test_url:
            return super(FirefoxUITests, self).query_minidump_stackwalk()

        # Otherwise grab the manifest file from hg.mozilla.org
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

        return super(FirefoxUITests, self).query_minidump_stackwalk(manifest=manifest_path)

    @PostScriptRun
    def copy_logs_to_upload_dir(self):
        """Overwrite this method so we also upload the other (e.g. report) files"""
        # Copy logs and report files to the upload folder
        super(FirefoxUITests, self).copy_logs_to_upload_dir()

        dirs = self.query_abs_dirs()
        self.info("Copying reports to upload dir...")
        for report in self.reports:
            self.copy_to_upload_dir(os.path.join(dirs['abs_reports_dir'], self.reports[report]),
                                    dest=os.path.join('reports', self.reports[report]),
                                    short_desc='%s log' % self.reports[report],
                                    long_desc='%s log' % self.reports[report],
                                    max_backups=self.config.get("log_max_rotate", 0),
                                    error_level=WARNING,
                                    )

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
            '--log-html', os.path.join(dirs["abs_reports_dir"], self.reports['html']),
            '--log-xunit', os.path.join(dirs["abs_reports_dir"], self.reports['xunit']),
        ]

        # Collect all pass-through harness options to the script
        cmd.extend(self.query_harness_args())

        # Translate deprecated --e10s flag
        if not self.config.get('e10s'):
            cmd.append('--disable-e10s')

        # Set further environment settings
        env = env or self.query_env()

        if self.symbols_url:
            cmd.extend(['--symbols-path', self.symbols_url])

        if self.query_minidump_stackwalk():
            env['MINIDUMP_STACKWALK'] = self.minidump_stackwalk_path

        parser = StructuredOutputParser(config=self.config,
                                        log_obj=self.log_obj,
                                        strict=False)

        # Add the default tests to run
        tests = [os.path.join(dirs['abs_fxui_dir'], 'tests', test) for test in self.default_tests]
        cmd.extend(tests)

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

    def download_unzip(self, url, parent_dir, target_unzip_dirs=None, halt_on_failure=True):
        """Overwritten method from BaseScript until bug 1237706 is fixed.

        The downloaded file will always be saved to the working directory and is not getting
        deleted after extracting.

        Args:
            url (str): URL where the file to be downloaded is located.
            parent_dir (str): directory where the downloaded file will
                              be extracted to.
            target_unzip_dirs (list, optional): directories inside the zip file to extract.
                                                Defaults to `None`.
            halt_on_failure (bool, optional): whether or not to redefine the
                                              log level as `FATAL` on errors. Defaults to True.

        """
        import fnmatch
        import itertools
        import functools
        import zipfile

        def _filter_entries(namelist):
            """Filter entries of the archive based on the specified list of extract_dirs."""
            filter_partial = functools.partial(fnmatch.filter, namelist)
            for entry in itertools.chain(*map(filter_partial, target_unzip_dirs or ['*'])):
                yield entry

        dirs = self.query_abs_dirs()
        zip = self.download_file(url, parent_dir=dirs['abs_work_dir'],
                                 error_level=FATAL)

        try:
            self.info('Using ZipFile to extract {} to {}'.format(zip, parent_dir))
            with zipfile.ZipFile(zip) as bundle:
                for entry in _filter_entries(bundle.namelist()):
                    bundle.extract(entry, path=parent_dir)

                    # ZipFile doesn't preserve permissions: http://bugs.python.org/issue15795
                    fname = os.path.realpath(os.path.join(parent_dir, entry))
                    mode = bundle.getinfo(entry).external_attr >> 16 & 0x1FF
                    # Only set permissions if attributes are available.
                    if mode:
                        os.chmod(fname, mode)
        except zipfile.BadZipfile as e:
            self.log('%s (%s)' % (e.message, zip),
                     level=FATAL, exit_code=2)


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
