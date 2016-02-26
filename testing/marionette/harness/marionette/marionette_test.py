# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import imp
import os
import re
import functools
import sys
import socket
import time
import types
import unittest
import weakref
import warnings


from marionette_driver.errors import (
        MarionetteException, TimeoutException,
        JavascriptException, NoSuchElementException, NoSuchWindowException,
        StaleElementException, ScriptTimeoutException, ElementNotVisibleException,
        NoSuchFrameException, InvalidElementStateException, NoAlertPresentException,
        InvalidCookieDomainException, UnableToSetCookieException, InvalidSelectorException,
        MoveTargetOutOfBoundsException
        )
from marionette_driver.marionette import Marionette
from marionette_driver.wait import Wait
from marionette_driver.expected import element_present, element_not_present
from mozlog import get_default_logger


class SkipTest(Exception):
    """
    Raise this exception in a test to skip it.

    Usually you can use TestResult.skip() or one of the skipping decorators
    instead of raising this directly.
    """
    pass

class _ExpectedFailure(Exception):
    """
    Raise this when a test is expected to fail.

    This is an implementation detail.
    """

    def __init__(self, exc_info):
        super(_ExpectedFailure, self).__init__()
        self.exc_info = exc_info

class _UnexpectedSuccess(Exception):
    """
    The test was supposed to fail, but it didn't!
    """
    pass

def skip(reason):
    """Unconditionally skip a test."""
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

def expectedFailure(func):
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        try:
            func(*args, **kwargs)
        except Exception:
            raise _ExpectedFailure(sys.exc_info())
        raise _UnexpectedSuccess
    return wrapper

def skip_if_chrome(target):
    def wrapper(self, *args, **kwargs):
        if self.marionette._send_message("getContext", key="value") == "chrome":
            raise SkipTest("skipping test in chrome context")
        return target(self, *args, **kwargs)
    return wrapper

def skip_if_desktop(target):
    def wrapper(self, *args, **kwargs):
        if self.marionette.session_capabilities.get('b2g') is None:
            raise SkipTest('skipping due to desktop')
        return target(self, *args, **kwargs)
    return wrapper

def skip_if_b2g(target):
    def wrapper(self, *args, **kwargs):
        if self.marionette.session_capabilities.get('b2g') == True:
            raise SkipTest('skipping due to b2g')
        return target(self, *args, **kwargs)
    return wrapper

def skip_if_e10s(target):
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

def skip_unless_protocol(predicate):
    """Given a predicate passed the current protocol level, skip the
    test if the predicate does not match."""
    def decorator(test_item):
        @functools.wraps(test_item)
        def skip_wrapper(self):
            level = self.marionette.client.protocol
            if not predicate(level):
                raise SkipTest('skipping because protocol level is %s' % level)
            return self
        return skip_wrapper
    return decorator

def skip_unless_browser_pref(pref, predicate=bool):
    """
    Skip a test based on the value of a browser preference.

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
                self.fail("No such browser preference: %r" % pref)
            if not predicate(value):
                raise SkipTest("browser preference %r: %r" % (pref, value))
            return target(self, *args, **kwargs)
        return wrapped
    return wrapper

def parameterized(func_suffix, *args, **kwargs):
    """
    A decorator that can generate methods given a base method and some data.

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

