from __future__ import absolute_import, unicode_literals

import os
import threading
import time

import mozunit
import pytest

from mozprofile import BaseProfile
from mozrunner.errors import RunnerNotStartedError

from raptor.control_server import RaptorControlServer
from raptor.raptor import Raptor


@pytest.mark.parametrize('app', ['firefox', 'chrome'])
def test_create_profile(options, app, get_prefs):
    options['app'] = app
    raptor = Raptor(**options)

    assert isinstance(raptor.profile, BaseProfile)
    if app != 'firefox':
        return

    # This pref is set in mozprofile
    firefox_pref = 'user_pref("app.update.enabled", false);'
    # This pref is set in raptor
    raptor_pref = 'user_pref("security.enable_java", false);'

    prefs_file = os.path.join(raptor.profile.profile, 'user.js')
    with open(prefs_file, 'r') as fh:
        prefs = fh.read()
        assert firefox_pref in prefs
        assert raptor_pref in prefs


def test_start_and_stop_server(raptor):
    assert raptor.control_server is None

    raptor.start_control_server()
    assert isinstance(raptor.control_server, RaptorControlServer)

    assert raptor.control_server._server_thread.is_alive()
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
    test['name'] = 'raptor-{}-tp7'.format(app)

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
