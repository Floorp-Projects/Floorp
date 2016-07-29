#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** BEGIN LICENSE BLOCK *****

import copy
import os
import re
import urlparse

from mozharness.base.log import ERROR, WARNING
from mozharness.base.script import PreScriptAction
from mozharness.mozilla.testing.testbase import (TestingMixin,
                                                 testing_config_options)
from mozharness.mozilla.testing.unittest import TestSummaryOutputParserHelper
from mozharness.mozilla.vcstools import VCSToolsScript

BUSTED = 'busted'
TESTFAILED = 'testfailed'
UNKNOWN = 'unknown'
EXCEPTION = 'exception'
SUCCESS = 'success'

media_test_config_options = [
    [["--media-urls"],
     {"action": "store",
      "dest": "media_urls",
      "help": "Path to ini file that lists media urls for tests.",
      }],
    [["--profile"],
     {"action": "store",
      "dest": "profile",
      "default": None,
      "help": "Path to FF profile that should be used by Marionette",
      }],
    [["--test-timeout"],
     {"action": "store",
      "dest": "test_timeout",
      "default": 10000,
      "help": ("Number of seconds without output before"
                "firefox-media-tests is killed."
                "Set this based on expected time for all media to play."),
      }],
    [["--tests"],
     {"action": "store",
      "dest": "tests",
      "default": None,
      "help": ("Test(s) to run. Path to test_*.py or "
               "test manifest (*.ini)"),
      }],
    [["--e10s"],
     {"dest": "e10s",
      "action": "store_true",
      "default": False,
      "help": "Enable e10s when running marionette tests."
      }],
    [["--suite"],
     {"action": "store",
      "dest": "test_suite",
      "default": "media-tests",
      "help": "suite name",
      }],
    [['--browsermob-script'],
     {'help': 'path to the browsermob-proxy shell script or batch file',
     }],
    [['--browsermob-port'],
     {'help': 'port to run the browsermob proxy on',
     }],
] + (copy.deepcopy(testing_config_options))

class JobResultParser(TestSummaryOutputParserHelper):
    """ Parses test output to determine overall result."""
    def __init__(self, **kwargs):
        super(JobResultParser, self).__init__(**kwargs)
        self.return_code = 0
        # External-resource errors that should not count as test failures
        self.exception_re = re.compile(r'^TEST-UNEXPECTED-ERROR.*'
                                       r'TimeoutException: Error loading page,'
                                       r' timed out')
        self.exceptions = []

    def parse_single_line(self, line):
        super(JobResultParser, self).parse_single_line(line)
        if self.exception_re.match(line):
            self.exceptions.append(line)

    @property
    def status(self):
        status = UNKNOWN
        if self.passed and self.failed == 0:
            status = SUCCESS
        elif self.exceptions:
            status = EXCEPTION
        elif self.failed:
            status = TESTFAILED
        elif self.return_code:
            status = BUSTED
        return status


