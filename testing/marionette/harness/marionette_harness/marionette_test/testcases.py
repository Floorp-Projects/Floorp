# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
import re
import sys
import time
import unittest
import warnings
import weakref
from unittest.case import SkipTest

import six
from marionette_driver.errors import TimeoutException, UnresponsiveInstanceException
from mozfile import load_source
from mozlog import get_default_logger


# With Python 3 both expectedFailure and unexpectedSuccess are
# available in unittest/case.py but won't work here because both
# do not inherit from BaseException. And that's currently needed
# in our custom test status handling in `run()`.
class expectedFailure(Exception):
    """
    Raise this when a test is expected to fail.

    This is an implementation detail.
    """

    def __init__(self, exc_info):
        super(expectedFailure, self).__init__()
        self.exc_info = exc_info


class unexpectedSuccess(Exception):
    """
    The test was supposed to fail, but it didn't!
    """

    pass


def _wraps_parameterized(func, func_suffix, args, kwargs):
    """Internal: Decorator used in class MetaParameterized."""

    def wrapper(self):
        return func(self, *args, **kwargs)

    wrapper.__name__ = func.__name__ + "_" + str(func_suffix)
    wrapper.__doc__ = "[{0}] {1}".format(func_suffix, func.__doc__)
    return wrapper


class MetaParameterized(type):
    """
    A metaclass that allow a class to use decorators.

    It can be used like :func:`parameterized`
    or :func:`with_parameters` to generate new methods.
    """

    RE_ESCAPE_BAD_CHARS = re.compile(r"[\.\(\) -/]")

    def __new__(cls, name, bases, attrs):
        for k, v in list(attrs.items()):
            if callable(v) and hasattr(v, "metaparameters"):
                for func_suffix, args, kwargs in v.metaparameters:
                    func_suffix = cls.RE_ESCAPE_BAD_CHARS.sub("_", func_suffix)
                    wrapper = _wraps_parameterized(v, func_suffix, args, kwargs)
                    if wrapper.__name__ in attrs:
                        raise KeyError(
                            "{0} is already a defined method on {1}".format(
                                wrapper.__name__, name
                            )
                        )
                    attrs[wrapper.__name__] = wrapper
                del attrs[k]

        return type.__new__(cls, name, bases, attrs)


