# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import subprocess

import yaml
from mozboot.util import get_state_dir


class PresetHandler(object):
    config_path = os.path.join(get_state_dir(), "try_presets.yml")

    def __init__(self):
        self._presets = {}

    @property
    def presets(self):
        if not self._presets and os.path.isfile(self.config_path):
            with open(self.config_path, 'r') as fh:
                self._presets = yaml.safe_load(fh) or {}

        return self._presets

    def __contains__(self, name):
        return name in self.presets

    def __getitem__(self, name):
        return self.presets[name]

    def __str__(self):
        return yaml.safe_dump(self.presets, default_flow_style=False)

    def list(self):
        if not self.presets:
            print("no presets found")
        else:
            print(self)

    def edit(self):
        if 'EDITOR' not in os.environ:
            print("error: must set the $EDITOR environment variable to use --edit-presets")
            return

        subprocess.call([os.environ['EDITOR'], self.config_path])

    def save(self, name, **data):
        self.presets[name] = data

        with open(self.config_path, "w") as fh:
            fh.write(str(self))


presets = PresetHandler()