def with_parameters(parameters):
    """
    A decorator that can generate methods given a base method and some data.
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

def wraps_parameterized(func, func_suffix, args, kwargs):
    """Internal: for MetaParameterized"""
    def wrapper(self):
        return func(self, *args, **kwargs)
    wrapper.__name__ = func.__name__ + '_' + str(func_suffix)
    wrapper.__doc__ = '[%s] %s' % (func_suffix, func.__doc__)
    return wrapper

class MetaParameterized(type):
    """
    A metaclass that allow a class to use decorators like :func:`parameterized`
    or :func:`with_parameters` to generate new methods.
    """
    RE_ESCAPE_BAD_CHARS = re.compile(r'[\.\(\) -/]')
    def __new__(cls, name, bases, attrs):
        for k, v in attrs.items():
            if callable(v) and hasattr(v, 'metaparameters'):
                for func_suffix, args, kwargs in v.metaparameters:
                    func_suffix = cls.RE_ESCAPE_BAD_CHARS.sub('_', func_suffix)
                    wrapper = wraps_parameterized(v, func_suffix, args, kwargs)
                    if wrapper.__name__ in attrs:
                        raise KeyError("%s is already a defined method on %s" %
                                        (wrapper.__name__, name))
                    attrs[wrapper.__name__] = wrapper
                del attrs[k]

        return type.__new__(cls, name, bases, attrs)

class JSTest:
    head_js_re = re.compile(r"MARIONETTE_HEAD_JS(\s*)=(\s*)['|\"](.*?)['|\"];")
    context_re = re.compile(r"MARIONETTE_CONTEXT(\s*)=(\s*)['|\"](.*?)['|\"];")
    timeout_re = re.compile(r"MARIONETTE_TIMEOUT(\s*)=(\s*)(\d+);")
    inactivity_timeout_re = re.compile(r"MARIONETTE_INACTIVITY_TIMEOUT(\s*)=(\s*)(\d+);")

class CommonTestCase(unittest.TestCase):

    __metaclass__ = MetaParameterized
    match_re = None
    failureException = AssertionError
    pydebugger = None

    def __init__(self, methodName, **kwargs):
        unittest.TestCase.__init__(self, methodName)
        self.loglines = []
        self.duration = 0
        self.start_time = 0
        self.expected = kwargs.pop('expected', 'pass')
        self.logger = get_default_logger()

    def _enter_pm(self):
        if self.pydebugger:
            self.pydebugger.post_mortem(sys.exc_info()[2])

    def _addSkip(self, result, reason):
        addSkip = getattr(result, 'addSkip', None)
        if addSkip is not None:
            addSkip(self, reason)
        else:
            warnings.warn("TestResult has no addSkip method, skips not reported",
                          RuntimeWarning, 2)
            result.addSuccess(self)

    def run(self, result=None):
        # Bug 967566 suggests refactoring run, which would hopefully
        # mean getting rid of this inner function, which only sits
        # here to reduce code duplication:
        def expected_failure(result, exc_info):
            addExpectedFailure = getattr(result, "addExpectedFailure", None)
            if addExpectedFailure is not None:
                addExpectedFailure(self, exc_info)
            else:
                warnings.warn("TestResult has no addExpectedFailure method, "
                              "reporting as passes", RuntimeWarning)
                result.addSuccess(self)

        self.start_time = time.time()
        orig_result = result
        if result is None:
            result = self.defaultTestResult()
            startTestRun = getattr(result, 'startTestRun', None)
            if startTestRun is not None:
                startTestRun()

        result.startTest(self)

        testMethod = getattr(self, self._testMethodName)
        if (getattr(self.__class__, "__unittest_skip__", False) or
            getattr(testMethod, "__unittest_skip__", False)):
            # If the class or method was skipped.
            try:
                skip_why = (getattr(self.__class__, '__unittest_skip_why__', '')
                            or getattr(testMethod, '__unittest_skip_why__', ''))
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
                        raise _ExpectedFailure(sys.exc_info())
                else:
                    self.setUp()
            except SkipTest as e:
                self._addSkip(result, str(e))
            except KeyboardInterrupt:
                raise
            except _ExpectedFailure as e:
                expected_failure(result, e.exc_info)
            except:
                self._enter_pm()
                result.addError(self, sys.exc_info())
            else:
                try:
                    if self.expected == 'fail':
                        try:
                            testMethod()
                        except:
                            raise _ExpectedFailure(sys.exc_info())
                        raise _UnexpectedSuccess
                    else:
                        testMethod()
                except self.failureException:
                    self._enter_pm()
                    result.addFailure(self, sys.exc_info())
                except KeyboardInterrupt:
                    raise
                except _ExpectedFailure as e:
                    expected_failure(result, e.exc_info)
                except _UnexpectedSuccess:
                    addUnexpectedSuccess = getattr(result, 'addUnexpectedSuccess', None)
                    if addUnexpectedSuccess is not None:
                        addUnexpectedSuccess(self)
                    else:
                        warnings.warn("TestResult has no addUnexpectedSuccess method, reporting as failures",
                                      RuntimeWarning)
                        result.addFailure(self, sys.exc_info())
                except SkipTest as e:
                    self._addSkip(result, str(e))
                except:
                    self._enter_pm()
                    result.addError(self, sys.exc_info())
                else:
                    success = True
                try:
                    if self.expected == "fail":
                        try:
                            self.tearDown()
                        except:
                            raise _ExpectedFailure(sys.exc_info())
                    else:
                        self.tearDown()
                except KeyboardInterrupt:
                    raise
                except _ExpectedFailure as e:
                    expected_failure(result, e.exc_info)
                except:
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
                stopTestRun = getattr(result, 'stopTestRun', None)
                if stopTestRun is not None:
                    stopTestRun()

    @classmethod
    def match(cls, filename):
        """
        Determines if the specified filename should be handled by this
        test class; this is done by looking for a match for the filename
        using cls.match_re.
        """
        if not cls.match_re:
            return False
        m = cls.match_re.match(filename)
        return m is not None

    @classmethod
    def add_tests_to_suite(cls, mod_name, filepath, suite, testloader, marionette, testvars):
        """
        Adds all the tests in the specified file to the specified suite.
        """
        raise NotImplementedError

    @property
    def test_name(self):
        if hasattr(self, 'jsFile'):
            return os.path.basename(self.jsFile)
        else:
            return '%s.py %s.%s' % (self.__class__.__module__,
                                    self.__class__.__name__,
                                    self._testMethodName)

    def id(self):
        # TBPL starring requires that the "test name" field of a failure message
        # not differ over time. The test name to be used is passed to
        # mozlog via the test id, so this is overriden to maintain
        # consistency.
        return self.test_name

    def set_up_test_page(self, emulator, url="test.html", permissions=None):
        emulator.set_context("content")
        url = emulator.absolute_url(url)
        emulator.navigate(url)

        if not permissions:
            return

        emulator.set_context("chrome")
        emulator.execute_script("""
