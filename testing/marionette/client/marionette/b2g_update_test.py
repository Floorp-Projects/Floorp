# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from cStringIO import StringIO
import imp
import os
import re
import subprocess
import time
import types
import weakref

from b2ginstance import B2GInstance
from errors import InvalidResponseException
from marionette import Marionette
from marionette_test import MarionetteTestCase
from marionette_transport import MarionetteTransport
from runtests import MarionetteTestRunner, cli

class B2GUpdateMarionetteClient(MarionetteTransport):
    RETRY_TIMEOUT   = 5
    CONNECT_TIMEOUT = 30
    SEND_TIMEOUT    = 60 * 5
    MAX_RETRIES     = 24

    def __init__(self, addr, port, runner):
        super(B2GUpdateMarionetteClient, self).__init__(addr, port)
        self.runner = runner

    def connect(self):
        """ When using ADB port forwarding, the ADB server will actually accept
            connections even though there isn't a listening socket open on the
            device. Here we add a retry loop since we have to restart the debug
            server / b2g process for update tests
        """
        for i in range(self.MAX_RETRIES):
            try:
                MarionetteTransport.connect(self, timeout=self.CONNECT_TIMEOUT)
                break
            except:
                if i == self.MAX_RETRIES - 1:
                    raise

                time.sleep(self.RETRY_TIMEOUT)
                self.runner.port_forward()

        # Upon success, reset the socket timeout to something more reasonable
        self.sock.settimeout(self.SEND_TIMEOUT)

class B2GUpdateTestRunner(MarionetteTestRunner):
    match_re = re.compile(r'update_(smoke)?test_(.*)\.py$')

    def __init__(self, **kwargs):
        MarionetteTestRunner.__init__(self, **kwargs)
        if self.emulator or self.bin:
            raise Exception('Update tests do not support emulator or custom binaries')

        if not self.address:
            raise Exception('Update tests must be run with '
                            '--address=localhost:<port>')

        self.host, port = self.address.split(':')
        self.port = int(port)
        self.test_handlers.append(self)

        self.b2g = B2GInstance(homedir=kwargs.get('homedir'))
        self.update_tools = self.b2g.import_update_tools()
        self.adb = self.update_tools.AdbTool(path=self.b2g.adb_path,
                                             device=self.device_serial)

    def match(self, filename):
        return self.match_re.match(filename) is not None

    def add_tests_to_suite(self, mod_name, filepath, suite, testloader,
                           marionette, testvars):
        """ Here the runner itself is a handler so we can forward along the
            instance to test cases.
        """
        test_mod = imp.load_source(mod_name, filepath)

        def add_test(testcase, testname, **kwargs):
            suite.addTest(testcase(weakref.ref(marionette),
                          methodName=testname,
                          filepath=filepath,
                          testvars=testvars,
                          **kwargs))

        # The TestCase classes are apparently being loaded multiple times, so
        # using "isinstance" doesn't actually work. This function just compares
        # type names as a close enough analog.
        def has_super(cls, super_names):
            if not isinstance(super_names, (tuple, list)):
                super_names = (super_names)

            base = cls
            while base:
                if base.__name__ in super_names:
                    return True
                base = base.__base__
            return False

        for name in dir(test_mod):
            testcase = getattr(test_mod, name)
            if not isinstance(testcase, (type, types.ClassType)):
                continue

            # Support both B2GUpdateTestCase and MarionetteTestCase
            if has_super(testcase, 'B2GUpdateTestCase'):
                for testname in testloader.getTestCaseNames(testcase):
                    add_test(testcase, testname, runner=self)
            elif has_super(testcase, ('MarionetteTestCase', 'TestCase')):
                for testname in testloader.getTestCaseNames(testcase):
                    add_test(testcase, testname)

    def start_marionette(self):
        MarionetteTestRunner.start_marionette(self)
        self.marionette.client = B2GUpdateMarionetteClient(self.host,
                                                           self.port,
                                                           self)

    def reset(self, b2g_pid):
        if self.marionette.instance:
            self.marionette.instance.close()
            self.marionette.instance = None
        del self.marionette

        self.start_marionette()
        self.b2g_pid = b2g_pid
        return self.marionette

    def find_b2g_pid(self):
        pids = self.adb.get_pids('b2g')
        if len(pids) == 0:
            return None
        return pids[0]

    def port_forward(self):
        try:
            self.adb.run('forward', 'tcp:%d' % self.port, 'tcp:2828')
        except:
            # This command causes non-0 return codes even though it succeeds
            pass

