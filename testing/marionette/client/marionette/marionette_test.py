# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
import sys
import types
import unittest

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

    def __init__(self, methodName):
        unittest.TestCase.__init__(self, methodName)
        self.loglines = None
        self.perfdata = None

    def kill_gaia_app(self, url):
        self.marionette.execute_script("""
window.wrappedJSObject.getApplicationManager().kill("%s");
return(true);
""" % url)

    def kill_gaia_apps(self):
        # shut down any running Gaia apps
        # XXX there's no API to do this currently
        pass

    def launch_gaia_app(self, url):
        # launch the app using Gaia's AppManager
        self.marionette.set_script_timeout(30000)
        frame = self.marionette.execute_async_script("""
var frame = window.wrappedJSObject.getApplicationManager().launch("%s").element;
window.addEventListener('message', function frameload(e) {
    if (e.data == 'appready') {
        window.removeEventListener('message', frameload);
        marionetteScriptFinished(frame);
    }
});
    """ % url)

        self.assertTrue(isinstance(frame, HTMLElement))
        return frame

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
        self.marionette = self._marionette_weakref()
        if self.marionette.session is None:
            self.marionette.start_session()

    def tearDown(self):
        if self.marionette.session is not None:
            self.loglines = self.marionette.get_logs()
            self.perfdata = self.marionette.get_perf_data()
            self.marionette.delete_session()
        self.marionette = None

class MarionetteTestCase(CommonTestCase):

    def __init__(self, marionette_weakref, methodName='runTest',
                 filepath='', **kwargs):
        self._marionette_weakref = marionette_weakref
        self.marionette = None
        self.extra_emulator_index = -1
        self.methodName = methodName
        self.filepath = filepath
        CommonTestCase.__init__(self, methodName, **kwargs)

    def setUp(self):
        CommonTestCase.setUp(self)
        self.marionette.execute_script("log('TEST-START: %s:%s')" % 
                                       (self.filepath, self.methodName))

    def tearDown(self):
        self.marionette.set_context("content")
        self.marionette.execute_script("log('TEST-END: %s:%s')" % 
                                       (self.filepath, self.methodName))
        CommonTestCase.tearDown(self)

    def get_new_emulator(self):
        self.extra_emulator_index += 1
        if len(self.marionette.extra_emulators) == self.extra_emulator_index:
            qemu  = Marionette(emulator=self.marionette.emulator.arch,
                               emulatorBinary=self.marionette.emulator.binary,
                               homedir=self.marionette.homedir,
                               baseurl=self.marionette.baseurl,
                               noWindow=self.marionette.noWindow)
            qemu.start_session()
            self.marionette.extra_emulators.append(qemu)
        else:
            qemu = self.marionette.extra_emulators[self.extra_emulator_index]
        return qemu


class MarionetteJSTestCase(CommonTestCase):

    context_re = re.compile(r"MARIONETTE_CONTEXT(\s*)=(\s*)['|\"](.*?)['|\"];")
    timeout_re = re.compile(r"MARIONETTE_TIMEOUT(\s*)=(\s*)(\d+);")
    launch_re = re.compile(r"MARIONETTE_LAUNCH_APP(\s*)=(\s*)['|\"](.*?)['|\"];")

    def __init__(self, marionette_weakref, methodName='runTest', jsFile=None):
        assert(jsFile)
        self.jsFile = jsFile
        self._marionette_weakref = marionette_weakref
        self.marionette = None
        CommonTestCase.__init__(self, methodName)

    def runTest(self):
        if self.marionette.session is None:
            self.marionette.start_session()
        self.marionette.execute_script("log('TEST-START: %s');" % self.jsFile)
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
            page = self.marionette.absolute_url("empty.html")
            self.marionette.navigate(page)

        timeout = self.timeout_re.search(js)
        if timeout:
            timeout = timeout.group(3)
            self.marionette.set_script_timeout(timeout)

        launch_app = self.launch_re.search(js)
        if launch_app:
            launch_app = launch_app.group(3)
            frame = self.launch_gaia_app(launch_app)
            args.append({'__marionetteArgs': {'appframe': frame}})

        try:
            results = self.marionette.execute_js_script(js, args, special_powers=True)

            if launch_app:
                self.kill_gaia_app(launch_app)

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
                    fails.append('TEST-UNEXPECTED-FAIL %s| %s' % (diag, name))
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

        self.marionette.execute_script("log('TEST-END: %s');" % self.jsFile)



