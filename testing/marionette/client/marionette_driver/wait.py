# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import collections
import errors
import sys
import time

DEFAULT_TIMEOUT = 5
DEFAULT_INTERVAL = 0.1


class Wait(object):
    """An explicit conditional utility class for waiting until a condition
    evaluates to true or not null.

    This will repeatedly evaluate a condition in anticipation for a
    truthy return value, or its timeout to expire, or its waiting
    predicate to become true.

    A `Wait` instance defines the maximum amount of time to wait for a
    condition, as well as the frequency with which to check the
    condition.  Furthermore, the user may configure the wait to ignore
    specific types of exceptions whilst waiting, such as
    `errors.NoSuchElementException` when searching for an element on
    the page.

    """

    def __init__(self, marionette, timeout=None,
                 interval=DEFAULT_INTERVAL, ignored_exceptions=None,
                 clock=None):
        """Configure the Wait instance to have a custom timeout, interval, and
        list of ignored exceptions.  Optionally a different time
        implementation than the one provided by the standard library
        (time) can also be provided.

        Sample usage::

            # Wait 30 seconds for window to open, checking for its presence once
            # every 5 seconds.
            wait = Wait(marionette, timeout=30, interval=5,
                        ignored_exceptions=errors.NoSuchWindowException)
            window = wait.until(lambda m: m.switch_to_window(42))

        :param marionette: The input value to be provided to
            conditions, usually a Marionette instance.

        :param timeout: How long to wait for the evaluated condition
            to become true.  The default timeout is the `timeout`
            property on the `Marionette` object if set, or
            `wait.DEFAULT_TIMEOUT`.

        :param interval: How often the condition should be evaluated.
            In reality the interval may be greater as the cost of
            evaluating the condition function. If that is not the case the
            interval for the next condition function call is shortend to keep
            the original interval sequence as best as possible.
            The default polling interval is `wait.DEFAULT_INTERVAL`.

        :param ignored_exceptions: Ignore specific types of exceptions
            whilst waiting for the condition.  Any exceptions not
            whitelisted will be allowed to propagate, terminating the
            wait.

        :param clock: Allows overriding the use of the runtime's
            default time library.  See `wait.SystemClock` for
            implementation details.

        """

        self.marionette = marionette
        self.timeout = timeout or (self.marionette.timeout and
                                   self.marionette.timeout / 1000.0) or DEFAULT_TIMEOUT
        self.clock = clock or SystemClock()
        self.end = self.clock.now + self.timeout
        self.interval = interval

        exceptions = []
        if ignored_exceptions is not None:
            if isinstance(ignored_exceptions, collections.Iterable):
                exceptions.extend(iter(ignored_exceptions))
            else:
                exceptions.append(ignored_exceptions)
        self.exceptions = tuple(set(exceptions))

    def until(self, condition, is_true=None, message=""):
        """Repeatedly runs condition until its return value evaluates to true,
        or its timeout expires or the predicate evaluates to true.

        This will poll at the given interval until the given timeout
        is reached, or the predicate or conditions returns true.  A
        condition that returns null or does not evaluate to true will
        fully elapse its timeout before raising an
        `errors.TimeoutException`.

        If an exception is raised in the condition function and it's
        not ignored, this function will raise immediately.  If the
        exception is ignored, it will continue polling for the
        condition until it returns successfully or a
        `TimeoutException` is raised.

        :param condition: A callable function whose return value will
            be returned by this function if it evaluates to true.

        :param is_true: An optional predicate that will terminate and
            return when it evaluates to False.  It should be a
            function that will be passed clock and an end time.  The
            default predicate will terminate a wait when the clock
            elapses the timeout.

        :param message: An optional message to include in the
            exception's message if this function times out.

        """

        rv = None
        last_exc = None
        until = is_true or until_pred
        start = self.clock.now

        while not until(self.clock, self.end):
            try:
                next = self.clock.now + self.interval
                rv = condition(self.marionette)
            except (KeyboardInterrupt, SystemExit):
                raise
            except self.exceptions:
                last_exc = sys.exc_info()

            # Re-adjust the interval depending on how long the callback
            # took to evaluate the condition
            interval_new = max(next - self.clock.now, 0)

            if not rv:
                self.clock.sleep(interval_new)
                continue

            if rv is not None:
                return rv

            self.clock.sleep(interval_new)

        if message:
            message = " with message: %s" % message

        raise errors.TimeoutException(
            "Timed out after %s seconds%s" %
            (round((self.clock.now - start), 1), message if message else ""),
            cause=last_exc)


def until_pred(clock, end):
    return clock.now >= end


class SystemClock(object):
    def __init__(self):
        self._time = time

    def sleep(self, duration):
        self._time.sleep(duration)

    @property
    def now(self):
        return self._time.time()