Components.utils.import("resource://gre/modules/Services.jsm");
let [url, permissions] = arguments;
let uri = Services.io.newURI(url, null, null);
permissions.forEach(function (perm) {
    Services.perms.add(uri, "sms", Components.interfaces.nsIPermissionManager.ALLOW_ACTION);
});
        """, [url, permissions])
        emulator.set_context("content")

    def setUp(self):
        # Convert the marionette weakref to an object, just for the
        # duration of the test; this is deleted in tearDown() to prevent
        # a persistent circular reference which in turn would prevent
        # proper garbage collection.
        self.start_time = time.time()
        self.marionette = self._marionette_weakref()
        if self.marionette.session is None:
            self.marionette.start_session()
        if self.marionette.timeout is not None:
            self.marionette.timeouts(self.marionette.TIMEOUT_SEARCH, self.marionette.timeout)
            self.marionette.timeouts(self.marionette.TIMEOUT_SCRIPT, self.marionette.timeout)
            self.marionette.timeouts(self.marionette.TIMEOUT_PAGE, self.marionette.timeout)
        else:
            self.marionette.timeouts(self.marionette.TIMEOUT_PAGE, 30000)

        if hasattr(self, 'test_container') and self.test_container:
            self.switch_into_test_container()
        elif hasattr(self, 'test_container') and self.test_container is False:
            if self.marionette.session_capabilities.has_key('b2g') \
            and self.marionette.session_capabilities['b2g'] == True:
                self.close_test_container()

    def tearDown(self):
        pass

    def cleanTest(self):
        self._deleteSession()

    def _deleteSession(self):
        if hasattr(self, 'start_time'):
            self.duration = time.time() - self.start_time
        if hasattr(self.marionette, 'session'):
            if self.marionette.session is not None:
                try:
                    self.loglines.extend(self.marionette.get_logs())
                except Exception, inst:
                    self.loglines = [['Error getting log: %s' % inst]]
                try:
                    self.marionette.delete_session()
                except (socket.error, MarionetteException, IOError):
                    # Gecko has crashed?
                    self.marionette.session = None
                    try:
                        self.marionette.client.close()
                    except socket.error:
                        pass
        self.marionette = None

    def switch_into_test_container(self):
        self.marionette.set_context(self.marionette.CONTEXT_CONTENT)

        frame = Wait(self.marionette, timeout=10, interval=0.2).until(element_present(
            'css selector',
            'iframe[src*="app://test-container.gaiamobile.org/index.html"]'
        ))

        self.marionette.switch_to_frame(frame)

    def close_test_container(self):
        self.marionette.set_context(self.marionette.CONTEXT_CONTENT)

        result = self.marionette.execute_async_script("""
