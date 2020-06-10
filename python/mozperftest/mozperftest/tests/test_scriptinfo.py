#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit
import pytest

from mozperftest.scriptinfo import ScriptInfo, METADATA
from mozperftest.tests.support import EXAMPLE_TEST, HERE


def test_scriptinfo():
    info = ScriptInfo(EXAMPLE_TEST)
    assert info["author"] == "N/A"

    display = str(info)
    assert "The description of the example test." in display

    # Ensure that all known fields are in the script info
    assert set(list(info.keys())) - METADATA == set()


def test_scriptinfo_failure():
    bad_example = HERE / "data" / "failing-samples" / "perftest_doc_failure_example.js"
    with pytest.raises(AssertionError):
        ScriptInfo(bad_example)


if __name__ == "__main__":
    mozunit.main()
