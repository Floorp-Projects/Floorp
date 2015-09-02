# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Integrates Talos mozharness with mach

from __future__ import absolute_import, print_function, unicode_literals

import os
import sys
import json
import which
import socket

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
    def run_test(self, suite, sps_profile):
        """
        We want to do couple of things before running Talos
        1. Clone mozharness
        2. Make config for Talos Mozharness
        3. Run mozharness
        """

        print("Running Talos test suite %s" % suite)
        self.init_variables(suite, sps_profile)
        self.make_config()
        self.write_config()
        self.make_args()
        return self.run_mozharness()

    def init_variables(self, suite, sps_profile):
        self.suite = suite
        self.sps_profile = sps_profile

        self.talos_dir = os.path.join(self.topsrcdir, 'testing', 'talos')
        self.talos_webroot = os.path.join(self.topobjdir, 'testing', 'talos')
        self.mozharness_dir = os.path.join(self.topsrcdir, 'testing',
                                           'mozharness')
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

    def make_config(self):
        self.config = {
            'run_local': True,
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
            'title': socket.gethostname(),
            'default_actions': [
                'populate-webroot',
                'create-virtualenv',
                'run-tests',
            ],
            'python_webserver': False,
            'talos_extra_options': ['--develop'],
        }

    def make_args(self):
        self.args = {
            'config': {
                'suite': self.suite,
                'sps_profile': self.sps_profile,
                'use_talos_json': True,
                'webroot': self.talos_webroot,
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
        sys.path.insert(0, self.mozharness_dir)
        from mozharness.mozilla.testing.talos import Talos
        talos_mh = Talos(config=self.args['config'],
                         initial_config_file=self.args['initial_config_file'])
        return talos_mh.run()

@CommandProvider
class MachCommands(MachCommandBase):
    @Command('talos-test', category='testing',
             description='Run talos tests (performance testing).')
    @CommandArgument('suite', help='Talos test suite to run. Valid suites are '
                                   'chromez, dirtypaint, dromaeojs, other,'
                                   'svgr, rafx, tpn, tp5o, xperf.')
    @CommandArgument('--spsProfile', default=False,
                     help='Use the Gecko Profiler to capture profiles that can '
                          'then be displayed by Cleopatra.', action='store_true')

    def run_talos_test(self, suite, spsProfile=False):
        talos = self._spawn(TalosRunner)

        try:
            return talos.run_test(suite, spsProfile)
        except Exception as e:
            print(str(e))
            return 1
