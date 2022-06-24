# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os

import mozunit
import pytest


@pytest.mark.skipif(os.name == "nt", reason="fzf not installed on host")
def test_query_paths(run_mach, capfd):
    cmd = [
        "try",
        "fuzzy",
        "--no-push",
        "-q",
        "^test-linux '64-qr/debug-xpcshell-nofis-",
        "caps/tests/unit/test_origin.js",
    ]
    assert run_mach(cmd) == 0

    output = capfd.readouterr().out
    print(output)

    # If there are more than one tasks here, it means that something went wrong
    # with the path filtering.
    expected = """
    "tasks": [
        "test-linux1804-64-qr/debug-xpcshell-nofis-1"
    ]""".lstrip()

    assert expected in output


@pytest.mark.skipif(os.name == "nt", reason="fzf not installed on host")
def test_query(run_mach, capfd):
    cmd = ["try", "fuzzy", "--no-push", "-q", "'source-test-python-taskgraph-tests-py3"]
    assert run_mach(cmd) == 0

    output = capfd.readouterr().out
    print(output)

    # Should only ever mach one task exactly.
    expected = """
    "tasks": [
        "source-test-python-taskgraph-tests-py3"
    ]""".lstrip()

    assert expected in output


if __name__ == "__main__":
    mozunit.main()
