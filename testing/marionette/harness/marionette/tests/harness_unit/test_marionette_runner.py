# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import pytest
from mock import patch, Mock, DEFAULT, mock_open, MagicMock

from marionette.runtests import (
    MarionetteTestRunner,
    MarionetteHarness,
    MarionetteArguments,
    cli
)
from marionette.runner import MarionetteTestResult
from marionette_driver.marionette import Marionette

# avoid importing MarionetteJSTestCase to prevent pytest from
# collecting and running it as part of this test suite
import marionette.marionette_test as marionette_test


def _check_crash_counts(has_crashed, runner, mock_marionette):
    if has_crashed:
        assert mock_marionette.check_for_crash.call_count == 1
        assert runner.crashed == 1
    else:
        assert runner.crashed == 0


@pytest.fixture()
def mock_marionette(request):
    """ Mock marionette instance """
    marionette = MagicMock(spec=Marionette)
    if 'has_crashed' in request.funcargnames:
        marionette.check_for_crash.return_value = request.getfuncargvalue(
            'has_crashed'
        )
    return marionette


@pytest.fixture()
def empty_marionette_testcase():
    """ Testable MarionetteTestCase class """
    class EmptyTestCase(marionette_test.MarionetteTestCase):
        def test_nothing(self):
            pass

    return EmptyTestCase


@pytest.fixture()
def empty_marionette_test(mock_marionette, empty_marionette_testcase):
    return empty_marionette_testcase(lambda: mock_marionette, 'test_nothing')


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
        'adb_path': None,
        'addon': None,
        'address': None,
        'app': None,
        'app_args': [],
        'avd': None,
        'avd_home': None,
        'binary': u'/path/to/firefox',
        'device_serial': None,
        'e10s': True,
        'emulator': False,
        'emulator_bin': None,
        'gecko_log': None,
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
        'log_tbpl_compact': None,
        'log_tbpl_level': None,
        'log_unittest': None,
        'log_xunit': None,
        'logger_name': 'Marionette-based Tests',
        'prefs': {
            'browser.tabs.remote.autostart': True,
            'browser.tabs.remote.force-enable': True,
            'extensions.e10sBlocksEnabling': False,
        },
        'prefs_args': None,
        'prefs_files': None,
        'profile': None,
        'pydebugger': None,
        'repeat': 0,
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
        'verbose': None,
        'workspace': None,
        'logger': logger,
    }


@pytest.fixture()
def runner(mach_parsed_kwargs):
    """
    MarionetteTestRunner instance initialized with default options.
    """
    return MarionetteTestRunner(**mach_parsed_kwargs)


@pytest.fixture()
def mock_runner(runner, mock_marionette):
    """
    MarionetteTestRunner instance with mocked-out
    self.marionette and other properties.
    """
    runner.driverclass = mock_marionette
    runner._set_baseurl = Mock()
    runner.run_test_set = Mock()
    return runner


@pytest.fixture
def harness_class(request):
    """
    Mock based on MarionetteHarness whose run method just returns a number of
    failures according to the supplied test parameter
    """
    if 'num_fails_crashed' in request.funcargnames:
        num_fails_crashed = request.getfuncargvalue('num_fails_crashed')
    else:
        num_fails_crashed = (0, 0)
    harness_cls = Mock(spec=MarionetteHarness)
    harness = harness_cls.return_value
    if num_fails_crashed is None:
        harness.run.side_effect = Exception
    else:
        harness.run.return_value = sum(num_fails_crashed)
    return harness_cls


@pytest.fixture
def runner_class(request):
    """
    Mock based on MarionetteTestRunner, wherein the runner.failed,
    runner.crashed attributes are provided by a test parameter
    """
    if 'num_fails_crashed' in request.funcargnames:
        failures, crashed = request.getfuncargvalue('num_fails_crashed')
    else:
        failures = 0
        crashed = 0
    mock_runner_class = Mock(spec=MarionetteTestRunner)
    runner = mock_runner_class.return_value
    runner.failed = failures
    runner.crashed = crashed
    return mock_runner_class


