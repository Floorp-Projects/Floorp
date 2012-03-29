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
        self._qemu = []
        unittest.TestCase.__init__(self, methodName)

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

    def setUp(self):
        if self.marionette.session is None:
            self.marionette.start_session()
        self.loglines = None

    def tearDown(self):
        if self.marionette.session is not None:
            self.marionette.delete_session()
        for _qemu in self._qemu:
            _qemu.emulator.close()
            _qemu = None
        self._qemu = []


class MarionetteTestCase(CommonTestCase):

    def __init__(self, marionette, methodName='runTest'):
        self.marionette = marionette
        CommonTestCase.__init__(self, methodName)

    def get_new_emulator(self):
        _qemu  = Marionette(emulator=True,
                            homedir=self.marionette.homedir,
                            baseurl=self.marionette.baseurl,
                            noWindow=self.marionette.noWindow)
        _qemu.start_session()
        self._qemu.append(_qemu)
        return _qemu


class MarionetteJSTestCase(CommonTestCase):

    context_re = re.compile(r"MARIONETTE_CONTEXT(\s*)=(\s*)['|\"](.*?)['|\"];")
    timeout_re = re.compile(r"MARIONETTE_TIMEOUT(\s*)=(\s*)(\d+);")
    launch_re = re.compile(r"MARIONETTE_LAUNCH_APP(\s*)=(\s*)['|\"](.*?)['|\"];")

    def __init__(self, marionette, methodName='runTest', jsFile=None):
        assert(jsFile)
        self.jsFile = jsFile
        self.marionette = marionette
        CommonTestCase.__init__(self, methodName)

    def runTest(self):
        if self.marionette.session is None:
            self.marionette.start_session()
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
            results = self.marionette.execute_js_script(js, args)

            self.loglines = self.marionette.get_logs()

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

            self.assertTrue(results['passed'] + results['failed'] > 0,
                            'no tests run')
            if self.marionette.session is not None:
                self.marionette.delete_session()

        except ScriptTimeoutException:
            if 'timeout' in self.jsFile:
                # expected exception
                pass
            else:
                self.loglines = self.marionette.get_logs()
                raise




