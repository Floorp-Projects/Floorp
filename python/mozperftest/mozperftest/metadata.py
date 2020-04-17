# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


class Metadata:
    def __init__(self, mach_cmd, env, flavor):
        self._mach_cmd = mach_cmd
        self.flavor = flavor
        self.browser = {"prefs": {}}
        self._result = None
        self._output = None
        self._env = env

    def set_output(self, output):
        self._output = output

    def get_output(self):
        return self._output

    def set_result(self, result):
        self._result = result

    def get_result(self):
        return self._result

    def update_browser_prefs(self, prefs):
        self.browser["prefs"].update(prefs)

    def get_browser_prefs(self):
        return self.browser["prefs"]
