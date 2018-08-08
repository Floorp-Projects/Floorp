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
import subprocess

from mach.decorators import CommandProvider, Command
from mozboot.util import get_state_dir
from mozbuild.base import MozbuildObject, MachCommandBase
from mozpack.copier import FileCopier
from mozpack.manifests import InstallManifest

HERE = os.path.dirname(os.path.realpath(__file__))
BENCHMARK_REPOSITORY = 'https://github.com/mozilla/perf-automation'
BENCHMARK_REVISION = '6beb3d3e22abce8cf8e2e89bc45acd4152258f12'


class RaptorRunner(MozbuildObject):
    def run_test(self, raptor_args):
        """
        We want to do couple of things before running raptor
        1. Clone mozharness
        2. Make config for raptor mozharness
        3. Run mozharness
        """

        self.init_variables(raptor_args)
        self.setup_benchmarks()
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

    def setup_benchmarks(self):
        """Make sure benchmarks are linked to the proper location in the objdir.

        Benchmarks can either live in-tree or in an external repository. In the latter
        case also clone/update the repository if necessary.
        """
        print("Updating external benchmarks from {}".format(BENCHMARK_REPOSITORY))

        # Set up the external repo
        external_repo_path = os.path.join(get_state_dir()[0], 'performance-tests')

        if not os.path.isdir(external_repo_path):
            subprocess.check_call(['git', 'clone', BENCHMARK_REPOSITORY, external_repo_path])
        else:
            subprocess.check_call(['git', 'checkout', 'master'], cwd=external_repo_path)
            subprocess.check_call(['git', 'pull'], cwd=external_repo_path)

        subprocess.check_call(['git', 'checkout', BENCHMARK_REVISION], cwd=external_repo_path)

        # Link benchmarks to the objdir
        benchmark_paths = (
            os.path.join(external_repo_path, 'benchmarks'),
            os.path.join(self.topsrcdir, 'third_party', 'webkit', 'PerformanceTests'),
        )
        manifest = InstallManifest()

        for benchmark_path in benchmark_paths:
            for path in os.listdir(benchmark_path):
                abspath = os.path.join(benchmark_path, path)
                if not os.path.isdir(abspath) or path.startswith('.'):
                    continue

                manifest.add_link(abspath, path)

        copier = FileCopier()
        manifest.populate_registry(copier)
        copier.copy(os.path.join(self.topobjdir, 'testing', 'raptor', 'benchmarks'))

    def make_config(self):
        default_actions = ['populate-webroot', 'install-chrome', 'create-virtualenv', 'run-tests']
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
            'raptor_cmd_line_args': self.raptor_args,
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