if((navigator.mozSettings == undefined) || (navigator.mozSettings == null) || (navigator.mozApps == undefined) || (navigator.mozApps == null)) {
    marionetteScriptFinished(false);
    return;
}
let setReq = navigator.mozSettings.createLock().set({'lockscreen.enabled': false});
setReq.onsuccess = function() {
    let appsReq = navigator.mozApps.mgmt.getAll();
    appsReq.onsuccess = function() {
        let apps = appsReq.result;
        for (let i = 0; i < apps.length; i++) {
            let app = apps[i];
            if (app.manifest.name === 'Test Container') {
                window.wrappedJSObject.Service.request('AppWindowManager:kill', app.origin).then(function() {
                    marionetteScriptFinished(true);
                }).catch(function() {
                    marionetteScriptFinished(false);
                });
                return;
            }
        }
        marionetteScriptFinished(false);
    }
    appsReq.onerror = function() {
        marionetteScriptFinished(false);
    }
}
setReq.onerror = function() {
    marionetteScriptFinished(false);
}""", script_timeout=60000)

        if not result:
            raise Exception('Failed to close Test Container app')

        Wait(self.marionette, timeout=10, interval=0.2).until(element_not_present(
            'css selector',
            'iframe[src*="app://test-container.gaiamobile.org/index.html"]'
        ))

    def setup_SpecialPowers_observer(self):
        self.marionette.set_context("chrome")
        self.marionette.execute_script("""
            let SECURITY_PREF = "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer";
            Components.utils.import("resource://gre/modules/Preferences.jsm");
            Preferences.set(SECURITY_PREF, true);

            if (!testUtils.hasOwnProperty("specialPowersObserver")) {
              let loader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                .getService(Components.interfaces.mozIJSSubScriptLoader);
              loader.loadSubScript("chrome://specialpowers/content/SpecialPowersObserver.jsm",
                testUtils);
              testUtils.specialPowersObserver = new testUtils.SpecialPowersObserver();
              testUtils.specialPowersObserver.init();
            }
            """)

    def run_js_test(self, filename, marionette=None):
        '''
        Run a JavaScript test file and collect its set of assertions
        into the current test's results.

        :param filename: The path to the JavaScript test file to execute.
                         May be relative to the current script.
        :param marionette: The Marionette object in which to execute the test.
                           Defaults to self.marionette.
        '''
        marionette = marionette or self.marionette
        if not os.path.isabs(filename):
            # Find the caller's filename and make the path relative to that.
            caller_file = sys._getframe(1).f_globals.get('__file__', '')
            caller_file = os.path.abspath(caller_file)
            filename = os.path.join(os.path.dirname(caller_file), filename)
        self.assert_(os.path.exists(filename),
                     'Script "%s" must exist' % filename)
        original_test_name = self.marionette.test_name
        self.marionette.test_name = os.path.basename(filename)
        f = open(filename, 'r')
        js = f.read()
        args = []

        head_js = JSTest.head_js_re.search(js);
        if head_js:
            head_js = head_js.group(3)
            head = open(os.path.join(os.path.dirname(filename), head_js), 'r')
            js = head.read() + js;

        context = JSTest.context_re.search(js)
        if context:
            context = context.group(3)
        else:
            context = 'content'

        if 'SpecialPowers' in js:
            self.setup_SpecialPowers_observer()

            if context == 'content':
                js = "var SpecialPowers = window.wrappedJSObject.SpecialPowers;\n" + js
            else:
                marionette.execute_script("""
                if (typeof(SpecialPowers) == 'undefined') {
                  let loader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                    .getService(Components.interfaces.mozIJSSubScriptLoader);
                  loader.loadSubScript("chrome://specialpowers/content/specialpowersAPI.js");
                  loader.loadSubScript("chrome://specialpowers/content/SpecialPowersObserverAPI.js");
                  loader.loadSubScript("chrome://specialpowers/content/ChromePowers.js");
                }
                """)

        marionette.set_context(context)

        if context != 'chrome':
            marionette.navigate('data:text/html,<html>test page</html>')

        timeout = JSTest.timeout_re.search(js)
        if timeout:
            timeout = timeout.group(3)
            marionette.set_script_timeout(timeout)

        inactivity_timeout = JSTest.inactivity_timeout_re.search(js)
        if inactivity_timeout:
            inactivity_timeout = inactivity_timeout.group(3)

        try:
            results = marionette.execute_js_script(
                js,
                args,
                inactivity_timeout=inactivity_timeout,
                filename=os.path.basename(filename)
            )

            self.assertTrue(not 'timeout' in filename,
                            'expected timeout not triggered')

            if 'fail' in filename:
                self.assertTrue(len(results['failures']) > 0,
                                "expected test failures didn't occur")
            else:
                for failure in results['failures']:
                    diag = "" if failure.get('diag') is None else failure['diag']
                    name = "got false, expected true" if failure.get('name') is None else failure['name']
                    self.logger.test_status(self.test_name, name, 'FAIL',
                                            message=diag)
                for failure in results['expectedFailures']:
                    diag = "" if failure.get('diag') is None else failure['diag']
                    name = "got false, expected false" if failure.get('name') is None else failure['name']
                    self.logger.test_status(self.test_name, name, 'FAIL',
                                            expected='FAIL', message=diag)
                for failure in results['unexpectedSuccesses']:
                    diag = "" if failure.get('diag') is None else failure['diag']
                    name = "got true, expected false" if failure.get('name') is None else failure['name']
                    self.logger.test_status(self.test_name, name, 'PASS',
                                            expected='FAIL', message=diag)
                self.assertEqual(0, len(results['failures']),
                                 '%d tests failed' % len(results['failures']))
                if len(results['unexpectedSuccesses']) > 0:
                    raise _UnexpectedSuccess('')
                if len(results['expectedFailures']) > 0:
                    raise _ExpectedFailure((AssertionError, AssertionError(''), None))

            self.assertTrue(results['passed']
                            + len(results['failures'])
                            + len(results['expectedFailures'])
                            + len(results['unexpectedSuccesses']) > 0,
                            'no tests run')

        except ScriptTimeoutException:
            if 'timeout' in filename:
                # expected exception
                pass
            else:
                self.loglines = marionette.get_logs()
                raise
        self.marionette.test_name = original_test_name



class MarionetteTestCase(CommonTestCase):

    match_re = re.compile(r"test_(.*)\.py$")

    def __init__(self, marionette_weakref, methodName='runTest',
                 filepath='', **kwargs):
        self._marionette_weakref = marionette_weakref
        self.marionette = None
        self.extra_emulator_index = -1
        self.methodName = methodName
        self.filepath = filepath
        self.testvars = kwargs.pop('testvars', None)
        self.test_container = kwargs.pop('test_container', None)
        CommonTestCase.__init__(self, methodName, **kwargs)

    @classmethod
    def add_tests_to_suite(cls, mod_name, filepath, suite, testloader, marionette, testvars, **kwargs):
        # since we use imp.load_source to load test modules, if a module
        # is loaded with the same name as another one the module would just be
        # reloaded.
        #
        # We may end up by finding too many test in a module then since
        # reload() only update the module dict (so old keys are still there!)
        # see https://docs.python.org/2/library/functions.html#reload
        #
        # we get rid of that by removing the module from sys.modules,
        # so we ensure that it will be fully loaded by the
        # imp.load_source call.
        if mod_name in sys.modules:
            del sys.modules[mod_name]

        test_mod = imp.load_source(mod_name, filepath)

        for name in dir(test_mod):
            obj = getattr(test_mod, name)
            if (isinstance(obj, (type, types.ClassType)) and
                issubclass(obj, unittest.TestCase)):
                testnames = testloader.getTestCaseNames(obj)
                for testname in testnames:
                    suite.addTest(obj(weakref.ref(marionette),
                                  methodName=testname,
                                  filepath=filepath,
                                  testvars=testvars,
                                  **kwargs))

    def setUp(self):
        CommonTestCase.setUp(self)
        self.marionette.test_name = self.test_name
        self.marionette.execute_script("log('TEST-START: %s:%s')" %
                                       (self.filepath.replace('\\', '\\\\'), self.methodName),
                                       sandbox="simpletest")

    def tearDown(self):
        if not self.marionette.check_for_crash():
            try:
                self.marionette.clear_imported_scripts()
                self.marionette.execute_script("log('TEST-END: %s:%s')" %
                                               (self.filepath.replace('\\', '\\\\'),
                                                self.methodName),
                                               sandbox="simpletest")
                self.marionette.test_name = None
            except (MarionetteException, IOError):
                # We have tried to log the test end when there is no listener
                # object that we can access
                pass

        CommonTestCase.tearDown(self)

    def get_new_emulator(self):
        self.extra_emulator_index += 1
        if len(self.marionette.extra_emulators) == self.extra_emulator_index:
            qemu  = Marionette(emulator=self.marionette.emulator.arch,
                               emulatorBinary=self.marionette.emulator.binary,
                               homedir=self.marionette.homedir,
                               baseurl=self.marionette.baseurl,
                               noWindow=self.marionette.noWindow,
                               gecko_path=self.marionette.gecko_path)
            qemu.start_session()
            self.marionette.extra_emulators.append(qemu)
        else:
            qemu = self.marionette.extra_emulators[self.extra_emulator_index]
        return qemu

    def wait_for_condition(self, method, timeout=30):
        timeout = float(timeout) + time.time()
        while time.time() < timeout:
            value = method(self.marionette)
            if value:
                return value
            time.sleep(0.5)
        else:
            raise TimeoutException("wait_for_condition timed out")

class MarionetteJSTestCase(CommonTestCase):

    match_re = re.compile(r"test_(.*)\.js$")

    def __init__(self, marionette_weakref, methodName='runTest', jsFile=None, **kwargs):
        assert(jsFile)
        self.jsFile = jsFile
        self._marionette_weakref = marionette_weakref
        self.marionette = None
        self.test_container = kwargs.pop('test_container', None)
        CommonTestCase.__init__(self, methodName)

    @classmethod
    def add_tests_to_suite(cls, mod_name, filepath, suite, testloader, marionette, testvars, **kwargs):
        suite.addTest(cls(weakref.ref(marionette), jsFile=filepath, **kwargs))

    def runTest(self):
        if self.marionette.session is None:
            self.marionette.start_session()
        self.marionette.execute_script(
            "log('TEST-START: %s');" % self.jsFile.replace('\\', '\\\\'),
            sandbox="simpletest")

        self.run_js_test(self.jsFile)

        self.marionette.execute_script(
            "log('TEST-END: %s');" % self.jsFile.replace('\\', '\\\\'),
            sandbox="simpletest")
        self.marionette.test_name = None

    def get_test_class_name(self):
        # returns a dot separated folders as class name
        dirname = os.path.dirname(self.jsFile).replace('\\', '/')
        if dirname.startswith('/'):
            dirname = dirname[1:]
        return '.'.join(dirname.split('/'))

    def get_test_method_name(self):
        # returns the js filename without extension as method name
        return os.path.splitext(os.path.basename(self.jsFile))[0]
