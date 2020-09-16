# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from collections import defaultdict
from mozperftest.utils import MachLogger


class Metadata(MachLogger):
    def __init__(self, mach_cmd, env, flavor, script):
        MachLogger.__init__(self, mach_cmd)
        self._mach_cmd = mach_cmd
        self.flavor = flavor
        self.options = defaultdict(dict)
        self._results = []
        self._output = None
        self._env = env
        self.script = script

    def run_hook(self, name, **kw):
        # this bypasses layer restrictions on args,
        # which is fine since it's a user script
        return self._env.hooks.run(name, **kw)

    def set_output(self, output):
        self._output = output

    def get_output(self):
        return self._output

    def add_result(self, result):
        self._results.append(result)

    def get_results(self):
        return self._results

    def clear_results(self):
        self._results = []

    def update_options(self, name, options):
        self.options[name].update(options)

    def get_options(self, name):
        return self.options[name]
