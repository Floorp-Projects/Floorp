#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit
import pytest

from mozperftest.script import ScriptInfo, MissingFieldError, ScriptType
from mozperftest.tests.support import (
    EXAMPLE_TEST,
    HERE,
    EXAMPLE_XPCSHELL_TEST,
    EXAMPLE_XPCSHELL_TEST2,
)


def test_scriptinfo_bt():
    info = ScriptInfo(EXAMPLE_TEST)
    assert info["author"] == "N/A"

    display = str(info)
    assert "The description of the example test." in display
    assert info.script_type == ScriptType.browsertime


@pytest.mark.parametrize("script", [EXAMPLE_XPCSHELL_TEST, EXAMPLE_XPCSHELL_TEST2])
def test_scriptinfo_xpcshell(script):
    info = ScriptInfo(script)
    assert info["author"] == "N/A"

    display = str(info)
    assert "The description of the example test." in display
    assert info.script_type == ScriptType.xpcshell


def test_scriptinfo_failure():
    bad_example = HERE / "data" / "failing-samples" / "perftest_doc_failure_example.js"
    with pytest.raises(MissingFieldError):
        ScriptInfo(bad_example)


if __name__ == "__main__":
    mozunit.main()
