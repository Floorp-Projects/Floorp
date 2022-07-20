from typing import Any

from .. import any_int, any_string


def assert_handle(obj, should_contain_handle):
    if should_contain_handle:
        assert "handle" in obj, f"Result should contain `handle`. Actual: {obj}"
        assert isinstance(obj["handle"], str), f"`handle` should be a string, but was {type(obj['handle'])}"
    else:
        assert "handle" not in obj, f"Result should not contain `handle`. Actual: {obj}"


def any_stack_trace(actual: Any) -> None:
    assert type(actual) is dict
    assert "callFrames" in actual
    assert type(actual["callFrames"]) is list
    for actual_frame in actual["callFrames"]:
        any_stack_frame(actual_frame)


def any_stack_frame(actual: Any) -> None:
    assert type(actual) is dict

    assert "columnNumber" in actual
    any_int(actual["columnNumber"])

    assert "functionName" in actual
    any_string(actual["functionName"])

    assert "lineNumber" in actual
    any_int(actual["lineNumber"])

    assert "url" in actual
    any_string(actual["url"])
