#!/usr/bin/env python
# coding=UTF-8

from __future__ import absolute_import

import mozunit
import pytest


@pytest.fixture
def test_log():
    return [
        "01-30 20:15:41.937 E/GeckoAppShell( 1703): >>> "
        "REPORTING UNCAUGHT EXCEPTION FROM THREAD 9 (\"GeckoBackgroundThread\")",
        "01-30 20:15:41.937 E/GeckoAppShell( 1703): java.lang.NullPointerException",
        "01-30 20:15:41.937 E/GeckoAppShell( 1703):"
        "    at org.mozilla.gecko.GeckoApp$21.run(GeckoApp.java:1833)",
        "01-30 20:15:41.937 E/GeckoAppShell( 1703):"
        "    at android.os.Handler.handleCallback(Handler.java:587)"
    ]


def test_uncaught_exception(check_for_java_exception, test_log):
    """Test for an exception which should be caught."""
    assert 1 == check_for_java_exception(test_log)


def test_truncated_exception(check_for_java_exception, test_log):
    """Test for an exception which should be caught which was truncated."""
    truncated_log = list(test_log)
    truncated_log[0], truncated_log[1] = truncated_log[1], truncated_log[0]

    assert 1 == check_for_java_exception(truncated_log)


def test_unchecked_exception(check_for_java_exception, test_log):
    """Test for an exception which should not be caught."""
    passable_log = list(test_log)
    passable_log[0] = "01-30 20:15:41.937 E/GeckoAppShell( 1703):" \
        " >>> NOT-SO-BAD EXCEPTION FROM THREAD 9 (\"GeckoBackgroundThread\")"

    assert 0 == check_for_java_exception(passable_log)


def test_test_name_unicode(check_for_java_exception, test_log):
    """Test that check_for_crashes can handle unicode in dump_directory."""
    assert 1 == check_for_java_exception(test_log,
                                         test_name=u"🍪",
                                         quiet=False)


if __name__ == '__main__':
    mozunit.main()
