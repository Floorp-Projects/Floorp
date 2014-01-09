# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
import time
import sys

import errors
import wait

from marionette_test import MarionetteTestCase
from wait import Wait

class TickingClock(object):
    def __init__(self, incr=1):
        self.ticks = 0
        self.increment = incr

    def sleep(self, dur):
        self.ticks += self.increment

    @property
    def now(self):
        return self.ticks

class MockMarionette(object):
    def __init__(self):
        self.waited = 0

    def exception(self, e=None, wait=1):
        self.wait()
        if self.waited == wait:
            if e is None:
                e = Exception
            raise e

    def true(self, wait=1):
        self.wait()
        if self.waited == wait:
            return True
        return None

    def false(self, wait=1):
        self.wait()
        return False

    def none(self, wait=1):
        self.wait()
        return None

    def value(self, value, wait=1):
        self.wait()
        if self.waited == wait:
            return value
        return None

    def wait(self):
        self.waited += 1

def at_third_attempt(clock, end):
    return clock.now == 2

class SystemClockTest(MarionetteTestCase):
    def setUp(self):
        super(SystemClockTest, self).setUp()
        self.clock = wait.SystemClock()

    def test_construction_initializes_time(self):
        self.assertEqual(self.clock._time, time)

    def test_sleep(self):
        start = time.time()
        self.clock.sleep(0.1)
        end = time.time() - start

        self.assertGreaterEqual(end, 0.1)

    def test_time_now(self):
        self.assertIsNotNone(self.clock.now)

class FormalWaitTest(MarionetteTestCase):
    def test_construction_with_custom_timeout(self):
        w = Wait(None, timeout=42)
        self.assertEqual(w.timeout, 42)

    def test_construction_with_custom_interval(self):
        w = Wait(None, interval=42)
        self.assertEqual(w.interval, 42)

    def test_construction_with_custom_clock(self):
        c = TickingClock(1)
        w = Wait(None, clock=c)
        self.assertEqual(w.clock, c)

    def test_construction_with_custom_exception(self):
        w = Wait(None, ignored_exceptions=Exception)
        self.assertIn(Exception, w.exceptions)
        self.assertEqual(len(w.exceptions), 1)

    def test_construction_with_custom_exception_list(self):
        exc = [Exception, ValueError]
        w = Wait(None, ignored_exceptions=exc)
        for e in exc:
            self.assertIn(e, w.exceptions)
        self.assertEqual(len(w.exceptions), len(exc))

    def test_construction_with_custom_exception_tuple(self):
        exc = (Exception, ValueError)
        w = Wait(None, ignored_exceptions=exc)
        for e in exc:
            self.assertIn(e, w.exceptions)
        self.assertEqual(len(w.exceptions), len(exc))

    def test_duplicate_exceptions(self):
        w = Wait(None, ignored_exceptions=[Exception, Exception])
        self.assertIn(Exception, w.exceptions)
        self.assertEqual(len(w.exceptions), 1)

    def test_default_timeout(self):
        self.assertEqual(wait.DEFAULT_TIMEOUT, 5)

    def test_default_interval(self):
        self.assertEqual(wait.DEFAULT_INTERVAL, 0.1)

    def test_end_property(self):
        w = Wait(None)
        self.assertIsNotNone(w.end)

    def test_marionette_property(self):
        marionette = "cheddar"
        w = Wait(marionette)
        self.assertEqual(w.marionette, marionette)

    def test_clock_property(self):
        w = Wait(None)
        self.assertIsInstance(w.clock, wait.SystemClock)

class PredicatesTest(MarionetteTestCase):
    def test_until(self):
        c = wait.SystemClock()
        self.assertFalse(wait.until_pred(c, sys.maxint))
        self.assertTrue(wait.until_pred(c, 0))

class WaitUntilTest(MarionetteTestCase):
    def setUp(self):
        super(WaitUntilTest, self).setUp()

        self.m = MockMarionette()
        self.clock = TickingClock()
        self.w = Wait(self.m, timeout=10, interval=1, clock=self.clock)

    def test_true(self):
        r = self.w.until(lambda x: x.true())
        self.assertTrue(r)
        self.assertEqual(self.clock.ticks, 0)

    def test_true_within_timeout(self):
        r = self.w.until(lambda x: x.true(wait=5))
        self.assertTrue(r)
        self.assertEqual(self.clock.ticks, 4)

    def test_timeout(self):
        with self.assertRaises(errors.TimeoutException):
            r = self.w.until(lambda x: x.true(wait=15))
        self.assertEqual(self.clock.ticks, 10)

    def test_exception_raises_immediately(self):
        with self.assertRaises(Exception):
            self.w.until(lambda x: x.exception())
        self.assertEqual(self.clock.ticks, 0)

    def test_custom_ignored_exception(self):
        self.w.exceptions = self.w.exceptions + (Exception,)
        with self.assertRaises(Exception):
            self.w.until(lambda x: x.exception(e=Exception))

    def test_ignored_exception_after_timeout_is_not_raised(self):
        with self.assertRaises(errors.TimeoutException):
            r = self.w.until(lambda x: x.exception(wait=15))
        self.assertEqual(self.clock.ticks, 10)

    def test_keyboard_interrupt(self):
        with self.assertRaises(KeyboardInterrupt):
            self.w.until(lambda x: x.exception(e=KeyboardInterrupt))

    def test_system_exit(self):
        with self.assertRaises(SystemExit):
            self.w.until(lambda x: x.exception(SystemExit))

    def test_true_condition_returns_immediately(self):
        r = self.w.until(lambda x: x.true())
        self.assertIsInstance(r, bool)
        self.assertTrue(r)
        self.assertEqual(self.clock.ticks, 0)

    def test_value(self):
        r = self.w.until(lambda x: "foo")
        self.assertEqual(r, "foo")
        self.assertEqual(self.clock.ticks, 0)

    def test_custom_predicate(self):
        r = self.w.until(lambda x: x.true(wait=2), is_true=at_third_attempt)
        self.assertTrue(r)
        self.assertEqual(self.clock.ticks, 1)

    def test_custom_predicate_times_out(self):
        with self.assertRaises(errors.TimeoutException):
            self.w.until(lambda x: x.true(wait=4), is_true=at_third_attempt)

        self.assertEqual(self.clock.ticks, 2)

    def test_timeout_elapsed_duration(self):
        with self.assertRaisesRegexp(errors.TimeoutException,
                                     "Timed out after 2 seconds"):
            self.w.until(lambda x: x.true(wait=4), is_true=at_third_attempt)
