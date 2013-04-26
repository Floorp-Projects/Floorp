# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import imp
import os
import re
import sys
import time
import types
import unittest
import weakref

from errors import *
from marionette import HTMLElement, Marionette

def skip_if_b2g(target):
    def wrapper(self, *args, **kwargs):
        if not hasattr(self.marionette, 'b2g') or not self.marionette.b2g:
            return target(self, *args, **kwargs)
        else:
            sys.stderr.write('skipping ... ')
    return wrapper

class CommonTestCase(unittest.TestCase):

    match_re = None

    def __init__(self, methodName):
        unittest.TestCase.__init__(self, methodName)
        self.loglines = None
        self.perfdata = None
        self.duration = 0

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

    def tearDown(self):
        self.duration = time.time() - self.start_time
        if self.marionette.session is not None:
            self.loglines = self.marionette.get_logs()
            self.perfdata = self.marionette.get_perf_data()
            self.marionette.delete_session()
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


class MarionetteJSTestCase(CommonTestCase):

    context_re = re.compile(r"MARIONETTE_CONTEXT(\s*)=(\s*)['|\"](.*?)['|\"];")
    timeout_re = re.compile(r"MARIONETTE_TIMEOUT(\s*)=(\s*)(\d+);")
    match_re = re.compile(r"test_(.*)\.js$")

    def __init__(self, marionette_weakref, methodName='runTest', jsFile=None):
        assert(jsFile)
        self.jsFile = jsFile
        self._marionette_weakref = marionette_weakref
        self.marionette = None
        CommonTestCase.__init__(self, methodName)

    @classmethod
    def add_tests_to_suite(cls, mod_name, filepath, suite, testloader, marionette, testvars):
        suite.addTest(cls(weakref.ref(marionette), jsFile=filepath))

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

        try:
            results = self.marionette.execute_js_script(js, args, special_powers=True)

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

            if not self.perfdata:
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