@six.add_metaclass(MetaParameterized)
class CommonTestCase(unittest.TestCase):
    match_re = None
    failureException = AssertionError
    pydebugger = None

    def __init__(self, methodName, marionette_weakref, fixtures, **kwargs):
        super(CommonTestCase, self).__init__(methodName)
        self.methodName = methodName

        self._marionette_weakref = marionette_weakref
        self.fixtures = fixtures

        self.duration = 0
        self.start_time = 0
        self.expected = kwargs.pop("expected", "pass")
        self.logger = get_default_logger()

    def _enter_pm(self):
        if self.pydebugger:
            self.pydebugger.post_mortem(sys.exc_info()[2])

    def _addSkip(self, result, reason):
        addSkip = getattr(result, "addSkip", None)
        if addSkip is not None:
            addSkip(self, reason)
        else:
            warnings.warn(
                "TestResult has no addSkip method, skips not reported",
                RuntimeWarning,
                2,
            )
            result.addSuccess(self)

    def assertRaisesRegxp(
        self, expected_exception, expected_regexp, callable_obj=None, *args, **kwargs
    ):
        return six.assertRaisesRegex(
            self,
            expected_exception,
            expected_regexp,
            callable_obj=None,
            *args,
            **kwargs
        )

    def run(self, result=None):
        # Bug 967566 suggests refactoring run, which would hopefully
        # mean getting rid of this inner function, which only sits
        # here to reduce code duplication:
        def expected_failure(result, exc_info):
            addExpectedFailure = getattr(result, "addExpectedFailure", None)
            if addExpectedFailure is not None:
                addExpectedFailure(self, exc_info)
            else:
                warnings.warn(
                    "TestResult has no addExpectedFailure method, "
                    "reporting as passes",
                    RuntimeWarning,
                )
                result.addSuccess(self)

        self.start_time = time.time()
        orig_result = result
        if result is None:
            result = self.defaultTestResult()
            startTestRun = getattr(result, "startTestRun", None)
            if startTestRun is not None:
                startTestRun()

        result.startTest(self)

        testMethod = getattr(self, self._testMethodName)
        if getattr(self.__class__, "__unittest_skip__", False) or getattr(
            testMethod, "__unittest_skip__", False
        ):
            # If the class or method was skipped.
            try:
                skip_why = getattr(
                    self.__class__, "__unittest_skip_why__", ""
                ) or getattr(testMethod, "__unittest_skip_why__", "")
                self._addSkip(result, skip_why)
            finally:
                result.stopTest(self)
            self.stop_time = time.time()
            return
        try:
            success = False
            try:
                if self.expected == "fail":
                    try:
                        self.setUp()
                    except Exception:
                        raise expectedFailure(sys.exc_info())
                else:
                    self.setUp()
            except SkipTest as e:
                self._addSkip(result, str(e))
            except (KeyboardInterrupt, UnresponsiveInstanceException):
                raise
            except expectedFailure as e:
                expected_failure(result, e.exc_info)
            except Exception:
                self._enter_pm()
                result.addError(self, sys.exc_info())
            else:
                try:
                    if self.expected == "fail":
                        try:
                            testMethod()
                        except Exception:
                            raise expectedFailure(sys.exc_info())
                        raise unexpectedSuccess
                    else:
                        testMethod()
                except self.failureException:
                    self._enter_pm()
                    result.addFailure(self, sys.exc_info())
                except (KeyboardInterrupt, UnresponsiveInstanceException):
                    raise
                except expectedFailure as e:
                    expected_failure(result, e.exc_info)
                except unexpectedSuccess:
                    addUnexpectedSuccess = getattr(result, "addUnexpectedSuccess", None)
                    if addUnexpectedSuccess is not None:
                        addUnexpectedSuccess(self)
                    else:
                        warnings.warn(
                            "TestResult has no addUnexpectedSuccess method, "
                            "reporting as failures",
                            RuntimeWarning,
                        )
                        result.addFailure(self, sys.exc_info())
                except SkipTest as e:
                    self._addSkip(result, str(e))
                except Exception:
                    self._enter_pm()
                    result.addError(self, sys.exc_info())
                else:
                    success = True
                try:
                    if self.expected == "fail":
                        try:
                            self.tearDown()
                        except Exception:
                            raise expectedFailure(sys.exc_info())
                    else:
                        self.tearDown()
                except (KeyboardInterrupt, UnresponsiveInstanceException):
                    raise
                except expectedFailure as e:
                    expected_failure(result, e.exc_info)
                except Exception:
                    self._enter_pm()
                    result.addError(self, sys.exc_info())
                    success = False
            # Here we could handle doCleanups() instead of calling cleanTest directly
            self.cleanTest()

            if success:
                result.addSuccess(self)

        finally:
            result.stopTest(self)
            if orig_result is None:
                stopTestRun = getattr(result, "stopTestRun", None)
                if stopTestRun is not None:
                    stopTestRun()

    @classmethod
    def match(cls, filename):
        """Determine if the specified filename should be handled by this test class.

        This is done by looking for a match for the filename using cls.match_re.
        """
        if not cls.match_re:
            return False
        m = cls.match_re.match(filename)
        return m is not None

    @classmethod
    def add_tests_to_suite(
        cls,
        mod_name,
        filepath,
        suite,
        testloader,
        marionette,
        fixtures,
        testvars,
        **kwargs
    ):
        """Add all the tests in the specified file to the specified suite."""
        raise NotImplementedError

    @property
    def test_name(self):
        rel_path = None
        if os.path.exists(self.filepath):
            rel_path = self._fix_test_path(self.filepath)

        return "{0} {1}.{2}".format(
            rel_path, self.__class__.__name__, self._testMethodName
        )

    def id(self):
        # TBPL starring requires that the "test name" field of a failure message
        # not differ over time. The test name to be used is passed to
        # mozlog via the test id, so this is overriden to maintain
        # consistency.
        return self.test_name

    def setUp(self):
        # Convert the marionette weakref to an object, just for the
        # duration of the test; this is deleted in tearDown() to prevent
        # a persistent circular reference which in turn would prevent
        # proper garbage collection.
        self.start_time = time.time()
        self.marionette = self._marionette_weakref()
        if self.marionette.session is None:
            self.marionette.start_session()
        self.marionette.timeout.reset()

        super(CommonTestCase, self).setUp()

    def cleanTest(self):
        self._delete_session()

    def _delete_session(self):
        if hasattr(self, "start_time"):
            self.duration = time.time() - self.start_time
        if self.marionette.session is not None:
            try:
                self.marionette.delete_session()
            except IOError:
                # Gecko has crashed?
                pass
        self.marionette = None

    def _fix_test_path(self, path):
        """Normalize a logged test path from the test package."""
        test_path_prefixes = [
            "tests{}".format(os.path.sep),
        ]

        path = os.path.relpath(path)
        for prefix in test_path_prefixes:
            if path.startswith(prefix):
                path = path[len(prefix) :]
                break
        path = path.replace("\\", "/")

        return path


