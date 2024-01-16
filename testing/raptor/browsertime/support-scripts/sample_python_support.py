# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from base_python_support import BasePythonSupport


class SamplePythonSupport(BasePythonSupport):
    def modify_command(self, cmd):
        for i, entry in enumerate(cmd):
            if "{replace-with-constant-value}" in entry:
                cmd[i] = "25"