@pytest.mark.parametrize(
    "num_fails_crashed,exit_code",
    [((0, 0), 0), ((1, 0), 10), ((0, 1), 10), (None, 1)],
)
def test_cli_exit_code(num_fails_crashed, exit_code, harness_class):
    with pytest.raises(SystemExit) as err:
        cli(harness_class=harness_class)
    assert err.value.code == exit_code


@pytest.mark.parametrize("num_fails_crashed", [(0, 0), (1, 0), (1, 1)])
def test_call_harness_with_parsed_args_yields_num_failures(mach_parsed_kwargs,
                                                           runner_class,
                                                           num_fails_crashed):
    with patch(
        'marionette.runtests.MarionetteHarness.parse_args'
    ) as parse_args:
        failed_or_crashed = MarionetteHarness(runner_class,
                                              args=mach_parsed_kwargs).run()
        parse_args.assert_not_called()
    assert failed_or_crashed == sum(num_fails_crashed)


@pytest.mark.parametrize("sock_timeout_value", ['A', '10', '1B-', '1C2', '44.35'])
def test_parse_arg_socket_timeout_with_multiple_values(sock_timeout_value):
    argv = ['marionette', '--socket-timeout', sock_timeout_value]
    parser = MarionetteArguments()

    def _is_float_convertible(value):
        try:
            float(value)
            return True
        except:
            return False

    if not _is_float_convertible(sock_timeout_value):
        # should raising exception, since sock_timeout must be convertible to float.
        with pytest.raises(SystemExit) as ex:
            parser.parse_args(args=argv)
        assert ex.value.code == 2
    else:
        # should pass without raising exception.
        args = parser.parse_args(args=argv)
        assert hasattr(args, 'socket_timeout') and args.socket_timeout == float(sock_timeout_value)


def test_call_harness_with_no_args_yields_num_failures(runner_class):
    with patch(
        'marionette.runtests.MarionetteHarness.parse_args'
    ) as parse_args:
        parse_args.return_value = {'tests': []}
        failed_or_crashed = MarionetteHarness(runner_class).run()
        assert parse_args.call_count == 1
    assert failed_or_crashed == 0


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
        {"wifi": {"ssid": "blah", "keyManagement": "WPA-PSK", "psk": "foo"}},
        {"wifi": {"PEAP": "bar"}, "device": {"stuff": "buzz"}}
    ]
    expected_dict = {
         "wifi": {
             "ssid": "blah",
             "keyManagement": "WPA-PSK",
             "psk": "foo",
             "PEAP": "bar"
         },
         "device": {"stuff": "buzz"}
    }
    with patch(
        'marionette.runtests.MarionetteTestRunner._load_testvars'
    ) as load:
        load.return_value = testvars_json_loads
        runner = MarionetteTestRunner(**mach_parsed_kwargs)
        assert runner.testvars == expected_dict
        assert load.call_count == 1


def test_load_testvars_throws_expected_errors(mach_parsed_kwargs):
    mach_parsed_kwargs['testvars'] = ['some_bad_path.json']
    runner = MarionetteTestRunner(**mach_parsed_kwargs)
    with pytest.raises(IOError) as io_exc:
        runner._load_testvars()
    assert 'does not exist' in io_exc.value.message
    with patch('os.path.exists') as exists:
        exists.return_value = True
        with patch('__builtin__.open', mock_open(read_data='[not {valid JSON]')):
            with pytest.raises(Exception) as json_exc:
                runner._load_testvars()
    assert 'not properly formatted' in json_exc.value.message


@pytest.mark.parametrize("has_crashed", [True, False])
def test_crash_is_recorded_as_error(empty_marionette_test,
                                    logger,
                                    has_crashed):
    """ Number of errors is incremented by stopTest iff has_crashed is true """
    # collect results from the empty test
    result = MarionetteTestResult(
        marionette=empty_marionette_test._marionette_weakref(),
        logger=logger, verbosity=None,
        stream=None, descriptions=None,
    )
    result.startTest(empty_marionette_test)
    assert len(result.errors) == 0
    assert len(result.failures) == 0
    assert result.testsRun == 1
    assert result.shouldStop == False
    result.stopTest(empty_marionette_test)
    assert result.shouldStop == has_crashed
    if has_crashed:
        assert len(result.errors) == 1
    else:
        assert len(result.errors) == 0