OTA, FOTA = "OTA", "FOTA"
class B2GUpdateTestCase(MarionetteTestCase):
    """ A high level unit test for an OTA or FOTA update. This test case class
    has structural support for automatically waiting on an update to apply,
    and provides additional javascript support for separating a test into
    'pre-update' and 'post-update' lifecycles.

    See test examples in toolkit/mozapps/update/test/marionette/update_test_*.py
    """

    MAX_OTA_WAIT   = 60 * 2  # 2 minutes
    MAX_FOTA_WAIT  = 60 * 10 # 10 minutes

    def __init__(self, marionette_weakref, **kwargs):
        if 'runner' in kwargs:
            self.runner = kwargs['runner']
            del kwargs['runner']

        update_test_js = os.path.join(os.path.dirname(__file__), 'atoms',
                                      'b2g_update_test.js')
        with open(update_test_js, 'r') as f:
            self.update_test_js = f.read()

        self.b2g_pid = self.runner.find_b2g_pid()
        if not self.b2g_pid:
            raise Exception('B2G PID could not be found for update test')
        MarionetteTestCase.__init__(self, marionette_weakref, **kwargs)

        self.testvars = self.testvars or {}
        self.status_newline = True
        self.loglines = []

    def print_status(self, status, message=None):
        if self.status_newline:
            print ''
            self.status_newline = False

        status_msg = 'UPDATE-TEST-' + status
        if message:
            status_msg += ': ' + message
        print status_msg

    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_context(Marionette.CONTEXT_CHROME)

    def tearDown(self):
        # This completey overrides MarionetteTestCase.tearDown so we can control
        # logs being appended between various marionette runs
        self.marionette.set_context(Marionette.CONTEXT_CONTENT)
        self.marionette.execute_script("log('TEST-END: %s:%s')" %
                                       (self.filepath.replace('\\', '\\\\'), self.methodName))
        self.marionette.test_name = None

        self.duration = time.time() - self.start_time
        if self.marionette.session is not None:
            self.loglines.extend(self.marionette.get_logs())
            self.marionette.delete_session()
        self.marionette = None

    def reset(self, b2g_pid):
        self.print_status('RESET-MARIONETTE')
        self._marionette_weakref = weakref.ref(self.runner.reset(b2g_pid))
        self.marionette = self._marionette_weakref()
        if self.marionette.session is None:
            self.marionette.start_session()
            self.marionette.set_context(Marionette.CONTEXT_CHROME)

    def execute_update_test(self, path, apply=None):
        self.execute_update_js(path, stage='pre-update',
                               will_restart=(apply is not None))
        if not apply:
            return

        if self.marionette.session is not None:
            self.loglines.extend(self.marionette.get_logs())

        # This function will probably force-kill b2g, so we can't capture logs
        self.execute_update_js(path, stage='apply-update')
        self.wait_for_update(apply)

        self.execute_update_js(path, stage='post-update')

    def execute_update_js(self, path, stage=None, will_restart=True):
        data = self.update_test_js[:]
        with open(path, "r") as f:
            data += f.read()

        status = 'EXEC'
        if stage:
            status += '-' + stage.upper()
            data += '\nrunUpdateTest("%s");' % stage

        self.print_status(status, os.path.basename(path))

        try:
            results = self.marionette.execute_async_script(data,
                                                           script_args=[self.testvars],
                                                           special_powers=True)
            self.handle_results(path, stage, results)
        except InvalidResponseException, e:
            # If the update test causes a restart, we will get an invalid
            # response from the socket here.
            if not will_restart:
                raise e

    def handle_results(self, path, stage, results):
        passed = results['passed']
        failed = results['failed']

        fails = StringIO()
        stage_msg = ' %s' % stage if stage else ''
        fails.write('%d%s tests failed:\n' % (failed, stage_msg))

        for failure in results['failures']:
            diag = failure.get('diag')
            diag_msg = "" if not diag else "| %s " % diag
            name = failure.get('name') or 'got false, expected true'
            fails.write('TEST-UNEXPECTED-FAIL | %s %s| %s\n' %
                        (os.path.basename(path), diag_msg, name))
        self.assertEqual(0, failed, fails.getvalue())
        self.assertTrue(passed + failed > 0, 'no tests run')

    def stage_update(self, **kwargs):
        mar = kwargs.get('complete_mar') or kwargs.get('partial_mar')
        short_mar = os.path.relpath(mar, os.path.dirname(os.path.dirname(mar)))
        self.print_status('STAGE', short_mar)

        prefs = kwargs.pop('prefs', {})
        update_xml = kwargs.get('update_xml')
        if not update_xml:
            xml_kwargs = kwargs.copy()
            if 'update_dir' in xml_kwargs:
                del xml_kwargs['update_dir']
            if 'only_override' in xml_kwargs:
                del xml_kwargs['only_override']
            builder = self.runner.update_tools.UpdateXmlBuilder(**xml_kwargs)
            update_xml = builder.build_xml()

        test_kwargs = {
            'adb_path': self.runner.adb.tool,
            'update_xml': update_xml,
            'only_override': kwargs.get('only_override', False),
        }
        for key in ('complete_mar', 'partial_mar', 'url_template', 'update_dir'):
            test_kwargs[key] = kwargs.get(key)

        test_update = self.runner.update_tools.TestUpdate(**test_kwargs)
        test_update.test_update(write_url_pref=False, restart=False)
        if 'prefs' not in self.testvars:
            self.testvars['prefs'] = {}

        self.testvars['prefs']['app.update.url.override'] = test_update.update_url
        self.testvars['prefs'].update(prefs)

    def wait_for_update(self, type):
        if type == OTA:
            self.wait_for_ota_restart()
        elif type == FOTA:
            self.wait_for_fota_reboot()

    def wait_for_new_b2g_pid(self, max_wait):
        for i in range(max_wait):
            b2g_pid = self.runner.find_b2g_pid()
            if b2g_pid and b2g_pid != self.b2g_pid:
                return b2g_pid
            time.sleep(1)

        return None

    def wait_for_ota_restart(self):
        self.print_status('WAIT-FOR-OTA-RESTART')

        new_b2g_pid = self.wait_for_new_b2g_pid(self.MAX_OTA_WAIT)
        if not new_b2g_pid:
            self.fail('Timed out waiting for B2G process during OTA update')
            return

        self.reset(new_b2g_pid)

    def wait_for_fota_reboot(self):
        self.print_status('WAIT-FOR-FOTA-REBOOT')

        # First wait for the device to go offline
        for i in range(self.MAX_FOTA_WAIT):
            online = self.runner.adb.get_online_devices()
            if self.runner.device not in online:
                break
            time.sleep(1)

        if i == self.MAX_FOTA_WAIT - 1:
            self.fail('Timed out waiting for device to go offline during FOTA update')

        # Now wait for the device to come back online
        for j in range(i, self.MAX_FOTA_WAIT):
            online = self.runner.adb.get_online_devices()
            if self.runner.device in online:
                break
            time.sleep(1)

        if j == self.MAX_FOTA_WAIT - 1:
            self.fail('Timed out waiting for device to come back online during FOTA update')

        # Finally wait for the B2G process
        for k in range(j, self.MAX_FOTA_WAIT):
            b2g_pid = self.runner.find_b2g_pid()
            if b2g_pid:
                self.reset(b2g_pid)
                return
            time.sleep(1)

        self.fail('Timed out waiting for B2G process to start during FOTA update')

    def flash(self, flash_script):
        flash_build = os.path.basename(os.path.dirname(flash_script))
        self.print_status('FLASH-BUILD', flash_build)

        subprocess.check_call([flash_script, self.runner.device])
        self.runner.adb.run('wait-for-device')
        self.runner.port_forward()

        self.b2g_pid = None
        b2g_pid = self.wait_for_new_b2g_pid(self.MAX_OTA_WAIT)
        if not b2g_pid:
            self.fail('Timed out waiting for B2G process to start after a flash')

        self.reset(b2g_pid)