class FirefoxMediaTestsBase(TestingMixin, VCSToolsScript):
    job_result_parser = None

    error_list = [
        {'substr': 'FAILED (errors=', 'level': WARNING},
        {'substr': r'''Could not successfully complete transport of message to Gecko, socket closed''', 'level': ERROR},
        {'substr': r'''Connection to Marionette server is lost. Check gecko''', 'level': ERROR},
        {'substr': 'Timeout waiting for marionette on port', 'level': ERROR},
        {'regex': re.compile(r'''(TEST-UNEXPECTED|PROCESS-CRASH|CRASH|ERROR|FAIL)'''), 'level': ERROR},
        {'regex': re.compile(r'''(\b\w*Exception)'''), 'level': ERROR},
        {'regex': re.compile(r'''(\b\w*Error)'''), 'level': ERROR},
     ]

    def __init__(self, config_options=None, all_actions=None,
                 default_actions=None, **kwargs):
        self.config_options = media_test_config_options + (config_options or [])
        actions = [
            'clobber',
            'download-and-extract',
            'create-virtualenv',
            'install',
            'run-media-tests',
        ]
        super(FirefoxMediaTestsBase, self).__init__(
            config_options=self.config_options,
            all_actions=all_actions or actions,
            default_actions=default_actions or actions,
            **kwargs
        )
        c = self.config

        self.media_urls = c.get('media_urls')
        self.profile = c.get('profile')
        self.test_timeout = int(c.get('test_timeout'))
        self.tests = c.get('tests')
        self.e10s = c.get('e10s')
        self.installer_url = c.get('installer_url')
        self.installer_path = c.get('installer_path')
        self.binary_path = c.get('binary_path')
        self.test_packages_url = c.get('test_packages_url')
        self.test_url = c.get('test_url')
        self.browsermob_script = c.get('browsermob_script')
        self.browsermob_port = c.get('browsermob_port')

    @PreScriptAction('create-virtualenv')
    def _pre_create_virtualenv(self, action):
        dirs = self.query_abs_dirs()

        media_tests_requirements = os.path.join(dirs['abs_test_install_dir'],
                                                'config',
                                                'external-media-tests-requirements.txt')

        if os.access(media_tests_requirements, os.F_OK):
            self.register_virtualenv_module(requirements=[media_tests_requirements],
                                            two_pass=True)

    def download_and_extract(self):
        """Overriding method from TestingMixin for more specific behavior.

        We use the test_packages_url command line argument to check where to get the
        harness, puppeteer, and tests from and how to set them up.

        """
        target_unzip_dirs = ['config/*',
                             'external-media-tests/*',
                             'marionette/*',
                             'mozbase/*',
                             'puppeteer/*',
                             'tools/wptserve/*',
                             ]
        super(FirefoxMediaTestsBase, self).download_and_extract(
                target_unzip_dirs=target_unzip_dirs)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(FirefoxMediaTestsBase, self).query_abs_dirs()
        dirs = {
            'abs_test_install_dir' : os.path.join(abs_dirs['abs_work_dir'],
                                                    'tests')
        }
        dirs['external-media-tests'] = os.path.join(dirs['abs_test_install_dir'],
                                                    'external-media-tests')
        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def _query_cmd(self):
        """ Determine how to call firefox-media-tests """
        if not self.binary_path:
            self.fatal("Binary path could not be determined. "
                       "Should be set by default during 'install' action.")
        dirs = self.query_abs_dirs()

        import external_media_harness.runtests

        cmd = [
            self.query_python_path(),
            external_media_harness.runtests.__file__
        ]

        cmd += ['--binary', self.binary_path]
        if self.symbols_path:
            cmd += ['--symbols-path', self.symbols_path]
        if self.media_urls:
            cmd += ['--urls', self.media_urls]
        if self.profile:
            cmd += ['--profile', self.profile]
        if self.tests:
            cmd.append(self.tests)
        if not self.e10s:
            cmd.append('--disable-e10s')
        if self.browsermob_script:
            cmd += ['--browsermob-script', self.browsermob_script]
        if self.browsermob_port:
            cmd += ['--browsermob-port', self.browsermob_port]

        test_suite = self.config.get('test_suite')
        if test_suite not in self.config["suite_definitions"]:
            self.fatal("%s is not defined in the config!" % test_suite)

        test_manifest = None if test_suite != 'media-youtube-tests' else \
            os.path.join(dirs['external-media-tests'],
                         'external_media_tests',
                         'playback', 'youtube', 'manifest.ini')
        config_fmt_args = {
            'test_manifest': test_manifest,
        }

        for s in self.config["suite_definitions"][test_suite]["options"]:
            cmd.append(s % config_fmt_args)

        return cmd

    def query_minidump_stackwalk(self):
        """We don't have an extracted test package available to get the manifest file.

        So we have to explicitely download the latest version of the manifest from the
        mozilla-central repository and feed it into the query_minidump_stackwalk() method.

        We can remove this whole method once our tests are part of the tree.

        """
        manifest_path = None

        if os.environ.get('MINIDUMP_STACKWALK') or self.config.get('download_minidump_stackwalk'):
            tooltool_manifest = self.query_minidump_tooltool_manifest()
            url_base = 'https://hg.mozilla.org/mozilla-central/raw-file/default/testing/'

            dirs = self.query_abs_dirs()
            manifest_path = os.path.join(dirs['abs_work_dir'], 'releng.manifest')
            try:
                self.download_file(urlparse.urljoin(url_base, tooltool_manifest),
                                   manifest_path)
            except Exception as e:
                self.fatal('Download of tooltool manifest file failed: %s' % e.message)

        return super(FirefoxMediaTestsBase, self).query_minidump_stackwalk(manifest=manifest_path)

    def run_media_tests(self):
        cmd = self._query_cmd()
        self.job_result_parser = JobResultParser(
            config=self.config,
            log_obj=self.log_obj,
            error_list=self.error_list
        )

        env = self.query_env()
        if self.query_minidump_stackwalk():
            env['MINIDUMP_STACKWALK'] = self.minidump_stackwalk_path

        return_code = self.run_command(
            cmd,
            output_timeout=self.test_timeout,
            output_parser=self.job_result_parser,
            env=env
        )
        self.job_result_parser.return_code = return_code
        return self.job_result_parser.status
