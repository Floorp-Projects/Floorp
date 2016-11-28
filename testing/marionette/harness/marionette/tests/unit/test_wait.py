# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import time

from marionette_driver import errors
from marionette_driver import wait
from marionette_driver.wait import Wait

from marionette import MarionetteTestCase


class TickingClock(object):

    def __init__(self, incr=1):
        self.ticks = 0
        self.increment = incr

    def sleep(self, dur=None):
        dur = dur if dur is not None else self.increment
        self.ticks += dur

    @property
    def now(self):
        return self.ticks


class SequenceClock(object):

    def __init__(self, times):
        self.times = times
        self.i = 0

    @property
    def now(self):
        if len(self.times) > self.i:
            self.i += 1
        return self.times[self.i - 1]

    def sleep(self, dur):
        pass


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


def now(clock, end):
    return True


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
        self.assertGreater(end, 0)

    def test_time_now(self):
        self.assertIsNotNone(self.clock.now)


class FormalWaitTest(MarionetteTestCase):

    def setUp(self):
        super(FormalWaitTest, self).setUp()
        self.m = MockMarionette()
        self.m.timeout = 123

    def test_construction_with_custom_timeout(self):
        wt = Wait(self.m, timeout=42)
        self.assertEqual(wt.timeout, 42)

    def test_construction_with_custom_interval(self):
        wt = Wait(self.m, interval=42)
        self.assertEqual(wt.interval, 42)

    def test_construction_with_custom_clock(self):
        c = TickingClock(1)
        wt = Wait(self.m, clock=c)
        self.assertEqual(wt.clock, c)

    def test_construction_with_custom_exception(self):
        wt = Wait(self.m, ignored_exceptions=Exception)
        self.assertIn(Exception, wt.exceptions)
        self.assertEqual(len(wt.exceptions), 1)

    def test_construction_with_custom_exception_list(self):
        exc = [Exception, ValueError]
        wt = Wait(self.m, ignored_exceptions=exc)
        for e in exc:
            self.assertIn(e, wt.exceptions)
        self.assertEqual(len(wt.exceptions), len(exc))

    def test_construction_with_custom_exception_tuple(self):
        exc = (Exception, ValueError)
        wt = Wait(self.m, ignored_exceptions=exc)
        for e in exc:
            self.assertIn(e, wt.exceptions)
        self.assertEqual(len(wt.exceptions), len(exc))

    def test_duplicate_exceptions(self):
        wt = Wait(self.m, ignored_exceptions=[Exception, Exception])
        self.assertIn(Exception, wt.exceptions)
        self.assertEqual(len(wt.exceptions), 1)

    def test_default_timeout(self):
        self.assertEqual(wait.DEFAULT_TIMEOUT, 5)

    def test_default_interval(self):
        self.assertEqual(wait.DEFAULT_INTERVAL, 0.1)

    def test_end_property(self):
        wt = Wait(self.m)
        self.assertIsNotNone(wt.end)

    def test_marionette_property(self):
        wt = Wait(self.m)
        self.assertEqual(wt.marionette, self.m)

    def test_clock_property(self):
        wt = Wait(self.m)
        self.assertIsInstance(wt.clock, wait.SystemClock)

    def test_timeout_uses_default_if_marionette_timeout_is_none(self):
        self.m.timeout = None
        wt = Wait(self.m)
        self.assertEqual(wt.timeout, wait.DEFAULT_TIMEOUT)


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
        self.wt = Wait(self.m, timeout=10, interval=1, clock=self.clock)

    def test_true(self):
        r = self.wt.until(lambda x: x.true())
        self.assertTrue(r)
        self.assertEqual(self.clock.ticks, 0)

    def test_true_within_timeout(self):
        r = self.wt.until(lambda x: x.true(wait=5))
        self.assertTrue(r)
        self.assertEqual(self.clock.ticks, 4)

    def test_timeout(self):
        with self.assertRaises(errors.TimeoutException):
            r = self.wt.until(lambda x: x.true(wait=15))
        self.assertEqual(self.clock.ticks, 10)

    def test_exception_raises_immediately(self):
        with self.assertRaises(TypeError):
            self.wt.until(lambda x: x.exception(e=TypeError))
        self.assertEqual(self.clock.ticks, 0)

    def test_ignored_exception(self):
        self.wt.exceptions = (TypeError,)
        with self.assertRaises(errors.TimeoutException):
            self.wt.until(lambda x: x.exception(e=TypeError))

    def test_ignored_exception_wrapped_in_timeoutexception(self):
        self.wt.exceptions = (TypeError,)

        exc = None
        try:
            self.wt.until(lambda x: x.exception(e=TypeError))
        except Exception as e:
            exc = e

        s = str(exc)
        self.assertIsNotNone(exc)
        self.assertIsInstance(exc, errors.TimeoutException)
        self.assertIn(", caused by {0!r}".format(TypeError), s)
        self.assertIn("self.wt.until(lambda x: x.exception(e=TypeError))", s)

    def test_ignored_exception_after_timeout_is_not_raised(self):
        with self.assertRaises(errors.TimeoutException):
            r = self.wt.until(lambda x: x.exception(wait=15))
        self.assertEqual(self.clock.ticks, 10)

    def test_keyboard_interrupt(self):
        with self.assertRaises(KeyboardInterrupt):
            self.wt.until(lambda x: x.exception(e=KeyboardInterrupt))

    def test_system_exit(self):
        with self.assertRaises(SystemExit):
            self.wt.until(lambda x: x.exception(SystemExit))

    def test_true_condition_returns_immediately(self):
        r = self.wt.until(lambda x: x.true())
        self.assertIsInstance(r, bool)
        self.assertTrue(r)
        self.assertEqual(self.clock.ticks, 0)

    def test_value(self):
        r = self.wt.until(lambda x: "foo")
        self.assertEqual(r, "foo")
        self.assertEqual(self.clock.ticks, 0)

    def test_custom_predicate(self):
        r = self.wt.until(lambda x: x.true(wait=2), is_true=at_third_attempt)
        self.assertTrue(r)
        self.assertEqual(self.clock.ticks, 1)

    def test_custom_predicate_times_out(self):
        with self.assertRaises(errors.TimeoutException):
            self.wt.until(lambda x: x.true(wait=4), is_true=at_third_attempt)

        self.assertEqual(self.clock.ticks, 2)

    def test_timeout_elapsed_duration(self):
        with self.assertRaisesRegexp(errors.TimeoutException,
                                     "Timed out after 2.0 seconds"):
            self.wt.until(lambda x: x.true(wait=4), is_true=at_third_attempt)

    def test_timeout_elapsed_rounding(self):
        wt = Wait(self.m, clock=SequenceClock([1, 0.01, 1]), timeout=0)
        with self.assertRaisesRegexp(errors.TimeoutException,
                                     "Timed out after 1.0 seconds"):
            wt.until(lambda x: x.true(), is_true=now)

    def test_timeout_elapsed_interval_by_delayed_condition_return(self):
        def callback(mn):
            self.clock.sleep(11)
            return mn.false()

        with self.assertRaisesRegexp(errors.TimeoutException,
                                     "Timed out after 11.0 seconds"):
            self.wt.until(callback)
        # With a delayed conditional return > timeout, only 1 iteration is
        # possible
        self.assertEqual(self.m.waited, 1)

    def test_timeout_with_delayed_condition_return(self):
        def callback(mn):
            self.clock.sleep(.5)
            return mn.false()

        with self.assertRaisesRegexp(errors.TimeoutException,
                                     "Timed out after 10.0 seconds"):
            self.wt.until(callback)
        # With a delayed conditional return < interval, 10 iterations should be
        # possible
        self.assertEqual(self.m.waited, 10)

    def test_timeout_interval_shorter_than_delayed_condition_return(self):
        def callback(mn):
            self.clock.sleep(2)
            return mn.false()

        with self.assertRaisesRegexp(errors.TimeoutException,
                                     "Timed out after 10.0 seconds"):
            self.wt.until(callback)
        # With a delayed return of the conditional which takes twice that long than the interval,
        # half of the iterations should be possible
        self.assertEqual(self.m.waited, 5)

    def test_message(self):
        self.wt.exceptions = (TypeError,)
        exc = None
        try:
            self.wt.until(lambda x: x.exception(e=TypeError), message="hooba")
        except errors.TimeoutException as e:
            exc = e

        result = str(exc)
        self.assertIn("seconds with message: hooba, caused by", result)

    def test_no_message(self):
        self.wt.exceptions = (TypeError,)
        exc = None
        try:
            self.wt.until(lambda x: x.exception(e=TypeError), message="")
        except errors.TimeoutException as e:
            exc = e

        result = str(exc)
        self.assertIn("seconds, caused by", result)

    def test_message_has_none_as_its_value(self):
        self.wt.exceptions = (TypeError,)
        exc = None
        try:
            self.wt.until(False, None, None)
        except errors.TimeoutException as e:
            exc = e

        result = str(exc)
        self.assertNotIn("with message:", result)
        self.assertNotIn("secondsNone", result)
