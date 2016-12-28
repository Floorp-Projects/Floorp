# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import functools
import sys
import types

from .errors import (
    _ExpectedFailure,
    _UnexpectedSuccess,
    SkipTest,
)


def expectedFailure(func):
    """Decorator which marks a test as expected fail."""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        try:
            func(*args, **kwargs)
        except Exception:
            raise _ExpectedFailure(sys.exc_info())
        raise _UnexpectedSuccess
    return wrapper


def parameterized(func_suffix, *args, **kwargs):
    r"""Decorator which generates methods given a base method and some data.

    **func_suffix** is used as a suffix for the new created method and must be
    unique given a base method. if **func_suffix** countains characters that
    are not allowed in normal python function name, these characters will be
    replaced with "_".

    This decorator can be used more than once on a single base method. The class
    must have a metaclass of :class:`MetaParameterized`.

    Example::

      # This example will generate two methods:
      #
      # - MyTestCase.test_it_1
      # - MyTestCase.test_it_2
      #
      class MyTestCase(MarionetteTestCase):
          @parameterized("1", 5, named='name')
          @parameterized("2", 6, named='name2')
          def test_it(self, value, named=None):
              print value, named

    :param func_suffix: will be used as a suffix for the new method
    :param \*args: arguments to pass to the new method
    :param \*\*kwargs: named arguments to pass to the new method
    """
    def wrapped(func):
        if not hasattr(func, 'metaparameters'):
            func.metaparameters = []
        func.metaparameters.append((func_suffix, args, kwargs))
        return func
    return wrapped


def run_if_e10s(target):
    """Decorator which runs a test if e10s mode is active."""
    def wrapper(self, *args, **kwargs):
        with self.marionette.using_context('chrome'):
            multi_process_browser = self.marionette.execute_script("""
            try {
              return Services.appinfo.browserTabsRemoteAutostart;
            } catch (e) {
              return false;
            }""")

        if not multi_process_browser:
            raise SkipTest('skipping due to e10s is disabled')
        return target(self, *args, **kwargs)
    return wrapper


def skip(reason):
    """Decorator which unconditionally skips a test."""
    def decorator(test_item):
        if not isinstance(test_item, (type, types.ClassType)):
            @functools.wraps(test_item)
            def skip_wrapper(*args, **kwargs):
                raise SkipTest(reason)
            test_item = skip_wrapper

        test_item.__unittest_skip__ = True
        test_item.__unittest_skip_why__ = reason
        return test_item
    return decorator


def skip_if_chrome(target):
    """Decorator which skips a test if chrome context is active."""
    def wrapper(self, *args, **kwargs):
        if self.marionette._send_message("getContext", key="value") == "chrome":
            raise SkipTest("skipping test in chrome context")
        return target(self, *args, **kwargs)
    return wrapper


def skip_if_desktop(target):
    """Decorator which skips a test if run on desktop."""
    def wrapper(self, *args, **kwargs):
        if self.marionette.session_capabilities.get('browserName') == 'firefox':
            raise SkipTest('skipping due to desktop')
        return target(self, *args, **kwargs)
    return wrapper


def skip_if_e10s(target):
    """Decorator which skips a test if e10s mode is active."""
    def wrapper(self, *args, **kwargs):
        with self.marionette.using_context('chrome'):
            multi_process_browser = self.marionette.execute_script("""
            try {
              return Services.appinfo.browserTabsRemoteAutostart;
            } catch (e) {
              return false;
            }""")

        if multi_process_browser:
            raise SkipTest('skipping due to e10s')
        return target(self, *args, **kwargs)
    return wrapper


def skip_if_mobile(target):
    """Decorator which skips a test if run on mobile."""
    def wrapper(self, *args, **kwargs):
        if self.marionette.session_capabilities.get('browserName') == 'fennec':
            raise SkipTest('skipping due to fennec')
        return target(self, *args, **kwargs)
    return wrapper


def skip_unless_browser_pref(pref, predicate=bool):
    """Decorator which skips a test based on the value of a browser preference.

    :param pref: the preference name
    :param predicate: a function that should return false to skip the test.
                      The function takes one parameter, the preference value.
                      Defaults to the python built-in bool function.

    Note that the preference must exist, else a failure is raised.

    Example: ::

      class TestSomething(MarionetteTestCase):
          @skip_unless_browser_pref("accessibility.tabfocus",
                                    lambda value: value >= 7)
          def test_foo(self):
              pass  # test implementation here
    """
    def wrapper(target):
        @functools.wraps(target)
        def wrapped(self, *args, **kwargs):
            value = self.marionette.get_pref(pref)
            if value is None:
                self.fail("No such browser preference: {0!r}".format(pref))
            if not predicate(value):
                raise SkipTest("browser preference {0!r}: {1!r}".format((pref, value)))
            return target(self, *args, **kwargs)
        return wrapped
    return wrapper


def skip_unless_protocol(predicate):
    """Decorator which skips a test if the predicate does not match the current protocol level."""
    def decorator(test_item):
        @functools.wraps(test_item)
        def skip_wrapper(self):
            level = self.marionette.client.protocol
            if not predicate(level):
                raise SkipTest('skipping because protocol level is {}'.format(level))
            return test_item(self)
        return skip_wrapper
    return decorator


def with_parameters(parameters):
    """Decorator which generates methods given a base method and some data.

    Acts like :func:`parameterized`, but define all methods in one call.

    Example::

      # This example will generate two methods:
      #
      # - MyTestCase.test_it_1
      # - MyTestCase.test_it_2
      #

      DATA = [("1", [5], {'named':'name'}), ("2", [6], {'named':'name2'})]

      class MyTestCase(MarionetteTestCase):
          @with_parameters(DATA)
          def test_it(self, value, named=None):
              print value, named

    :param parameters: list of tuples (**func_suffix**, **args**, **kwargs**)
                       defining parameters like in :func:`todo`.
    """
    def wrapped(func):
        func.metaparameters = parameters
        return func
    return wrapped
