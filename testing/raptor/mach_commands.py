# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Originally taken from /talos/mach_commands.py

# Integrates raptor mozharness with mach

from __future__ import absolute_import, print_function, unicode_literals

import os
import sys
import json
import shutil
import socket
import subprocess

import mozfile
from mach.decorators import CommandProvider, Command
from mozboot.util import get_state_dir
from mozbuild.base import MozbuildObject, MachCommandBase
from mozbuild.base import MachCommandConditions as conditions

HERE = os.path.dirname(os.path.realpath(__file__))
BENCHMARK_REPOSITORY = 'https://github.com/mozilla/perf-automation'
BENCHMARK_REVISION = '2720cdc790828952964524bb44ce8b4c14670e90'


class RaptorRunner(MozbuildObject):
    def run_test(self, raptor_args, kwargs):
        """
        We want to do a few things before running Raptor:
        1. Clone mozharness
        2. Make the config for Raptor mozharness
        3. Run mozharness
        """

        self.init_variables(raptor_args, kwargs)
        self.setup_benchmarks()
        self.make_config()
        self.write_config()
        self.make_args()
        return self.run_mozharness()

    def init_variables(self, raptor_args, kwargs):
        self.raptor_dir = os.path.join(self.topsrcdir, 'testing', 'raptor')
        self.mozharness_dir = os.path.join(self.topsrcdir, 'testing',
                                           'mozharness')
        self.config_file_path = os.path.join(self._topobjdir, 'testing',
                                             'raptor-in_tree_conf.json')
        self.binary_path = self.get_binary_path() if kwargs['app'] not in \
            ['geckoview', 'fennec', 'refbrow', 'fenix'] else None
        self.virtualenv_script = os.path.join(self.topsrcdir, 'third_party', 'python',
                                              'virtualenv', 'virtualenv.py')
        self.virtualenv_path = os.path.join(self._topobjdir, 'testing',
                                            'raptor-venv')
        self.python_interp = sys.executable
        self.raptor_args = raptor_args
        if kwargs.get('host', None) == 'HOST_IP':
            kwargs['host'] = os.environ['HOST_IP']
        self.host = kwargs['host']
        self.power_test = kwargs['power_test']
        self.memory_test = kwargs['memory_test']
        self.is_release_build = kwargs['is_release_build']

    def setup_benchmarks(self):
        """Make sure benchmarks are linked to the proper location in the objdir.

        Benchmarks can either live in-tree or in an external repository. In the latter
        case also clone/update the repository if necessary.
        """
        print("Updating external benchmarks from {}".format(BENCHMARK_REPOSITORY))

        # Set up the external repo
        external_repo_path = os.path.join(get_state_dir(), 'performance-tests')

        try:
            subprocess.check_output(['git', '--version'])
        except Exception as ex:
            print("Git is not available! Please install git and "
                  "ensure it is included in the terminal path")
            raise ex

        if not os.path.isdir(external_repo_path):
            subprocess.check_call(['git', 'clone', BENCHMARK_REPOSITORY, external_repo_path])
        else:
            subprocess.check_call(['git', 'checkout', 'master'], cwd=external_repo_path)
            subprocess.check_call(['git', 'pull'], cwd=external_repo_path)

        subprocess.check_call(['git', 'checkout', BENCHMARK_REVISION], cwd=external_repo_path)

        # Link or copy benchmarks to the objdir
        benchmark_paths = (
            os.path.join(external_repo_path, 'benchmarks'),
            os.path.join(self.topsrcdir, 'third_party', 'webkit', 'PerformanceTests'),
        )

        benchmark_dest = os.path.join(self.topobjdir, 'testing', 'raptor', 'benchmarks')
        if not os.path.isdir(benchmark_dest):
            os.makedirs(benchmark_dest)

        for benchmark_path in benchmark_paths:
            for name in os.listdir(benchmark_path):
                path = os.path.join(benchmark_path, name)
                dest = os.path.join(benchmark_dest, name)
                if not os.path.isdir(path) or name.startswith('.'):
                    continue

                if hasattr(os, 'symlink'):
                    if not os.path.exists(dest):
                        os.symlink(path, dest)
                else:
                    # Clobber the benchmark in case a recent update removed any files.
                    mozfile.remove(dest)
                    shutil.copytree(path, dest)

    def make_config(self):
        default_actions = [
            'populate-webroot',
            'install-chromium-distribution',
            'create-virtualenv',
            'run-tests'
        ]
        self.config = {
            'run_local': True,
            'binary_path': self.binary_path,
            'repo_path': self.topsrcdir,
            'raptor_path': self.raptor_dir,
            'obj_path': self.topobjdir,
            'log_name': 'raptor',
            'virtualenv_path': self.virtualenv_path,
            'pypi_url': 'http://pypi.org/simple',
            'base_work_dir': self.mozharness_dir,
            'exes': {
                'python': self.python_interp,
                'virtualenv': [self.python_interp, self.virtualenv_script],
            },
            'title': socket.gethostname(),
            'default_actions': default_actions,
            'raptor_cmd_line_args': self.raptor_args,
            'host': self.host,
            'power_test': self.power_test,
            'memory_test': self.memory_test,
            'is_release_build': self.is_release_build,
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

        build_obj = MozbuildObject.from_environment(cwd=HERE)

        firefox_android_browsers = ["fennec", "geckoview", "refbrow", "fenix"]

        if conditions.is_android(build_obj) or kwargs['app'] in firefox_android_browsers:
            from mozrunner.devices.android_device import verify_android_device
            from mozdevice import ADBAndroid, ADBHost
            if not verify_android_device(build_obj, install=True, app=kwargs['binary'],
                                         xre=True):  # Equivalent to 'run_local' = True.
                return 1

        debug_command = '--debug-command'
        if debug_command in sys.argv:
            sys.argv.remove(debug_command)

        raptor = self._spawn(RaptorRunner)

        try:
            if kwargs['app'] in firefox_android_browsers and kwargs['power_test']:
                device = ADBAndroid(verbose=True)
                adbhost = ADBHost(verbose=True)
                device_serial = "%s:5555" % device.get_ip_address()
                device.command_output(["tcpip", "5555"])
                raw_input("Please disconnect your device from USB then press Enter/return...")
                adbhost.command_output(["connect", device_serial])
                while len(adbhost.devices()) > 1:
                    raw_input("You must disconnect your device from USB before continuing.")
                # must reset the environment DEVICE_SERIAL which was set during
                # verify_android_device to match our new tcpip value.
                os.environ["DEVICE_SERIAL"] = device_serial
            return raptor.run_test(sys.argv[2:], kwargs)
        except Exception as e:
            print(repr(e))
            return 1
        finally:
            try:
                if kwargs['app'] in firefox_android_browsers and kwargs['power_test']:
                    raw_input("Connect device via USB and press Enter/return...")
                    device = ADBAndroid(device=device_serial, verbose=True)
                    device.command_output(["usb"])
                    adbhost.command_output(["disconnect", device_serial])
            except Exception:
                adbhost.command_output(["kill-server"])
