from __future__ import absolute_import, unicode_literals

import os
import sys
import threading
import time

import mozunit
import pytest

from mozprofile import BaseProfile
from mozrunner.errors import RunnerNotStartedError

# need this so raptor imports work both from /raptor and via mach
here = os.path.abspath(os.path.dirname(__file__))
if os.environ.get('SCRIPTSPATH', None) is not None:
    # in production it is env SCRIPTS_PATH
    mozharness_dir = os.environ['SCRIPTSPATH']
else:
    # locally it's in source tree
    mozharness_dir = os.path.join(here, '../../mozharness')
sys.path.insert(0, mozharness_dir)

from raptor.raptor import RaptorDesktopFirefox, RaptorDesktopChrome, RaptorAndroid


class TestBrowserThread(threading.Thread):
        def __init__(self, raptor_instance, test, timeout=None):
            super(TestBrowserThread, self).__init__()
            self.raptor_instance = raptor_instance
            self.test = test
            self.timeout = timeout
            self.exc = None

        def run(self):
            try:
                self.raptor_instance.run_test(self.test, self.timeout)
            except BaseException:
                self.exc = sys.exc_info()


@pytest.mark.parametrize("raptor_class, app_name", [
                         [RaptorDesktopFirefox, "firefox"],
                         [RaptorDesktopChrome, "chrome"],
                         [RaptorAndroid, "fennec"],
                         [RaptorAndroid, "geckoview"],
                         ])
def test_create_profile(options, raptor_class, app_name, get_prefs):
    options['app'] = app_name
    raptor = raptor_class(**options)

    if app_name in ["fennec", "geckoview"]:
        raptor.profile_class = "firefox"
    raptor.create_browser_profile()

    assert isinstance(raptor.profile, BaseProfile)
    if app_name != 'firefox':
        return

    # These prefs are set in mozprofile
    firefox_prefs = [
        'user_pref("app.update.checkInstallTime", false);',
        'user_pref("app.update.disabledForTesting", true);',
        'user_pref("'
        'security.turn_off_all_security_so_that_viruses_can_take_over_this_computer", true);'
    ]
    # This pref is set in raptor
    raptor_pref = 'user_pref("security.enable_java", false);'

    prefs_file = os.path.join(raptor.profile.profile, 'user.js')
    with open(prefs_file, 'r') as fh:
        prefs = fh.read()
        for firefox_pref in firefox_prefs:
            assert firefox_pref in prefs
        assert raptor_pref in prefs


def test_start_and_stop_server(raptor):
    assert raptor.control_server is None

    raptor.create_browser_profile()
    raptor.create_browser_handler()
    raptor.start_control_server()

    assert raptor.control_server._server_thread.is_alive()
    assert raptor.control_server.port is not None
    assert raptor.control_server.server is not None

    raptor.clean_up()
    assert not raptor.control_server._server_thread.is_alive()


@pytest.mark.parametrize('app', [
    'firefox',
    pytest.mark.xfail('chrome'),
])
def test_start_browser(get_binary, app):
    binary = get_binary(app)
    assert binary

    raptor = RaptorDesktopFirefox(app, binary, post_startup_delay=0)
    raptor.create_browser_profile()
    raptor.create_browser_handler()
    raptor.start_control_server()

    test = {}
    test['name'] = 'raptor-{}-tp6'.format(app)

    thread = TestBrowserThread(raptor, test, timeout=0)
    thread.start()

    timeout = time.time() + 5  # seconds
    while time.time() < timeout:
        try:
            is_running = raptor.runner.is_running()
            assert is_running
            break
        except RunnerNotStartedError:
            time.sleep(0.1)
    else:
        assert False  # browser didn't start

    raptor.clean_up()
    thread.join(5)

    if thread.exc is not None:
        exc, value, tb = thread.exc
        raise exc, value, tb

    assert not raptor.runner.is_running()
    assert raptor.runner.returncode is not None


if __name__ == '__main__':
    mozunit.main()