class MarionetteTestCase(CommonTestCase):
    match_re = re.compile(r"test_(.*)\.py$")

    def __init__(
        self, marionette_weakref, fixtures, methodName="runTest", filepath="", **kwargs
    ):
        self.filepath = filepath
        self.testvars = kwargs.pop("testvars", None)

        super(MarionetteTestCase, self).__init__(
            methodName,
            marionette_weakref=marionette_weakref,
            fixtures=fixtures,
            **kwargs
        )

    @classmethod
    def add_tests_to_suite(
        cls,
        mod_name,
        filepath,
        suite,
        testloader,
        marionette,
        fixtures,
        testvars,
        **kwargs
    ):
        # since load_source caches modules, if a module is loaded with the same
        # name as another one the module would just be reloaded.
        #
        # We may end up by finding too many test in a module then since reload()
        # only update the module dict (so old keys are still there!) see
        # https://docs.python.org/2/library/functions.html#reload
        #
        # we get rid of that by removing the module from sys.modules, so we
        # ensure that it will be fully loaded by the imp.load_source call.

        if mod_name in sys.modules:
            del sys.modules[mod_name]

        test_mod = load_source(mod_name, filepath)

        for name in dir(test_mod):
            obj = getattr(test_mod, name)
            if isinstance(obj, six.class_types) and issubclass(obj, unittest.TestCase):
                testnames = testloader.getTestCaseNames(obj)
                for testname in testnames:
                    suite.addTest(
                        obj(
                            weakref.ref(marionette),
                            fixtures,
                            methodName=testname,
                            filepath=filepath,
                            testvars=testvars,
                            **kwargs
                        )
                    )

    def setUp(self):
        super(MarionetteTestCase, self).setUp()
        self.marionette.test_name = self.test_name

    def tearDown(self):
        # In the case no session is active (eg. the application was quit), start
        # a new session for clean-up steps.
        if not self.marionette.session:
            self.marionette.start_session()

        self.marionette.test_name = None

        super(MarionetteTestCase, self).tearDown()

    def wait_for_condition(self, method, timeout=30):
        timeout = float(timeout) + time.time()
        while time.time() < timeout:
            value = method(self.marionette)
            if value:
                return value
            time.sleep(0.5)
        else:
            raise TimeoutException("wait_for_condition timed out")
