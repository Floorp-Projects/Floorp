# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import, print_function

import six
import json
import time
import types

import mozunit
import pytest
from conftest import setup_args
from mozlog.formatters import JSONFormatter
from mozlog.handlers.base import StreamHandler
from mozlog.structuredlog import StructuredLogger
from six import string_types


@pytest.fixture
def logger():
    logger = StructuredLogger("mochitest_message_logger")

    buf = six.StringIO()
    handler = StreamHandler(buf, JSONFormatter())
    logger.add_handler(handler)
    return logger


@pytest.fixture
def get_message_logger(setup_test_harness, logger):
    setup_test_harness(*setup_args)
    runtests = pytest.importorskip("runtests")

    def fake_message(self, action, **extra):
        message = {
            "action": action,
            "time": time.time(),
        }
        if action in ("test_start", "test_end", "test_status"):
            message["test"] = "test_foo.html"

            if action == "test_end":
                message["status"] = "PASS"
                message["expected"] = "PASS"

            elif action == "test_status":
                message["subtest"] = "bar"
                message["status"] = "PASS"

        elif action == "log":
            message["level"] = "INFO"
            message["message"] = "foobar"

        message.update(**extra)
        return self.process_message(message)

    def inner(**kwargs):
        ml = runtests.MessageLogger(logger, **kwargs)

        # Create a convenience function for faking incoming log messages.
        ml.fake_message = types.MethodType(fake_message, ml)
        return ml

    return inner


@pytest.fixture
def assert_actions(logger):
    buf = logger.handlers[0].stream

    def inner(expected):
        if isinstance(expected, string_types):
            expected = [expected]

        lines = buf.getvalue().splitlines()
        actions = [json.loads(l)["action"] for l in lines]
        assert actions == expected
        buf.truncate(0)
        # Python3 will not reposition the buffer position after
        # truncate and will extend the buffer with null bytes.
        # Force the buffer position to the start of the buffer
        # to prevent null bytes from creeping in.
        buf.seek(0)

    return inner


def test_buffering_on(get_message_logger, assert_actions):
    ml = get_message_logger(buffering=True)

    # no buffering initially (outside of test)
    ml.fake_message("log")
    assert_actions(["log"])

    # inside a test buffering is enabled, only 'test_start' logged
    ml.fake_message("test_start")
    ml.fake_message("test_status")
    ml.fake_message("log")
    assert_actions(["test_start"])

    # buffering turned off manually within a test
    ml.fake_message("buffering_off")
    ml.fake_message("test_status")
    ml.fake_message("log")
    assert_actions(["test_status", "log"])

    # buffering turned back on again
    ml.fake_message("buffering_on")
    ml.fake_message("test_status")
    ml.fake_message("log")
    assert_actions([])

    # test end, it failed! All previsouly buffered messages are now logged.
    ml.fake_message("test_end", status="FAIL")
    assert_actions(
        [
            "log",  # "Buffered messages logged"
            "test_status",
            "log",
            "test_status",
            "log",
            "log",  # "Buffered messages finished"
            "test_end",
        ]
    )

    # enabling buffering outside of a test has no affect
    ml.fake_message("buffering_on")
    ml.fake_message("log")
    ml.fake_message("test_status")
    assert_actions(["log", "test_status"])


def test_buffering_off(get_message_logger, assert_actions):
    ml = get_message_logger(buffering=False)

    ml.fake_message("test_start")
    assert_actions(["test_start"])

    # messages logged no matter what the state
    ml.fake_message("test_status")
    ml.fake_message("buffering_off")
    ml.fake_message("log")
    assert_actions(["test_status", "log"])

    # even after a 'buffering_on' action
    ml.fake_message("buffering_on")
    ml.fake_message("test_status")
    ml.fake_message("log")
    assert_actions(["test_status", "log"])

    # no buffer to empty on test fail
    ml.fake_message("test_end", status="FAIL")
    assert_actions(["test_end"])


if __name__ == "__main__":
    mozunit.main()
