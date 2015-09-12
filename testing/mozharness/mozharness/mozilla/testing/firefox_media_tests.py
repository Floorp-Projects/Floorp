#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** BEGIN LICENSE BLOCK *****
"""firefox_media_tests.py

Author: Maja Frydrychowicz
"""
import copy
import os
import re

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
    [['--firefox-media-repo'], {
        'dest': 'firefox_media_repo',
        'default': 'https://github.com/mjzffr/firefox-media-tests.git',
        'help': 'which firefox_media_tests repo to use',
    }],
    [['--firefox-media-branch'], {
        'dest': 'firefox_media_branch',
        'default': 'master',
        'help': 'which branch to use for firefox_media_tests',
    }],
    [['--firefox-media-rev'], {
        'dest': 'firefox_media_rev',
        'help': 'which firefox_media_tests revision to use',
    }],
    [['--firefox-ui-repo'], {
        'dest': 'firefox_ui_repo',
        'default': 'https://github.com/mozilla/firefox-ui-tests.git',
        'help': 'which firefox_ui_tests repo to use',
    }],
    [['--firefox-ui-branch'], {
        'dest': 'firefox_ui_branch',
        'default': 'master',
        'help': 'which branch to use for firefox_ui_tests',
    }],
    [['--firefox-ui-rev'], {
        'dest': 'firefox_ui_rev',
        'help': 'which firefox_ui_tests revision to use',
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
            'checkout',
            'create-virtualenv',
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

    @PreScriptAction('create-virtualenv')
    def _pre_create_virtualenv(self, action):
        dirs = self.query_abs_dirs()
        # cwd is $workspace/build
        self.register_virtualenv_module(name='firefox-ui-tests',
                                        url=dirs['firefox_ui_dir'],
                                        method='pip',
                                        editable='true')
        self.register_virtualenv_module(name='firefox-media-tests',
                                        url=dirs['firefox_media_dir'],
                                        method='pip',
                                        editable='true')

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(FirefoxMediaTestsBase, self).query_abs_dirs()
        dirs = {
            'firefox_media_dir': os.path.join(abs_dirs['abs_work_dir'],
                                              'firefox-media-tests')
        }
        dirs['firefox_ui_dir'] = os.path.join(dirs['firefox_media_dir'],
                                              'firefox-ui-tests')
        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    @PreScriptAction('checkout')
    def _pre_checkout(self, action):
        super(FirefoxMediaTestsBase, self)._pre_checkout(action)
        c = self.config
        dirs = self.query_abs_dirs()
        self.firefox_media_vc = {
            'branch': c['firefox_media_branch'],
            'repo': c['firefox_media_repo'],
            'revision': c['firefox_media_rev'],
            'dest': dirs['firefox_media_dir'],
        }
        self.firefox_ui_vc = {
            'branch': c['firefox_ui_branch'],
            'repo': c['firefox_ui_repo'],
            'revision': c['firefox_ui_rev'],
            'dest': dirs['firefox_ui_dir']
        }

    def checkout(self):
        revision = self.vcs_checkout(vcs='gittool', **self.firefox_media_vc)
        if revision:
            self.vcs_checkout(vcs='gittool', **self.firefox_ui_vc)

    def _query_cmd(self):
        """ Determine how to call firefox-media-tests """
        if not self.binary_path:
            self.fatal("Binary path could not be determined. "
                       "Should be set by default during 'install' action.")
        cmd = ['firefox-media-tests']
        cmd += ['--binary', self.binary_path]
        if self.symbols_path:
            cmd += ['--symbols-path', self.symbols_path]
        if self.media_urls:
            cmd += ['--urls', self.media_urls]
        if self.profile:
            cmd += ['--profile', self.profile]
        if self.tests:
            cmd.append(self.tests)
        if self.e10s:
            cmd.append('--e10s')
        return cmd

    def run_media_tests(self):
        cmd = self._query_cmd()
        self.job_result_parser = JobResultParser(
            config=self.config,
            log_obj=self.log_obj,
            error_list=self.error_list
        )
        return_code = self.run_command(
            cmd,
            output_timeout=self.test_timeout,
            output_parser=self.job_result_parser
        )
        self.job_result_parser.return_code = return_code
        return self.job_result_parser.status
