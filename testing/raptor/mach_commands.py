# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Originally taken from /talos/mach_commands.py

# Integrates raptor mozharness with mach

from __future__ import absolute_import, print_function, unicode_literals

import os
import sys
import json
import socket

from mozbuild.base import MozbuildObject, MachCommandBase
from mach.decorators import CommandProvider, Command

HERE = os.path.dirname(os.path.realpath(__file__))


class RaptorRunner(MozbuildObject):
    def run_test(self, raptor_args):
        """
        We want to do couple of things before running raptor
        1. Clone mozharness
        2. Make config for raptor mozharness
        3. Run mozharness
        """

        self.init_variables(raptor_args)
        self.make_config()
        self.write_config()
        self.make_args()
        return self.run_mozharness()

    def init_variables(self, raptor_args):
        self.raptor_dir = os.path.join(self.topsrcdir, 'testing', 'raptor')
        self.mozharness_dir = os.path.join(self.topsrcdir, 'testing',
                                           'mozharness')
        self.config_file_path = os.path.join(self._topobjdir, 'testing',
                                             'raptor-in_tree_conf.json')
        self.binary_path = self.get_binary_path()
        self.virtualenv_script = os.path.join(self.topsrcdir, 'third_party', 'python',
                                              'virtualenv', 'virtualenv.py')
        self.virtualenv_path = os.path.join(self._topobjdir, 'testing',
                                            'raptor-venv')
        self.python_interp = sys.executable
        self.raptor_args = raptor_args

    def make_config(self):
        default_actions = ['populate-webroot', 'create-virtualenv', 'run-tests']
        self.config = {
            'run_local': True,
            'binary_path': self.binary_path,
            'repo_path': self.topsrcdir,
            'raptor_path': self.raptor_dir,
            'obj_path': self.topobjdir,
            'log_name': 'raptor',
            'virtualenv_path': self.virtualenv_path,
            'pypi_url': 'http://pypi.python.org/simple',
            'base_work_dir': self.mozharness_dir,
            'exes': {
                'python': self.python_interp,
                'virtualenv': [self.python_interp, self.virtualenv_script],
            },
            'title': socket.gethostname(),
            'default_actions': default_actions,
            'raptor_extra_options': self.raptor_args,
            'python3_manifest': {
                'win32': 'python3.manifest',
                'win64': 'python3_x64.manifest',
            }
        }

    def make_args(self):
        self.args = {
            'config': {},
            'initial_config_file': self.config_file_path,
        }

    def write_config(self):
        try:
            config_file = open(self.config_file_path, 'wb')
            config_file.write(json.dumps(self.config))
            config_file.close()
        except IOError as e:
            err_str = "Error writing to Raptor Mozharness config file {0}:{1}"
            print(err_str.format(self.config_file_path, str(e)))
            raise e

    def run_mozharness(self):
        sys.path.insert(0, self.mozharness_dir)
        from mozharness.mozilla.testing.raptor import Raptor
        raptor_mh = Raptor(config=self.args['config'],
                           initial_config_file=self.args['initial_config_file'])
        return raptor_mh.run()


def create_parser():
    sys.path.insert(0, HERE)  # allow to import the raptor package
    from raptor.cmdline import create_parser
    return create_parser(mach_interface=True)


@CommandProvider
class MachRaptor(MachCommandBase):
    @Command('raptor-test', category='testing',
             description='Run raptor performance tests.',
             parser=create_parser)
    def run_raptor_test(self, **kwargs):
        raptor = self._spawn(RaptorRunner)

        try:
            return raptor.run_test(sys.argv[2:])
        except Exception as e:
            print(str(e))
            return 1
