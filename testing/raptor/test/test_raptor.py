from __future__ import absolute_import, unicode_literals

import os
import threading
import time

import mozunit
import pytest

from mozprofile import BaseProfile
from mozrunner.errors import RunnerNotStartedError

from raptor.raptor import Raptor


@pytest.mark.parametrize('app', ['firefox', 'chrome'])
def test_create_profile(options, app, get_prefs):
    options['app'] = app
    raptor = Raptor(**options)

    assert isinstance(raptor.profile, BaseProfile)
    if app != 'firefox':
        return

    # These prefs are set in mozprofile
    firefox_prefs = [
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

    raptor = Raptor(app, binary)
    raptor.start_control_server()

    test = {}
    test['name'] = 'raptor-{}-tp6'.format(app)

    thread = threading.Thread(target=raptor.run_test, args=(test,))
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
    assert not raptor.runner.is_running()
    assert raptor.runner.returncode is not None


if __name__ == '__main__':
    mozunit.main()
