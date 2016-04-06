# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import pytest
from mock import patch, Mock

from marionette.runtests import (
    MarionetteTestRunner,
    MarionetteHarness,
    cli
)

# avoid importing MarionetteJSTestCase to prevent pytest from
# collecting and running it as part of this test suite
import marionette.marionette_test as marionette_test

@pytest.fixture(scope='module')
def logger():
    """
    Fake logger to help with mocking out other runner-related classes.
    """
    import mozlog
    return Mock(spec=mozlog.structuredlog.StructuredLogger)


@pytest.fixture()
def mach_parsed_kwargs(logger):
    """
    Parsed and verified dictionary used during simplest
    call to mach marionette-test
    """
    return {
         'adb_host': None,
         'adb_port': None,
         'addon': None,
         'address': None,
         'app': None,
         'app_args': [],
         'binary': u'/path/to/firefox',
         'device_serial': None,
         'e10s': False,
         'emulator': None,
         'emulator_binary': None,
         'emulator_img': None,
         'emulator_res': None,
         'gecko_log': None,
         'homedir': None,
         'jsdebugger': False,
         'log_errorsummary': None,
         'log_html': None,
         'log_mach': None,
         'log_mach_buffer': None,
         'log_mach_level': None,
         'log_mach_verbose': None,
         'log_raw': None,
         'log_raw_level': None,
         'log_tbpl': None,
         'log_tbpl_buffer': None,
         'log_tbpl_level': None,
         'log_unittest': None,
         'log_xunit': None,
         'logcat_stdout': False,
         'logdir': None,
         'logger_name': 'Marionette-based Tests',
         'no_window': False,
         'prefs': {},
         'prefs_args': None,
         'prefs_files': None,
         'profile': None,
         'pydebugger': None,
         'repeat': 0,
         'sdcard': None,
         'server_root': None,
         'shuffle': False,
         'shuffle_seed': 2276870381009474531,
         'socket_timeout': 360.0,
         'sources': None,
         'startup_timeout': 60,
         'symbols_path': None,
         'test_tags': None,
         'tests': [u'/path/to/unit-tests.ini'],
         'testvars': None,
         'this_chunk': None,
         'timeout': None,
         'total_chunks': None,
         'tree': 'b2g',
         'type': None,
         'verbose': None,
         'workspace': None,
         'logger': logger,
    }



@pytest.fixture
def harness_class(request):
    """
    Mock based on MarionetteHarness whose run method just returns a number of
    failures according to the supplied test parameter
    """
    if 'num_failures' in request.funcargnames:
        failures = request.getfuncargvalue('num_failures')
    else:
        failures = 0
    harness_cls = Mock(spec=MarionetteHarness)
    harness = harness_cls.return_value
    if failures is None:
        harness.run.side_effect = Exception
    else:
        harness.run.return_value = failures
    return harness_cls


@pytest.fixture
def runner_class(request):
    """
    Mock based on MarionetteTestRunner, wherein the runner.failed
    attribute is provided by a test parameter
    """
    if 'num_failures' in request.funcargnames:
        failures = request.getfuncargvalue('num_failures')
    else:
        failures = 0
    mock_runner_class = Mock(spec=MarionetteTestRunner)
    runner = mock_runner_class.return_value
    runner.failed = failures
    return mock_runner_class


@pytest.mark.parametrize(
    "num_failures,exit_code",
    [(0, 0), (1, 10), (None, 1)],
)
def test_cli_exit_code(num_failures, exit_code, harness_class):
    with pytest.raises(SystemExit) as err:
        cli(harness_class=harness_class)
    assert err.value.code == exit_code


@pytest.mark.parametrize("num_failures", [0, 1])
def test_call_harness_with_parsed_args_yields_num_failures(mach_parsed_kwargs,
                                                           runner_class,
                                                           num_failures):
    with patch('marionette.runtests.MarionetteHarness.parse_args') as parse_args:
        failed = MarionetteHarness(runner_class, args=mach_parsed_kwargs).run()
        parse_args.assert_not_called()
    assert failed == num_failures


def test_call_harness_with_no_args_yields_num_failures(runner_class):
    with patch('marionette.runtests.MarionetteHarness.parse_args') as parse_args:
        parse_args.return_value = {'tests':[]}
        failed = MarionetteHarness(runner_class).run()
        assert parse_args.call_count == 1
    assert failed == 0


def test_harness_sets_up_default_test_handlers(mach_parsed_kwargs):
    """
    If the necessary TestCase is not in test_handlers,
    tests are omitted silently
    """
    harness = MarionetteHarness(args=mach_parsed_kwargs)
    mach_parsed_kwargs.pop('tests')
    runner = harness._runner_class(**mach_parsed_kwargs)
    assert marionette_test.MarionetteTestCase in runner.test_handlers
    assert marionette_test.MarionetteJSTestCase in runner.test_handlers


def test_parsing_testvars(mach_parsed_kwargs):
    mach_parsed_kwargs.pop('tests')
    testvars_json_loads = [
        {"wifi":{"ssid": "blah", "keyManagement": "WPA-PSK", "psk": "foo"}},
        {"wifi":{"PEAP": "bar"}, "device": {"stuff": "buzz"}}
    ]
    expected_dict = {
         "wifi": {
             "ssid": "blah",
             "keyManagement": "WPA-PSK",
             "psk": "foo",
             "PEAP":"bar"
         },
         "device": {"stuff":"buzz"}
    }
    with patch('marionette.runtests.MarionetteTestRunner._load_testvars') as load:
        load.return_value = testvars_json_loads
        runner = MarionetteTestRunner(**mach_parsed_kwargs)
        assert runner.testvars == expected_dict
        assert load.call_count == 1

if __name__ == '__main__':
    import sys
    sys.exit(pytest.main(['--verbose', __file__]))
