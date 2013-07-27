# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Integrates Talos mozharness with mach

from __future__ import print_function, unicode_literals

import os
import sys
import json
import which

from mozbuild.base import (
    MozbuildObject,
    MachCommandBase
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

class TalosRunner(MozbuildObject):
    def run_test(self, suite, repo, rev):
        """
        We want to do couple of things before running Talos
        1. Clone mozharness
        2. Make config for Talos Mozharness
        3. Run mozharness
        """

        print("Running Talos test suite %s" % suite)
        self.init_variables(suite, repo, rev)
        self.clone_mozharness()
        self.make_config()
        self.write_config()
        self.make_args()
        return self.run_mozharness()

    def init_variables(self, suite, repo, rev):
        self.suite = suite
        self.mozharness_repo = repo
        self.mozharness_rev = rev

        self.talos_dir = os.path.join(self.topsrcdir, 'testing', 'talos')
        self.mozharness_dir = os.path.join(self.topobjdir, 'mozharness')
        self.config_dir = os.path.join(self.mozharness_dir, 'configs', 'talos')
        self.talos_json = os.path.join(self.talos_dir, 'talos.json')
        self.config_filename = 'in_tree_conf.json'
        self.config_file_path = os.path.join(self.config_dir,
                                             self.config_filename)
        self.binary_path = self.get_binary_path()
        self.virtualenv_script = os.path.join(self.topsrcdir, 'python',
                                              'virtualenv', 'virtualenv.py')
        self.virtualenv_path = os.path.join(self.mozharness_dir, 'venv')
        self.python_interp = sys.executable

    def clone_mozharness(self):
        """Clones mozharness into topobjdir/mozharness
           using mercurial. If mozharness is already cloned,
           it updates it to the latest version"""
        try:
            mercurial = which.which('hg')
        except which.WhichError as e:
            print("You don't have hg in your PATH: {0}".format(e))
            raise e
        clone_cmd = [mercurial, 'clone', '-r', self.mozharness_rev,
                     self.mozharness_repo, self.mozharness_dir]
        pull_cmd = [mercurial, 'pull', '-r', self.mozharness_rev, '-u']

        dot_hg = os.path.join(self.mozharness_dir, '.hg')
        if os.path.exists(dot_hg):
            self.run_process(args=pull_cmd, cwd=self.mozharness_dir)
        else:
            self.run_process(args=clone_cmd)

    def make_config(self):
        self.config = {
            'talos_json': self.talos_json,
            'binary_path': self.binary_path,
            'log_name': 'talos',
            'virtualenv_path': self.virtualenv_path,
            'pypi_url': 'http://pypi.python.org/simple',
            'use_talos_json': True,
            'base_work_dir': self.mozharness_dir,
            'exes': {
                'python': self.python_interp,
                'virtualenv': [self.python_interp, self.virtualenv_script]
            },
            'title': os.uname()[1].lower().split('.')[0],
            'default_actions': [
                'clone-talos',
                'create-virtualenv',
                'run-tests',
            ],
            'python_webserver': True
        }

    def make_args(self):
        self.args = {
            'config': {
                'suite': self.suite,
                'use_talos_json': True
            },
           'initial_config_file': self.config_file_path,
       }

    def write_config(self):
        try:
            config_file = open(self.config_file_path, 'wb')
            config_file.write(json.dumps(self.config))
        except IOError as e:
            err_str = "Error writing to Talos Mozharness config file {0}:{1}"
            print(err_str.format(self.config_file_path, str(e)))
            raise e

    def run_mozharness(self):
        sys.path.append(self.mozharness_dir)
        from mozharness.mozilla.testing.talos import Talos
        talos_mh = Talos(config=self.args['config'],
                         initial_config_file=self.args['initial_config_file'])
        return talos_mh.run()

@CommandProvider
class MachCommands(MachCommandBase):
    mozharness_repo = 'https://hg.mozilla.org/build/mozharness'
    mozharness_rev = 'production'

    @Command('talos-test', category='testing',
             description='Run talos tests.')
    @CommandArgument('suite', help='Talos test suite to run. Valid suites are '
                                   'chromez, dirtypaint, dromaeojs, other,'
                                   'svgr, rafx, tpn, tp5o, xperf.')
    @CommandArgument('--repo', default=mozharness_repo,
                     help='The mozharness repository to clone from. '
                          'Defaults to http://hg.mozilla.org/build/mozharness')
    @CommandArgument('--rev', default=mozharness_rev,
                     help='The mozharness revision to clone. Defaults to '
                          'production')
    def run_talos_test(self, suite, repo=None, rev=None):
        talos = self._spawn(TalosRunner)

        try:
            return talos.run_test(suite, repo, rev)
        except Exception as e:
            print(str(e))
            return 1
