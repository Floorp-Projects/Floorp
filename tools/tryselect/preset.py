# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import shlex
import subprocess

import yaml


class PresetHandler(object):

    def __init__(self, path):
        self.path = path
        self._presets = {}

    @property
    def presets(self):
        if not self._presets and os.path.isfile(self.path):
            with open(self.path, 'r') as fh:
                self._presets = yaml.safe_load(fh) or {}

        return self._presets

    def __contains__(self, name):
        return name in self.presets

    def __getitem__(self, name):
        return self.presets[name]

    def __len__(self):
        return len(self.presets)

    def __str__(self):
        if not self.presets:
            return ''
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

        subprocess.call(shlex.split(os.environ['EDITOR']) + [self.path])

    def save(self, name, **data):
        self.presets[name] = data

        with open(self.path, "w") as fh:
            fh.write(str(self))


class MergedHandler(object):
    def __init__(self, *paths):
        """Helper class for dealing with multiple preset files."""
        self.handlers = [PresetHandler(p) for p in paths]

    def __contains__(self, name):
        return any(name in handler for handler in self.handlers)

    def __getitem__(self, name):
        for handler in self.handlers:
            if name in handler:
                return handler[name]
        raise KeyError(name)

    def __len__(self):
        return sum(len(h) for h in self.handlers)

    def __str__(self):
        all_presets = {
            k: v
            for handler in self.handlers
            for k, v in handler.presets.items()
        }
        return yaml.safe_dump(all_presets, default_flow_style=False)

    def list(self):
        if len(self) == 0:
            print("no presets found")
            return

        for handler in self.handlers:
            val = str(handler)
            if val:
                val = '\n  '.join([''] + val.splitlines() + [''])  # indent all lines by 2 spaces
                print("Presets from {}:".format(handler.path))
                print(val)
