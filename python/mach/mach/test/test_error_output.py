# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

from mach.main import COMMAND_ERROR_TEMPLATE, MODULE_ERROR_TEMPLATE
from mozunit import main


def test_command_error(run_mach):
    result, stdout, stderr = run_mach(
        ["throw", "--message", "Command Error"], provider_files="throw.py"
    )
    assert result == 1
    assert COMMAND_ERROR_TEMPLATE % "throw" in stdout


def test_invoked_error(run_mach):
    result, stdout, stderr = run_mach(
        ["throw_deep", "--message", "Deep stack"], provider_files="throw.py"
    )
    assert result == 1
    assert MODULE_ERROR_TEMPLATE % "throw_deep" in stdout


if __name__ == "__main__":
    main()