class B2GUpdateSmokeTestCase(B2GUpdateTestCase):
    """ An even higher-level update test that is meant to be run with builds
    and updates generated in an automated build environment.

    Update smoke tests have two main differences from a plain update test:
    - They are meant to be passed various MARs and system images through the
      --testvars Marionette argument.
    - Before each test, the device can be flashed with a specified set of images
      by the test case.

    See smoketest examples in
    toolkit/mozapps/update/test/marionette/update_smoketest_*.py
    """

    "The path of the Javascript file that has assertions to run"
    JS_PATH          = None

    """ Which build to flash and vars to stage before the test is run.
        Possible values: 'start', 'finish', or None
    """
    START_WITH_BUILD = 'start'

    "A map of prefs to set while staging, before the test is run"
    STAGE_PREFS      = None

    "Marionette script timeout"
    TIMEOUT          = 2 * 60 * 1000

    """What kind of update to apply after the 'pre-update' tests have
    finished. Possible values: OTA, FOTA, None. None means "Don't apply"
    """
    APPLY            = OTA

    def setUp(self):
        if self.START_WITH_BUILD:
            build = self.testvars[self.START_WITH_BUILD]
            self.flash(build['flash_script'])

        B2GUpdateTestCase.setUp(self)

    def stage_update(self, build=None, mar=None, **kwargs):
        if build and not mar:
            raise Exception('mar required with build')
        if mar and not build:
            raise Exception('build required with mar')

        if not build:
            B2GUpdateTestCase.stage_update(self, **kwargs)
            return

        build_vars = self.testvars[build]
        mar_key = mar + '_mar'
        mar_path = build_vars[mar_key]

        stage_kwargs = {
            'build_id': build_vars['app_build_id'],
            'app_version': build_vars['app_version'],
            'platform_version': build_vars['platform_milestone'],
            mar_key: mar_path
        }
        stage_kwargs.update(kwargs)

        if self.STAGE_PREFS:
            if 'prefs' not in stage_kwargs:
                stage_kwargs['prefs'] = self.STAGE_PREFS

        B2GUpdateTestCase.stage_update(self, **stage_kwargs)

    def execute_smoketest(self):
        self.marionette.set_script_timeout(self.TIMEOUT)
        self.execute_update_test(self.JS_PATH, apply=self.APPLY)

if __name__ == '__main__':
    cli(B2GUpdateTestRunner)
