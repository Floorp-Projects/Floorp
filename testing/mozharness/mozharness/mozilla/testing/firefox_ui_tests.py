#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""firefox_ui_updates.py

Author: Armen Zambrano G.
"""
import copy
import sys
import os

from mozharness.base.python import (
    PreScriptAction,
    VirtualenvMixin,
    virtualenv_config_options,
)
from mozharness.mozilla.vcstools import VCSToolsScript


class FirefoxUITests(VCSToolsScript, VirtualenvMixin):
    config_options = [
        [['--firefox-ui-repo'], {
            'dest': 'firefox_ui_repo',
            'default': 'https://github.com/mozilla/firefox-ui-tests.git',
            'help': 'which firefox_ui_tests repo to use',
        }],
        [['--firefox-ui-branch'], {
            'dest': 'firefox_ui_branch',
            'help': 'which branch to use for firefox_ui_tests',
        }],
    ] + copy.deepcopy(virtualenv_config_options)


    def __init__(self, config_options=[], all_actions=[], **kwargs):
        self.config_options += config_options

        if all_actions is None:
            # Default actions
            all_actions = [
                'clobber',
                'checkout',
                'create-virtualenv',
                'run-tests',
            ]

        super(FirefoxUITests, self).__init__(
            config_options=self.config_options,
            all_actions=all_actions,
            **kwargs
        )

        self.firefox_ui_repo = self.config['firefox_ui_repo']
        self.firefox_ui_branch = self.config.get('firefox_ui_branch')

        if not self.firefox_ui_branch:
            self.fatal(
                'Please specify --firefox-ui-branch. Valid values can be found '
                'in here https://github.com/mozilla/firefox-ui-tests/branches')

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(FirefoxUITests, self).query_abs_dirs()

        dirs = {
            'fx_ui_dir': os.path.join(abs_dirs['abs_work_dir'], 'firefox_ui_tests'),
        }

        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs
        return self.abs_dirs


    def checkout(self):
        '''
        We checkout firefox_ui_tests and update to the right branch
        for it.
        '''
        dirs = self.query_abs_dirs()

        self.vcs_checkout(
            repo=self.firefox_ui_repo,
            dest=dirs['fx_ui_dir'],
            branch=self.firefox_ui_branch,
            vcs='gittool'
        )


    @PreScriptAction('create-virtualenv')
    def _pre_create_virtualenv(self, action):
        dirs = self.query_abs_dirs()

        self.register_virtualenv_module(
            'firefox-ui-tests',
            url=dirs['fx_ui_dir'],
        )


    def run_tests(self):
        pass
