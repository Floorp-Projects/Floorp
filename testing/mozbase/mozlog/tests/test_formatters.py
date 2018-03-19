# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
# flake8: noqa

from __future__ import absolute_import, print_function, unicode_literals

import mozunit
import pytest
from io import BytesIO

from mozlog.structuredlog import StructuredLogger
from mozlog.formatters import (
    MachFormatter,
)
from mozlog.handlers import StreamHandler

formatters = {
    'mach': MachFormatter,
}

FORMATS = {
    # A list of tuples consisting of (name, options, expected string).
    'PASS': [
        ('mach', {}, """
 0:00.00 SUITE_START: running 3 tests
 0:00.00 TEST_START: test_foo
 0:00.00 TEST_END: OK
 0:00.00 TEST_START: test_bar
 0:00.00 TEST_END: Test OK. Subtests passed 1/1. Unexpected 0
 0:00.00 TEST_START: test_baz
 0:00.00 TEST_END: FAIL
 0:00.00 SUITE_END

suite 1
~~~~~~~
Ran 4 checks (3 tests, 1 subtests)
Expected results: 4
OK
""".lstrip('\n')),
        ('mach', {'verbose': True}, """
 0:00.00 SUITE_START: running 3 tests
 0:00.00 TEST_START: test_foo
 0:00.00 TEST_END: OK
 0:00.00 TEST_START: test_bar
 0:00.00 TEST_STATUS: a subtest PASS 
 0:00.00 TEST_END: Test OK. Subtests passed 1/1. Unexpected 0
 0:00.00 TEST_START: test_baz
 0:00.00 TEST_END: FAIL
 0:00.00 SUITE_END

suite 1
~~~~~~~
Ran 4 checks (3 tests, 1 subtests)
Expected results: 4
OK
""".lstrip('\n')),
    ],

    'FAIL': [
        ('mach', {}, """
 0:00.00 SUITE_START: running 3 tests
 0:00.00 TEST_START: test_foo
 0:00.00 TEST_END: FAIL, expected PASS
expected 0 got 1
 0:00.00 TEST_START: test_bar
 0:00.00 TEST_STATUS: Test OK. Subtests passed 0/2. Unexpected 2
a subtest
---------
Expected PASS, got FAIL
expected 0 got 1
another subtest
---------------
Expected PASS, got TIMEOUT

 0:00.00 TEST_START: test_baz
 0:00.00 TEST_END: PASS, expected FAIL

 0:00.00 SUITE_END

suite 1
~~~~~~~
Ran 5 checks (3 tests, 2 subtests)
Expected results: 1
Unexpected results: 4
  test: 2 (1 fail, 1 pass)
  subtest: 2 (1 fail, 1 timeout)

Unexpected Logs
---------------
test_foo
  FAIL [Parent]
test_bar
  FAIL a subtest
  TIMEOUT another subtest
test_baz
  PASS expected FAIL [Parent]
""".lstrip('\n')),
        ('mach', {'verbose': True}, """
 0:00.00 SUITE_START: running 3 tests
 0:00.00 TEST_START: test_foo
 0:00.00 TEST_END: FAIL, expected PASS
expected 0 got 1
 0:00.00 TEST_START: test_bar
 0:00.00 TEST_STATUS: a subtest FAIL expected 0 got 1
    SimpleTest.is@SimpleTest/SimpleTest.js:312:5
    @caps/tests/mochitest/test_bug246699.html:53:1
 0:00.00 TEST_STATUS: another subtest TIMEOUT 
 0:00.00 TEST_STATUS: Test OK. Subtests passed 0/2. Unexpected 2
a subtest
---------
Expected PASS, got FAIL
expected 0 got 1
another subtest
---------------
Expected PASS, got TIMEOUT

 0:00.00 TEST_START: test_baz
 0:00.00 TEST_END: PASS, expected FAIL

 0:00.00 SUITE_END

suite 1
~~~~~~~
Ran 5 checks (3 tests, 2 subtests)
Expected results: 1
Unexpected results: 4
  test: 2 (1 fail, 1 pass)
  subtest: 2 (1 fail, 1 timeout)

Unexpected Logs
---------------
test_foo
  FAIL [Parent]
test_bar
  FAIL a subtest
  TIMEOUT another subtest
test_baz
  PASS expected FAIL [Parent]
""".lstrip('\n')),
    ],
}


def ids(test):
    ids = []
    for value in FORMATS[test]:
        args = ", ".join(["{}={}".format(k, v) for k, v in value[1].items()])
        if args:
            args = "-{}".format(args)
        ids.append("{}{}".format(value[0], args))
    return ids


@pytest.fixture(autouse=True)
def timestamp(monkeypatch):

    def fake_time(*args, **kwargs):
        return 0

    monkeypatch.setattr(MachFormatter, '_time', fake_time)


@pytest.mark.parametrize("name,opts,expected", FORMATS['PASS'],
                         ids=ids('PASS'))
def test_pass(name, opts, expected):
    buf = BytesIO()
    fmt = formatters[name](**opts)
    logger = StructuredLogger('test_logger')
    logger.add_handler(StreamHandler(buf, fmt))

    logger.suite_start(['test_foo', 'test_bar', 'test_baz'])
    logger.test_start('test_foo')
    logger.test_end('test_foo', 'OK')
    logger.test_start('test_bar')
    logger.test_status('test_bar', 'a subtest', 'PASS')
    logger.test_end('test_bar', 'OK')
    logger.test_start('test_baz')
    logger.test_end('test_baz', 'FAIL', 'FAIL', 'expected 0 got 1')
    logger.suite_end()

    result = buf.getvalue()
    print("Dumping result for copy/paste:")
    print(result)
    assert result == expected


@pytest.mark.parametrize("name,opts,expected", FORMATS['FAIL'],
                         ids=ids('FAIL'))
def test_fail(name, opts, expected):
    stack = """
    SimpleTest.is@SimpleTest/SimpleTest.js:312:5
    @caps/tests/mochitest/test_bug246699.html:53:1
""".strip('\n')

    buf = BytesIO()
    fmt = formatters[name](**opts)
    logger = StructuredLogger('test_logger')
    logger.add_handler(StreamHandler(buf, fmt))

    logger.suite_start(['test_foo', 'test_bar', 'test_baz'])
    logger.test_start('test_foo')
    logger.test_end('test_foo', 'FAIL', 'PASS', 'expected 0 got 1')
    logger.test_start('test_bar')
    logger.test_status('test_bar', 'a subtest', 'FAIL', 'PASS', 'expected 0 got 1', stack)
    logger.test_status('test_bar', 'another subtest', 'TIMEOUT')
    logger.test_end('test_bar', 'OK')
    logger.test_start('test_baz')
    logger.test_end('test_baz', 'PASS', 'FAIL')
    logger.suite_end()

    result = buf.getvalue()
    print("Dumping result for copy/paste:")
    print(result)
    assert result == expected


if __name__ == '__main__':
    mozunit.main()
