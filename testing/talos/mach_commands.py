# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Integrates Talos mozharness with mach

from __future__ import absolute_import, print_function, unicode_literals

import logging
import os
import six
import sys
import json
import socket

from mozbuild.base import (
    MozbuildObject,
    BinaryNotFoundException,
)
from mach.decorators import Command

HERE = os.path.dirname(os.path.realpath(__file__))


class TalosRunner(MozbuildObject):
    def run_test(self, talos_args):
        """
        We want to do couple of things before running Talos
        1. Clone mozharness
        2. Make config for Talos Mozharness
        3. Run mozharness
        """

        try:
            self.init_variables(talos_args)
        except BinaryNotFoundException as e:
            self.log(logging.ERROR, "talos", {"error": str(e)}, "ERROR: {error}")
            self.log(logging.INFO, "raptor", {"help": e.help()}, "{help}")
            return 1

        self.make_config()
        self.write_config()
        self.make_args()
        return self.run_mozharness()

    def init_variables(self, talos_args):
        self.talos_dir = os.path.join(self.topsrcdir, "testing", "talos")
        self.mozharness_dir = os.path.join(self.topsrcdir, "testing", "mozharness")
        self.talos_json = os.path.join(self.talos_dir, "talos.json")
        self.config_file_path = os.path.join(
            self._topobjdir, "testing", "talos-in_tree_conf.json"
        )
        self.binary_path = self.get_binary_path()
        self.virtualenv_script = os.path.join(
            self.topsrcdir, "third_party", "python", "virtualenv", "virtualenv.py"
        )
        self.virtualenv_path = os.path.join(self._topobjdir, "testing", "talos-venv")
        self.python_interp = sys.executable
        self.talos_args = talos_args

    def make_config(self):
        default_actions = ["populate-webroot"]
        default_actions.extend(
            [
                "create-virtualenv",
                "run-tests",
            ]
        )
        self.config = {
            "run_local": True,
            "talos_json": self.talos_json,
            "binary_path": self.binary_path,
            "repo_path": self.topsrcdir,
            "obj_path": self.topobjdir,
            "log_name": "talos",
            "virtualenv_path": self.virtualenv_path,
            "pypi_url": "http://pypi.python.org/simple",
            "base_work_dir": self.mozharness_dir,
            "exes": {
                "python": self.python_interp,
                "virtualenv": [self.python_interp, self.virtualenv_script],
            },
            "title": socket.gethostname(),
            "default_actions": default_actions,
            "talos_extra_options": ["--develop"] + self.talos_args,
            "python3_manifest": {
                "win32": "python3.manifest",
                "win64": "python3_x64.manifest",
            },
        }

    def make_args(self):
        self.args = {
            "config": {},
            "initial_config_file": self.config_file_path,
        }

    def write_config(self):
        try:
            config_file = open(self.config_file_path, "wb")
            config_file.write(six.ensure_binary(json.dumps(self.config)))
        except IOError as e:
            err_str = "Error writing to Talos Mozharness config file {0}:{1}"
            print(err_str.format(self.config_file_path, str(e)))
            raise e

    def run_mozharness(self):
        sys.path.insert(0, self.mozharness_dir)
        from mozharness.mozilla.testing.talos import Talos

        talos_mh = Talos(
            config=self.args["config"],
            initial_config_file=self.args["initial_config_file"],
        )
        return talos_mh.run()


def create_parser():
    sys.path.insert(0, HERE)  # allow to import the talos package
    from talos.cmdline import create_parser

    return create_parser(mach_interface=True)


@Command(
    "talos-test",
    category="testing",
    description="Run talos tests (performance testing).",
    parser=create_parser,
)
def run_talos_test(command_context, **kwargs):
    talos = command_context._spawn(TalosRunner)

    try:
        return talos.run_test(sys.argv[2:])
    except Exception as e:
        print(str(e))
        return 1
