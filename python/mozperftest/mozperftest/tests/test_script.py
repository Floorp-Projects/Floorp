#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit
import pytest

from mozperftest.script import (
    BadOptionTypeError,
    MissingFieldError,
    ParseError,
    ScriptInfo,
    ScriptType,
)
from mozperftest.tests.support import (
    EXAMPLE_MOCHITEST_TEST,
    EXAMPLE_MOCHITEST_TEST2,
    EXAMPLE_TEST,
    EXAMPLE_XPCSHELL_TEST,
    EXAMPLE_XPCSHELL_TEST2,
    HERE,
    temp_file,
)


def check_options(info):
    assert info["options"]["default"]["perfherder"]
    assert info["options"]["linux"]["perfherder_metrics"] == [
        {"name": "speed", "unit": "bps_lin"}
    ]
    assert info["options"]["win"]["perfherder_metrics"] == [
        {"name": "speed", "unit": "bps_win"}
    ]
    assert info["options"]["mac"]["perfherder_metrics"] == [
        {"name": "speed", "unit": "bps_mac"}
    ]


def test_scriptinfo_bt():
    info = ScriptInfo(EXAMPLE_TEST)
    assert info["author"] == "N/A"
    display = str(info)
    assert "The description of the example test." in display
    assert info.script_type == ScriptType.browsertime
    check_options(info)


@pytest.mark.parametrize("script", [EXAMPLE_MOCHITEST_TEST, EXAMPLE_MOCHITEST_TEST2])
def test_scriptinfo_mochitest(script):
    info = ScriptInfo(script)
    assert info["author"] == "N/A"

    display = str(info)
    assert "N/A" in display
    assert "Performance Team" in display
    assert "Test test" in display
    assert info.script_type == ScriptType.mochitest

    assert info["options"]["default"]["manifest"] == "mochitest-common.ini"
    assert info["options"]["default"]["manifest_flavor"] == "plain"
    assert info["options"]["default"]["perfherder_metrics"] == [
        {"name": "Registration", "unit": "ms"}
    ]


def test_scriptinfo_mochitest_missing_perfmetadata():
    with temp_file(name="sample.html", content="<html></html>") as temp:
        with pytest.raises(ParseError) as exc_info:
            ScriptInfo(temp)
        assert "MissingPerfMetadata" in str(exc_info.value)


@pytest.mark.parametrize("script", [EXAMPLE_XPCSHELL_TEST, EXAMPLE_XPCSHELL_TEST2])
def test_scriptinfo_xpcshell(script):
    info = ScriptInfo(script)
    assert info["author"] == "N/A"

    display = str(info)
    assert "The description of the example test." in display
    assert info.script_type == ScriptType.xpcshell
    check_options(info)


def test_scriptinfo_failure():
    bad_example = HERE / "data" / "failing-samples" / "perftest_doc_failure_example.js"
    with pytest.raises(MissingFieldError):
        ScriptInfo(bad_example)


def test_parserror():
    exc = Exception("original")
    error = ParseError("script", exc)
    assert error.exception is exc
    assert "original" in str(error)


def test_update_args():
    args = {"perfherder_metrics": [{"name": "yey"}]}
    info = ScriptInfo(EXAMPLE_TEST)
    new_args = info.update_args(**args)

    # arguments should not be overriden
    assert new_args["perfherder_metrics"] == [{"name": "yey"}]

    # arguments in platform-specific options should
    # override default options
    assert new_args["verbose"]


def test_update_args_metrics_list_failure():
    args = {"perfherder_metrics": "yey"}
    info = ScriptInfo(EXAMPLE_TEST)

    with pytest.raises(BadOptionTypeError):
        info.update_args(**args)


def test_update_args_metrics_json_failure():
    args = {"perfherder_metrics": ["yey"]}
    info = ScriptInfo(EXAMPLE_TEST)

    with pytest.raises(BadOptionTypeError):
        info.update_args(**args)


if __name__ == "__main__":
    mozunit.main()