@pytest.mark.parametrize("has_crashed", [True, False])
def test_increment_crash_count_in_run_test_set(runner, has_crashed,
                                               mock_marionette):
    fake_tests = [{'filepath': i,
                   'expected': 'pass',
                   'test_container': False} for i in 'abc']

    with patch.multiple(runner, run_test=DEFAULT, marionette=mock_marionette):
        runner.run_test_set(fake_tests)
        if not has_crashed:
            assert runner.marionette.check_for_crash.call_count == len(fake_tests)
        _check_crash_counts(has_crashed, runner, runner.marionette)


@pytest.mark.parametrize("has_crashed", [True, False])
def test_record_crash(runner, has_crashed, mock_marionette):
    with patch.object(runner, 'marionette', mock_marionette):
        assert runner.record_crash() == has_crashed
        _check_crash_counts(has_crashed, runner, runner.marionette)


def test_add_test_module(runner):
    tests = ['test_something.py', 'testSomething.js', 'bad_test.py']
    assert len(runner.tests) == 0
    for test in tests:
        with patch ('os.path.abspath') as abspath:
            abspath.return_value = test
            runner.add_test(test)
        assert abspath.called
        assert {'filepath': test,
                'expected': 'pass',
                'test_container': None} in runner.tests
    # add_test doesn't validate module names; 'bad_test.py' gets through
    assert len(runner.tests) == 3

def test_add_test_directory(runner):
    test_dir = 'path/to/tests'
    dir_contents = [
        (test_dir, ('subdir',), ('test_a.py', 'test_a.js', 'bad_test_a.py', 'bad_test_a.js')),
        (test_dir + '/subdir', (), ('test_b.py', 'test_b.js', 'bad_test_a.py', 'bad_test_b.js')),
    ]
    tests = list(dir_contents[0][2] + dir_contents[1][2])
    assert len(runner.tests) == 0
    with patch('os.path.isdir') as isdir:
        # Need to use side effect to make isdir return True for test_dir and False for tests
        isdir.side_effect = [True] + [False for i in tests]
        with patch('os.walk') as walk:
            walk.return_value = dir_contents
            runner.add_test(test_dir)
    assert isdir.called and walk.called
    for test in runner.tests:
        assert test_dir in test['filepath']
    assert len(runner.tests) == 4

def test_add_test_manifest(runner):
    runner._appName = 'fake_app'
    manifest = "/path/to/fake/manifest.ini"
    active_tests = [{'expected': 'pass',
                     'path': u'/path/to/fake/test_expected_pass.py'},
                    {'expected': 'fail',
                     'path': u'/path/to/fake/test_expected_fail.py'},
                    {'disabled': 'skip-if: true # "testing disabled test"',
                     'expected': 'pass',
                     'path': u'/path/to/fake/test_disabled.py'}]
    with patch.multiple('marionette.runner.base.TestManifest',
                        read=DEFAULT, active_tests=DEFAULT) as mocks:
            mocks['active_tests'].return_value = active_tests
            with pytest.raises(IOError) as err:
                runner.add_test(manifest)
            assert "does not exist" in err.value.message
            assert mocks['read'].call_count == mocks['active_tests'].call_count == 1
            args, kwargs = mocks['active_tests'].call_args
            assert kwargs['app'] == runner._appName
            runner.tests, runner.manifest_skipped_tests = [], []
            with patch('marionette.runner.base.os.path.exists', return_value=True):
                runner.add_test(manifest)
            assert mocks['read'].call_count == mocks['active_tests'].call_count == 2
    assert len(runner.tests) == 2
    assert len(runner.manifest_skipped_tests) == 1
    for test in runner.tests:
        assert test['filepath'].endswith(('test_expected_pass.py', 'test_expected_fail.py'))
        if test['filepath'].endswith('test_expected_fail.py'):
            assert test['expected'] == 'fail'
        else:
            assert test['expected'] == 'pass'


