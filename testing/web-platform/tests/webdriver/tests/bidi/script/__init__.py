from typing import Any

from .. import any_int, any_string


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
