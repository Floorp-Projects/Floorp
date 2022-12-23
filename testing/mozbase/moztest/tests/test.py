# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import math
import time

import mozunit
import pytest
from moztest.results import TestContext, TestResult, TestResultCollection


def test_results():
    with pytest.raises(AssertionError):
        TestResult("test", result_expected="hello")
    t = TestResult("test")
    with pytest.raises(ValueError):
        t.finish(result="good bye")


def test_time():
    now = time.time()
    t = TestResult("test")
    time.sleep(1)
    t.finish("PASS")
    duration = time.time() - now
    assert math.fabs(duration - t.duration) < 1


def test_custom_time():
    t = TestResult("test", time_start=0)
    t.finish(result="PASS", time_end=1000)
    assert t.duration == 1000


def test_unique_contexts():
    c1 = TestContext("host1")
    c2 = TestContext("host2")
    c3 = TestContext("host2")
    c4 = TestContext("host1")

    t1 = TestResult("t1", context=c1)
    t2 = TestResult("t2", context=c2)
    t3 = TestResult("t3", context=c3)
    t4 = TestResult("t4", context=c4)

    collection = TestResultCollection("tests")
    collection.extend([t1, t2, t3, t4])

    assert len(collection.contexts) == 2


if __name__ == "__main__":
    mozunit.main()
