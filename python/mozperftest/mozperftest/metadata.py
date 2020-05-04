# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from mozperftest.utils import MachLogger


class Metadata(MachLogger):
    def __init__(self, mach_cmd, env, flavor):
        MachLogger.__init__(self, mach_cmd)
        self._mach_cmd = mach_cmd
        self.flavor = flavor
        self.browser = {"prefs": {}}
        self._results = []
        self._output = None
        self._env = env

    def run_hook(self, name, **kw):
        # this bypasses layer restrictions on args,
        # which is fine since it's user script
        return self._env.run_hook(name, **kw)

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

    def update_browser_prefs(self, prefs):
        self.browser["prefs"].update(prefs)

    def get_browser_prefs(self):
        return self.browser["prefs"]