def test_cleanup_with_manifest(mock_runner):
    mock_runner._appName = 'fake_app'
    with patch.multiple('marionette.runner.base.TestManifest',
                        read=DEFAULT, active_tests=DEFAULT) as mocks:
        mocks['active_tests'].return_value = [{'expected':'pass', 'path':'test_something.py'}]
        with patch('marionette.runner.base.mozversion.get_version'):
            with patch('marionette.runner.base.os.path.exists', return_value=True):
                mock_runner.run_tests(['fake_manifest.ini'])
    assert mock_runner.marionette is None
    assert mock_runner.httpd is None

def test_cleanup_empty_manifest(mock_runner):
    mock_runner._appName = 'fake_app'
    with patch.multiple('marionette.runner.base.TestManifest',
                        read=DEFAULT, active_tests=DEFAULT) as mocks:
        mocks['active_tests'].return_value = []
        with patch('marionette.runner.base.mozversion.get_version'):
            with pytest.raises(Exception) as exc:
                mock_runner.run_tests(['fake_empty_manifest.ini'])
    assert "no tests to run" in exc.value.message
    assert mock_runner.marionette is None
    assert mock_runner.httpd is None


def test_reset_test_stats(runner):
    def reset_successful(runner):
        stats = ['passed', 'failed', 'unexpected_successes', 'todo', 'skipped', 'failures']
        return all([((s in vars(runner)) and (not vars(runner)[s])) for s in stats])
    assert reset_successful(runner)
    runner.passed = 1
    runner.failed = 1
    runner.failures.append(['TEST-UNEXPECTED-FAIL'])
    assert not reset_successful(runner)
    with pytest.raises(Exception):
        runner.run_tests([u'test_fake_thing.py'])
    assert reset_successful(runner)


def test_initialize_test_run(mock_runner):
    tests = [u'test_fake_thing.py']
    mock_runner.reset_test_stats = Mock()
    with patch('marionette.runner.base.mozversion.get_version'):
        mock_runner.run_tests(tests)
    assert mock_runner.reset_test_stats.called
    with pytest.raises(AssertionError) as test_exc:
        mock_runner.run_tests([])
    assert "len(tests)" in str(test_exc.traceback[-1].statement)
    with pytest.raises(AssertionError) as hndl_exc:
        mock_runner.test_handlers = []
        mock_runner.run_tests(tests)
    assert "test_handlers" in str(hndl_exc.traceback[-1].statement)
    assert mock_runner.reset_test_stats.call_count == 1


def test_add_tests(mock_runner):
    assert len(mock_runner.tests) == 0
    fake_tests = ["test_" + i + ".py" for i in "abc"]
    with patch('marionette.runner.base.mozversion.get_version'):
        mock_runner.run_tests(fake_tests)
    assert len(mock_runner.tests) == 3
    for (test_name, added_test) in zip(fake_tests, mock_runner.tests):
        assert added_test['filepath'].endswith(test_name)


def test_catch_invalid_test_names(runner):
    good_tests = [u'test_ok.py', u'test_is_ok.py', u'test_is_ok.js', u'testIsOk.js']
    bad_tests = [u'bad_test.py', u'testbad.py', u'_test_bad.py', u'testBad.notjs',
                 u'test_bad.notpy', u'test_bad', u'testbad.js', u'badtest.js',
                 u'test.py', u'test_.py', u'test.js', u'test_.js']
    with pytest.raises(Exception) as exc:
        runner._add_tests(good_tests + bad_tests)
    msg = exc.value.message
    assert "Test file names must be of the form" in msg
    for bad_name in bad_tests:
        assert bad_name in msg
    for good_name in good_tests:
        assert good_name not in msg


if __name__ == '__main__':
    import sys
    sys.exit(pytest.main(['--verbose', __file__]))
