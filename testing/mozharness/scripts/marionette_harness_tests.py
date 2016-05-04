#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
import copy
import os
import sys

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.python import PreScriptAction
from mozharness.base.python import (
    VirtualenvMixin,
    virtualenv_config_options,
)
from mozharness.base.script import BaseScript
from mozharness.mozilla.buildbot import (
    BuildbotMixin, TBPL_SUCCESS, TBPL_WARNING, TBPL_FAILURE,
    TBPL_EXCEPTION
)

marionette_harness_tests_config_options = [
    [['--tests'], {
        'dest': 'test_path',
        'default': None,
        'help': 'Path to test_*.py or directory relative to src root.',
    }],
    [['--src-dir'], {
        'dest': 'rel_src_dir',
        'default': None,
        'help': 'Path to hg.mo source checkout relative to work dir.',
    }],

] + copy.deepcopy(virtualenv_config_options)

marionette_harness_tests_config = {
    "find_links": [
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,
    # relative to workspace
    "rel_src_dir": os.path.join("build", "src"),
}

class MarionetteHarnessTests(VirtualenvMixin, BuildbotMixin, BaseScript):

    def __init__(self, config_options=None,
                 all_actions=None, default_actions=None,
                 *args, **kwargs):
        config_options = config_options or marionette_harness_tests_config_options
        actions = [
            'clobber',
            'create-virtualenv',
            'run-tests',
        ]
        super(MarionetteHarnessTests, self).__init__(
            config_options=config_options,
            all_actions=all_actions or actions,
            default_actions=default_actions or actions,
            config=marionette_harness_tests_config,
            *args, **kwargs)

    @PreScriptAction('create-virtualenv')
    def _pre_create_virtualenv(self, action):
        dirs = self.query_abs_dirs()
        c = self.config
        requirements = os.path.join(
            dirs['abs_src_dir'],
            'testing', 'config',
            'marionette_harness_test_requirements.txt'
        )
        self.register_virtualenv_module(
           requirements=[requirements],
           two_pass=True
        )

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        c = self.config
        abs_dirs = super(MarionetteHarnessTests, self).query_abs_dirs()
        dirs = {
            'abs_src_dir': os.path.abspath(
                os.path.join(abs_dirs['base_work_dir'], c['rel_src_dir'])
            ),
        }

        for key in dirs:
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs

        return self.abs_dirs

    def _get_pytest_status(self, code):
        """
        Translate pytest exit code to TH status

        Based on https://github.com/pytest-dev/pytest/blob/master/_pytest/main.py#L21-L26
        """
        if code == 0:
            return TBPL_SUCCESS
        elif code == 1:
            return TBPL_WARNING
        elif 1 < code < 6:
            self.error("pytest returned exit code: %s" % code)
            return TBPL_FAILURE
        else:
            return TBPL_EXCEPTION

    def run_tests(self):
        """Run all the tests"""
        dirs = self.query_abs_dirs()
        test_relpath = self.config.get(
            'test_path',
            os.path.join('testing', 'marionette',
                         'harness', 'marionette', 'tests',
                         'harness_unit')
        )
        test_path = os.path.join(dirs['abs_src_dir'], test_relpath)
        self.activate_virtualenv()
        import pytest
        log_path = os.path.join(dirs['abs_log_dir'], 'pytest.log')
        command = ['--resultlog', log_path, '--verbose', test_path]
        self.info('Calling pytest.main with the following arguments: %s' % command)
        status = self._get_pytest_status(pytest.main(command))
        self.read_from_file(log_path)
        self.buildbot_status(status)


if __name__ == '__main__':
    script = MarionetteHarnessTests()
    script.run_and_exit()
