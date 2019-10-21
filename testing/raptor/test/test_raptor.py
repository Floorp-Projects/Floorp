from __future__ import absolute_import, unicode_literals
from six import reraise

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

from raptor.raptor import RaptorDesktopFirefox, RaptorDesktopChrome, RaptorAndroid


class TestBrowserThread(threading.Thread):
    def __init__(self, raptor_instance, tests, names):
        super(TestBrowserThread, self).__init__()
        self.raptor_instance = raptor_instance
        self.tests = tests
        self.names = names
        self.exc = None

    def run(self):
        try:
            self.raptor_instance.run_tests(self.tests, self.names)
        except BaseException:
            self.exc = sys.exc_info()


@pytest.mark.parametrize("raptor_class, app_name", [
                         [RaptorDesktopFirefox, "firefox"],
                         [RaptorDesktopChrome, "chrome"],
                         [RaptorDesktopChrome, "chromium"],
                         [RaptorAndroid, "fennec"],
                         [RaptorAndroid, "geckoview"],
                         ])
def test_build_profile(options, raptor_class, app_name, get_prefs):
    options['app'] = app_name
    raptor = raptor_class(**options)

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


@pytest.mark.parametrize('app', [
    'firefox',
    pytest.mark.xfail('chrome'),
    pytest.mark.xfail('chromium'),
])
def test_start_browser(get_binary, app):
    binary = get_binary(app)
    assert binary

    raptor = RaptorDesktopFirefox(app, binary, post_startup_delay=0)

    tests = [{
        'name': 'raptor-{}-tp6'.format(app),
        'page_timeout': 1000
    }]
    test_names = [test['name'] for test in tests]

    thread = TestBrowserThread(raptor, tests, test_names)
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
        reraise(exc, value, tb)

    assert not raptor.runner.is_running()
    assert raptor.runner.returncode is not None


if __name__ == '__main__':
    mozunit.main()
