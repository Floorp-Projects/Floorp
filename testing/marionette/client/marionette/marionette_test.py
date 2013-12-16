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

from errors import *
from marionette import Marionette

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
    """
    Unconditionally skip a test.
    """
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

def skip_if_b2g(target):
    def wrapper(self, *args, **kwargs):
        if not hasattr(self.marionette, 'b2g') or not self.marionette.b2g:
            return target(self, *args, **kwargs)
        else:
            sys.stderr.write('skipping ... ')
    return wrapper

class CommonTestCase(unittest.TestCase):

    match_re = None
    failureException = AssertionError

    def __init__(self, methodName, **kwargs):
        unittest.TestCase.__init__(self, methodName)
        self.loglines = []
        self.duration = 0
        self.start_time = 0
        self.expected = kwargs.pop('expected', 'pass')

    def _addSkip(self, result, reason):
        addSkip = getattr(result, 'addSkip', None)
        if addSkip is not None:
            addSkip(self, reason)
        else:
            warnings.warn("TestResult has no addSkip method, skips not reported",
                          RuntimeWarning, 2)
            result.addSuccess(self)

    def run(self, result=None):
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
                self.setUp()
            except SkipTest as e:
                self._addSkip(result, str(e))
            except KeyboardInterrupt:
                raise
            except:
                result.addError(self, sys.exc_info())
            else:
                try:
                    if self.expected == 'fail':
                        try:
                            testMethod()
                        except Exception:
                            raise _ExpectedFailure(sys.exc_info())
                        raise _UnexpectedSuccess
                    else:
                        testMethod()
                except self.failureException:
                    result.addFailure(self, sys.exc_info())
                except KeyboardInterrupt:
                    raise
                except self.failureException:
                    result.addFailure(self, sys.exc_info())
                except _ExpectedFailure as e:
                    addExpectedFailure = getattr(result, 'addExpectedFailure', None)
                    if addExpectedFailure is not None:
                        addExpectedFailure(self, e.exc_info)
                    else:
                        warnings.warn("TestResult has no addExpectedFailure method, reporting as passes",
                                      RuntimeWarning)
                        result.addSuccess(self)
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
                    result.addError(self, sys.exc_info())
                else:
                    success = True
                try:
                    self.tearDown()
                except KeyboardInterrupt:
                    raise
                except:
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

    def tearDown(self):
        pass  # bug 874599

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
                except (socket.error, MarionetteException):
                    # Gecko has crashed?
                    self.marionette.session = None
                    try:
                        self.marionette.client.close()
                    except socket.error:
                        pass
        self.marionette = None

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
        CommonTestCase.__init__(self, methodName, **kwargs)

    @classmethod
    def add_tests_to_suite(cls, mod_name, filepath, suite, testloader, marionette, testvars, **kwargs):
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
                                       (self.filepath.replace('\\', '\\\\'), self.methodName))

    def tearDown(self):
        self.marionette.set_context("content")
        self.marionette.execute_script("log('TEST-END: %s:%s')" %
                                       (self.filepath.replace('\\', '\\\\'), self.methodName))
        self.marionette.test_name = None
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

    head_js_re = re.compile(r"MARIONETTE_HEAD_JS(\s*)=(\s*)['|\"](.*?)['|\"];")
    context_re = re.compile(r"MARIONETTE_CONTEXT(\s*)=(\s*)['|\"](.*?)['|\"];")
    timeout_re = re.compile(r"MARIONETTE_TIMEOUT(\s*)=(\s*)(\d+);")
    inactivity_timeout_re = re.compile(r"MARIONETTE_INACTIVITY_TIMEOUT(\s*)=(\s*)(\d+);")
    match_re = re.compile(r"test_(.*)\.js$")

    def __init__(self, marionette_weakref, methodName='runTest', jsFile=None, **kwargs):
        assert(jsFile)
        self.jsFile = jsFile
        self._marionette_weakref = marionette_weakref
        self.marionette = None
        self.oop = kwargs.pop('oop')
        CommonTestCase.__init__(self, methodName)

    @classmethod
    def add_tests_to_suite(cls, mod_name, filepath, suite, testloader, marionette, testvars, **kwargs):
        suite.addTest(cls(weakref.ref(marionette), jsFile=filepath, **kwargs))

    def runTest(self):
        if self.marionette.session is None:
            self.marionette.start_session()
        self.marionette.test_name = os.path.basename(self.jsFile)
        self.marionette.execute_script("log('TEST-START: %s');" % self.jsFile.replace('\\', '\\\\'))

        f = open(self.jsFile, 'r')
        js = f.read()
        args = []

        # if this is a browser_ test, prepend head.js to it
        if os.path.basename(self.jsFile).startswith('browser_'):
            local_head = open(os.path.join(os.path.dirname(__file__), 'tests', 'head.js'), 'r')
            js = local_head.read() + js
            head = open(os.path.join(os.path.dirname(self.jsFile), 'head.js'), 'r')
            for line in head:
                # we need a bigger timeout than the default specified by the
                # 'real' head.js
                if 'const kDefaultWait' in line:
                    js += 'const kDefaultWait = 45000;\n'
                else:
                    js += line

        if os.path.basename(self.jsFile).startswith('test_'):
            head_js = self.head_js_re.search(js);
            if head_js:
                head_js = head_js.group(3)
                head = open(os.path.join(os.path.dirname(self.jsFile), head_js), 'r')
                js = head.read() + js;

        if self.oop:
            print 'running oop'
            result = self.marionette.execute_async_script("""
let setReq = navigator.mozSettings.createLock().set({'lockscreen.enabled': false});
setReq.onsuccess = function() {
    let appsReq = navigator.mozApps.mgmt.getAll();
    appsReq.onsuccess = function() {
        let apps = appsReq.result;
        for (let i = 0; i < apps.length; i++) {
            let app = apps[i];
            if (app.manifest.name === 'Test Container') {
                app.launch();
                window.addEventListener('apploadtime', function apploadtime(){
                    window.removeEventListener('apploadtime', apploadtime);
                    marionetteScriptFinished(true);
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
            self.assertTrue(result)

            self.marionette.switch_to_frame(
                self.marionette.find_element(
                    'css selector',
                    'iframe[src*="app://test-container.gaiamobile.org/index.html"]'
                ))

            main_process = self.marionette.execute_script("""
                return SpecialPowers.isMainProcess();
                """)
            self.assertFalse(main_process)

        context = self.context_re.search(js)
        if context:
            context = context.group(3)
            self.marionette.set_context(context)

        if context != "chrome":
            self.marionette.navigate('data:text/html,<html>test page</html>')

        timeout = self.timeout_re.search(js)
        if timeout:
            timeout = timeout.group(3)
            self.marionette.set_script_timeout(timeout)

        inactivity_timeout = self.inactivity_timeout_re.search(js)
        if inactivity_timeout:
            inactivity_timeout = inactivity_timeout.group(3)

        try:
            results = self.marionette.execute_js_script(js,
                                                        args,
                                                        special_powers=True,
                                                        inactivity_timeout=inactivity_timeout,
                                                        filename=os.path.basename(self.jsFile))

            self.assertTrue(not 'timeout' in self.jsFile,
                            'expected timeout not triggered')

            if 'fail' in self.jsFile:
                self.assertTrue(results['failed'] > 0,
                                "expected test failures didn't occur")
            else:
                fails = []
                for failure in results['failures']:
                    diag = "" if failure.get('diag') is None else "| %s " % failure['diag']
                    name = "got false, expected true" if failure.get('name') is None else failure['name']
                    fails.append('TEST-UNEXPECTED-FAIL | %s %s| %s' %
                                 (os.path.basename(self.jsFile), diag, name))
                self.assertEqual(0, results['failed'],
                                 '%d tests failed:\n%s' % (results['failed'], '\n'.join(fails)))

            self.assertTrue(results['passed'] + results['failed'] > 0,
                            'no tests run')

        except ScriptTimeoutException:
            if 'timeout' in self.jsFile:
                # expected exception
                pass
            else:
                self.loglines = self.marionette.get_logs()
                raise

        self.marionette.execute_script("log('TEST-END: %s');" % self.jsFile.replace('\\', '\\\\'))
        self.marionette.test_name = None
