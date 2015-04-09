#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

import os
import sys
from marionette.marionette_test import MarionetteTestCase, MarionetteJSTestCase
from marionette_driver.errors import ScriptTimeoutException

class LucidDreamTestCase(MarionetteTestCase):
    def __init__(self, marionette_weakref, browser=None, logger=None, **kwargs):
        self.browser = browser
        self.logger = logger
        MarionetteTestCase.__init__(self, marionette_weakref, **kwargs)

    def run_js_test(self, filename, marionette):
        '''
        Run a JavaScript test file and collect its set of assertions
        into the current test's results.

        :param filename: The path to the JavaScript test file to execute.
                         May be relative to the current script.
        :param marionette: The Marionette object in which to execute the test.
        '''
        caller_file = sys._getframe(1).f_globals.get('__file__', '')
        caller_file = os.path.abspath(caller_file)
        script = os.path.join(os.path.dirname(caller_file), filename)
        self.assert_(os.path.exists(script), 'Script "%s" must exist' % script)
        if hasattr(MarionetteTestCase, 'run_js_test'):
            return MarionetteTestCase.run_js_test(self, script, marionette)
        #XXX: copy/pasted from marionette_test.py, refactor this!
        f = open(script, 'r')
        js = f.read()
        args = []

        head_js = MarionetteJSTestCase.head_js_re.search(js);
        if head_js:
            head_js = head_js.group(3)
            head = open(os.path.join(os.path.dirname(script), head_js), 'r')
            js = head.read() + js;

        context = MarionetteJSTestCase.context_re.search(js)
        if context:
            context = context.group(3)
        else:
            context = 'content'
        marionette.set_context(context)

        if context != "chrome":
            marionette.navigate('data:text/html,<html>test page</html>')

        timeout = MarionetteJSTestCase.timeout_re.search(js)
        if timeout:
            timeout = timeout.group(3)
            marionette.set_script_timeout(timeout)

        inactivity_timeout = MarionetteJSTestCase.inactivity_timeout_re.search(js)
        if inactivity_timeout:
            inactivity_timeout = inactivity_timeout.group(3)

        try:
            results = marionette.execute_js_script(js,
                                                   args,
                                                   special_powers=True,
                                                   inactivity_timeout=inactivity_timeout,
                                                   filename=os.path.basename(script))

            self.assertTrue(not 'timeout' in script,
                            'expected timeout not triggered')

            if 'fail' in script:
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
            if 'timeout' in script:
                # expected exception
                pass
            else:
                self.loglines = marionette.get_logs()
                raise
