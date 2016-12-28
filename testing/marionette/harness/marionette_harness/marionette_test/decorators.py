# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import functools
import types

from unittest.case import (
    SkipTest,
)


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


def run_if_e10s(reason):
    """Decorator which runs a test if e10s mode is active."""
    def decorator(test_item):
        if not isinstance(test_item, types.FunctionType):
            raise Exception('Decorator only supported for functions')

        @functools.wraps(test_item)
        def skip_wrapper(self, *args, **kwargs):
            with self.marionette.using_context('chrome'):
                multi_process_browser = not self.marionette.execute_script("""
                    try {
                      return Services.appinfo.browserTabsRemoteAutostart;
                    } catch (e) {
                      return false;
                    }
                """)
                if multi_process_browser:
                    raise SkipTest(reason)
            return test_item(self, *args, **kwargs)
        return skip_wrapper
    return decorator


def skip_if_chrome(reason):
    """Decorator which skips a test if chrome context is active."""
    def decorator(test_item):
        if not isinstance(test_item, types.FunctionType):
            raise Exception('Decorator only supported for functions')

        @functools.wraps(test_item)
        def skip_wrapper(self, *args, **kwargs):
            if self.marionette._send_message('getContext', key='value') == 'chrome':
                raise SkipTest(reason)
            return test_item(self, *args, **kwargs)
        return skip_wrapper
    return decorator


def skip_if_desktop(reason):
    """Decorator which skips a test if run on desktop."""
    def decorator(test_item):
        if not isinstance(test_item, types.FunctionType):
            raise Exception('Decorator only supported for functions')

        @functools.wraps(test_item)
        def skip_wrapper(self, *args, **kwargs):
            if self.marionette.session_capabilities.get('browserName') == 'firefox':
                raise SkipTest(reason)
            return test_item(self, *args, **kwargs)
        return skip_wrapper
    return decorator


def skip_if_e10s(reason):
    """Decorator which skips a test if e10s mode is active."""
    def decorator(test_item):
        if not isinstance(test_item, types.FunctionType):
            raise Exception('Decorator only supported for functions')

        @functools.wraps(test_item)
        def skip_wrapper(self, *args, **kwargs):
            with self.marionette.using_context('chrome'):
                multi_process_browser = self.marionette.execute_script("""
                    try {
                      return Services.appinfo.browserTabsRemoteAutostart;
                    } catch (e) {
                      return false;
                    }
                """)
                if multi_process_browser:
                    raise SkipTest(reason)
            return test_item(self, *args, **kwargs)
        return skip_wrapper
    return decorator


def skip_if_mobile(reason):
    """Decorator which skips a test if run on mobile."""
    def decorator(test_item):
        if not isinstance(test_item, types.FunctionType):
            raise Exception('Decorator only supported for functions')

        @functools.wraps(test_item)
        def skip_wrapper(self, *args, **kwargs):
            if self.marionette.session_capabilities.get('browserName') == 'fennec':
                raise SkipTest(reason)
            return test_item(self, *args, **kwargs)
        return skip_wrapper
    return decorator


def skip_unless_browser_pref(reason, pref, predicate=bool):
    """Decorator which skips a test based on the value of a browser preference.

    :param reason: Message describing why the test need to be skipped.
    :param pref: the preference name
    :param predicate: a function that should return false to skip the test.
                      The function takes one parameter, the preference value.
                      Defaults to the python built-in bool function.

    Note that the preference must exist, else a failure is raised.

    Example: ::

      class TestSomething(MarionetteTestCase):
          @skip_unless_browser_pref("Sessionstore needs to be enabled for crashes",
                                    "browser.sessionstore.resume_from_crash",
                                    lambda value: value is True,
                                    )
          def test_foo(self):
              pass  # test implementation here

    """
    def decorator(test_item):
        if not isinstance(test_item, types.FunctionType):
            raise Exception('Decorator only supported for functions')
        if not callable(predicate):
            raise ValueError('predicate must be callable')

        @functools.wraps(test_item)
        def skip_wrapper(self, *args, **kwargs):
            value = self.marionette.get_pref(pref)
            if value is None:
                self.fail("No such browser preference: {0!r}".format(pref))
            if not predicate(value):
                raise SkipTest(reason)
            return test_item(self, *args, **kwargs)
        return skip_wrapper
    return decorator


def skip_unless_protocol(reason, predicate):
    """Decorator which skips a test if the predicate does not match the current protocol level."""
    def decorator(test_item):
        if not isinstance(test_item, types.FunctionType):
            raise Exception('Decorator only supported for functions')
        if not callable(predicate):
            raise ValueError('predicate must be callable')

        @functools.wraps(test_item)
        def skip_wrapper(self, *args, **kwargs):
            level = self.marionette.client.protocol
            if not predicate(level):
                raise SkipTest(reason)
            return test_item(self, *args, **kwargs)
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
