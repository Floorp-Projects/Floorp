# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os

import mozunit
import pytest


@pytest.mark.skipif(os.name == 'nt', reason="fzf not installed on host")
def test_paths(run_mach, capfd):
    cmd = ['try', 'fuzzy', '--no-push',
           '-q', "'linux64/opt-xpcshell", 'caps/tests/unit/test_origin.js']
    assert run_mach(cmd) == 0

    output = capfd.readouterr().out
    print(output)

    # If there are more than one tasks here, it means that something went wrong
    # with the path filtering.
    expected = """
    "tasks": [
        "test-linux64/opt-xpcshell-e10s-1"
    ]""".lstrip()

    assert expected in output


if __name__ == '__main__':
    mozunit.main()
