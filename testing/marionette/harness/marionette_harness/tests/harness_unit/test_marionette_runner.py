# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import manifestparser
import pytest

from mock import Mock, patch, mock_open, sentinel, DEFAULT

from marionette_harness.runtests import MarionetteTestRunner


@pytest.fixture
def runner(mach_parsed_kwargs):
    """
    MarionetteTestRunner instance initialized with default options.
    """
    return MarionetteTestRunner(**mach_parsed_kwargs)


@pytest.fixture
def mock_runner(runner, mock_marionette, monkeypatch):
    """
    MarionetteTestRunner instance with mocked-out
    self.marionette and other properties,
    to enable testing runner.run_tests().
    """
    runner.driverclass = mock_marionette
    for attr in ['run_test_set', '_capabilities']:
        setattr(runner, attr, Mock())
    runner._appName = 'fake_app'
    # simulate that browser runs with e10s by default
    runner._appinfo = {'browserTabsRemoteAutostart': True}
    monkeypatch.setattr('marionette_harness.runner.base.mozversion', Mock())
    return runner


@pytest.fixture
def build_kwargs_using(mach_parsed_kwargs):
    '''Helper function for test_build_kwargs_* functions'''
    def kwarg_builder(new_items, return_socket=False):
        mach_parsed_kwargs.update(new_items)
        runner = MarionetteTestRunner(**mach_parsed_kwargs)
        with patch('marionette_harness.runner.base.socket') as socket:
            built_kwargs = runner._build_kwargs()
        if return_socket:
            return built_kwargs, socket
        return built_kwargs
    return kwarg_builder


@pytest.fixture
def expected_driver_args(runner):
    '''Helper fixture for tests of _build_kwargs
    with binary/emulator.
    Provides a dictionary of certain arguments
    related to binary/emulator settings
    which we expect to be passed to the
    driverclass constructor. Expected values can
    be updated in tests as needed.
    Provides convenience methods for comparing the
    expected arguments to the argument dictionary
    created by _build_kwargs. '''

    class ExpectedDict(dict):
        def assert_matches(self, actual):
            for (k, v) in self.items():
                assert actual[k] == v

        def assert_keys_not_in(self, actual):
            for k in self.keys():
                assert k not in actual

    expected = ExpectedDict(host=None, port=None, bin=None)
    for attr in ['app', 'app_args', 'profile', 'addons', 'gecko_log']:
        expected[attr] = getattr(runner, attr)
    return expected


class ManifestFixture:
    def __init__(self, name='mock_manifest',
                       tests=[{'path': u'test_something.py', 'expected': 'pass'}]):
        self.filepath = "/path/to/fake/manifest.ini"
        self.n_disabled = len([t for t in tests if 'disabled' in t])
        self.n_enabled = len(tests) - self.n_disabled
        mock_manifest = Mock(spec=manifestparser.TestManifest,
                             active_tests=Mock(return_value=tests))
        self.manifest_class = Mock(return_value=mock_manifest)
        self.__repr__ = lambda: "<ManifestFixture {}>".format(name)

@pytest.fixture
def manifest():
    return ManifestFixture()

@pytest.fixture(params=['enabled', 'disabled', 'enabled_disabled', 'empty'])
def manifest_with_tests(request):
    '''
    Fixture for the contents of mock_manifest, where a manifest
    can include enabled tests, disabled tests, both, or neither (empty)
    '''
    included = []
    if 'enabled' in request.param:
        included += [(u'test_expected_pass.py', 'pass'),
                     (u'test_expected_fail.py', 'fail')]
    if 'disabled' in request.param:
        included += [(u'test_pass_disabled.py', 'pass', 'skip-if: true'),
                     (u'test_fail_disabled.py', 'fail', 'skip-if: true')]
    keys = ('path', 'expected', 'disabled')
    active_tests = [dict(zip(keys, values)) for values in included]

    return ManifestFixture(request.param, active_tests)


def test_args_passed_to_driverclass(mock_runner):
    built_kwargs = {'arg1': 'value1', 'arg2': 'value2'}
    mock_runner._build_kwargs = Mock(return_value=built_kwargs)
    with pytest.raises(IOError):
        mock_runner.run_tests(['fake_tests.ini'])
    assert mock_runner.driverclass.call_args[1] == built_kwargs


def test_build_kwargs_basic_args(build_kwargs_using):
    '''Test the functionality of runner._build_kwargs:
    make sure that basic arguments (those which should
    always be included, irrespective of the runner's settings)
    get passed to the call to runner.driverclass'''

    basic_args = ['socket_timeout', 'prefs',
                  'startup_timeout', 'verbose', 'symbols_path']
    args_dict = {a: getattr(sentinel, a) for a in basic_args}
    # Mock an update method to work with calls to MarionetteTestRunner()
    args_dict['prefs'].update = Mock(return_value={})
    built_kwargs = build_kwargs_using([(a, getattr(sentinel, a)) for a in basic_args])
    for arg in basic_args:
        assert built_kwargs[arg] is getattr(sentinel, arg)


@pytest.mark.parametrize('workspace', ['path/to/workspace', None])
def test_build_kwargs_with_workspace(build_kwargs_using, workspace):
    built_kwargs = build_kwargs_using({'workspace': workspace})
    if workspace:
        assert built_kwargs['workspace'] == workspace
    else:
        assert 'workspace' not in built_kwargs


@pytest.mark.parametrize('address', ['host:123', None])
def test_build_kwargs_with_address(build_kwargs_using, address):
    built_kwargs, socket = build_kwargs_using(
        {'address': address, 'binary': None, 'emulator': None},
        return_socket=True
    )
    assert 'connect_to_running_emulator' not in built_kwargs
    if address is not None:
        host, port = address.split(":")
        assert built_kwargs['host'] == host and built_kwargs['port'] == int(port)
        socket.socket().connect.assert_called_with((host, int(port)))
        assert socket.socket().close.called
    else:
        assert not socket.socket.called


@pytest.mark.parametrize('address', ['host:123', None])
@pytest.mark.parametrize('binary', ['path/to/bin', None])
def test_build_kwargs_with_binary_or_address(expected_driver_args, build_kwargs_using,
                                             binary, address):
    built_kwargs = build_kwargs_using({'binary': binary, 'address': address, 'emulator': None})
    if binary:
        expected_driver_args['bin'] = binary
        if address:
            host, port = address.split(":")
            expected_driver_args.update({'host': host, 'port': int(port)})
        else:
            expected_driver_args.update({'host': 'localhost', 'port': 2828})
        expected_driver_args.assert_matches(built_kwargs)
    elif address is None:
        expected_driver_args.assert_keys_not_in(built_kwargs)


@pytest.mark.parametrize('address', ['host:123', None])
@pytest.mark.parametrize('emulator', [True, False, None])
def test_build_kwargs_with_emulator_or_address(expected_driver_args, build_kwargs_using,
                                               emulator, address):
    emulator_props = [(a, getattr(sentinel, a)) for a in ['avd_home', 'adb_path', 'emulator_bin']]
    built_kwargs = build_kwargs_using(
        [('emulator', emulator),  ('address', address), ('binary', None)] + emulator_props
    )
    if emulator:
        expected_driver_args.update(emulator_props)
        expected_driver_args['emulator_binary'] = expected_driver_args.pop('emulator_bin')
        expected_driver_args['bin'] = True
        if address:
            expected_driver_args['connect_to_running_emulator'] = True
            host, port = address.split(":")
            expected_driver_args.update({'host': host, 'port': int(port)})
        else:
            expected_driver_args.update({'host': 'localhost', 'port': 2828})
            assert 'connect_to_running_emulator' not in built_kwargs
        expected_driver_args.assert_matches(built_kwargs)
    elif not address:
        expected_driver_args.assert_keys_not_in(built_kwargs)


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
        'marionette_harness.runtests.MarionetteTestRunner._load_testvars',
        return_value=testvars_json_loads
    ) as load:
        runner = MarionetteTestRunner(**mach_parsed_kwargs)
        assert runner.testvars == expected_dict
        assert load.call_count == 1


def test_load_testvars_throws_expected_errors(mach_parsed_kwargs):
    mach_parsed_kwargs['testvars'] = ['some_bad_path.json']
    runner = MarionetteTestRunner(**mach_parsed_kwargs)
    with pytest.raises(IOError) as io_exc:
        runner._load_testvars()
    assert 'does not exist' in io_exc.value.message
    with patch('os.path.exists', return_value=True):
        with patch('__builtin__.open', mock_open(read_data='[not {valid JSON]')):
            with pytest.raises(Exception) as json_exc:
                runner._load_testvars()
    assert 'not properly formatted' in json_exc.value.message


def _check_crash_counts(has_crashed, runner, mock_marionette):
    if has_crashed:
        assert mock_marionette.check_for_crash.call_count == 1
        assert runner.crashed == 1
    else:
        assert runner.crashed == 0


@pytest.mark.parametrize("has_crashed", [True, False])
def test_increment_crash_count_in_run_test_set(runner, has_crashed,
                                               mock_marionette):
    fake_tests = [{'filepath': i,
                   'expected': 'pass'} for i in 'abc']

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
        with patch('os.path.abspath', return_value=test) as abspath:
            runner.add_test(test)
        assert abspath.called
        expected = {'filepath': test, 'expected': 'pass', 'group': 'default'}
        assert expected in runner.tests
    # add_test doesn't validate module names; 'bad_test.py' gets through
    assert len(runner.tests) == 3


def test_add_test_directory(runner):
    test_dir = 'path/to/tests'
    dir_contents = [
        (test_dir, ('subdir',), ('test_a.py', 'bad_test_a.py')),
        (test_dir + '/subdir', (), ('test_b.py', 'bad_test_b.py')),
    ]
    tests = list(dir_contents[0][2] + dir_contents[1][2])
    assert len(runner.tests) == 0
    # Need to use side effect to make isdir return True for test_dir and False for tests
    with patch('os.path.isdir', side_effect=[True] + [False for t in tests]) as isdir:
        with patch('os.walk', return_value=dir_contents) as walk:
            runner.add_test(test_dir)
    assert isdir.called and walk.called
    for test in runner.tests:
        assert test_dir in test['filepath']
    assert len(runner.tests) == 2


@pytest.mark.parametrize("test_files_exist", [True, False])
def test_add_test_manifest(mock_runner, manifest_with_tests, monkeypatch, test_files_exist):
    monkeypatch.setattr('marionette_harness.runner.base.TestManifest',
                        manifest_with_tests.manifest_class)
    mock_runner.marionette = mock_runner.driverclass()
    with patch('marionette_harness.runner.base.os.path.exists', return_value=test_files_exist):
        if test_files_exist or manifest_with_tests.n_enabled == 0:
            mock_runner.add_test(manifest_with_tests.filepath)
            assert len(mock_runner.tests) == manifest_with_tests.n_enabled
            assert len(mock_runner.manifest_skipped_tests) == manifest_with_tests.n_disabled
            for test in mock_runner.tests:
                assert test['filepath'].endswith(test['expected'] + '.py')
        else:
            pytest.raises(IOError, "mock_runner.add_test(manifest_with_tests.filepath)")
    assert manifest_with_tests.manifest_class().read.called
    assert manifest_with_tests.manifest_class().active_tests.called


def get_kwargs_passed_to_manifest(mock_runner, manifest, monkeypatch, **kwargs):
    '''Helper function for test_manifest_* tests.
    Returns the kwargs passed to the call to manifest.active_tests.'''
    monkeypatch.setattr('marionette_harness.runner.base.TestManifest', manifest.manifest_class)
    monkeypatch.setattr('marionette_harness.runner.base.mozinfo.info',
                        {'mozinfo_key': 'mozinfo_val'})
    for attr in kwargs:
        setattr(mock_runner, attr, kwargs[attr])
    mock_runner.marionette = mock_runner.driverclass()
    with patch('marionette_harness.runner.base.os.path.exists', return_value=True):
        mock_runner.add_test(manifest.filepath)
    call_args, call_kwargs = manifest.manifest_class().active_tests.call_args
    return call_kwargs


def test_manifest_basic_args(mock_runner, manifest, monkeypatch):
    kwargs = get_kwargs_passed_to_manifest(mock_runner, manifest, monkeypatch)
    assert kwargs['exists'] is False
    assert kwargs['disabled'] is True
    assert kwargs['appname'] == 'fake_app'
    assert 'mozinfo_key' in kwargs and kwargs['mozinfo_key'] == 'mozinfo_val'


@pytest.mark.parametrize('e10s', (True, False))
def test_manifest_with_e10s(mock_runner, manifest, monkeypatch, e10s):
    kwargs = get_kwargs_passed_to_manifest(mock_runner, manifest, monkeypatch, e10s=e10s)
    assert kwargs['e10s'] == e10s


@pytest.mark.parametrize('test_tags', (None, ['tag', 'tag2']))
def test_manifest_with_test_tags(mock_runner, manifest, monkeypatch, test_tags):
    kwargs = get_kwargs_passed_to_manifest(mock_runner, manifest, monkeypatch, test_tags=test_tags)
    if test_tags is None:
        assert kwargs['filters'] == []
    else:
        assert len(kwargs['filters']) == 1 and kwargs['filters'][0].tags == test_tags


def test_cleanup_with_manifest(mock_runner, manifest_with_tests, monkeypatch):
    monkeypatch.setattr('marionette_harness.runner.base.TestManifest',
                        manifest_with_tests.manifest_class)
    if manifest_with_tests.n_enabled > 0:
        context = patch('marionette_harness.runner.base.os.path.exists', return_value=True)
    else:
        context = pytest.raises(Exception)
    with context:
        mock_runner.run_tests([manifest_with_tests.filepath])
    assert mock_runner.marionette is None
    assert mock_runner.fixture_servers == {}


def test_reset_test_stats(mock_runner):
    def reset_successful(runner):
        stats = ['passed', 'failed', 'unexpected_successes', 'todo', 'skipped', 'failures']
        return all([((s in vars(runner)) and (not vars(runner)[s])) for s in stats])
    assert reset_successful(mock_runner)
    mock_runner.passed = 1
    mock_runner.failed = 1
    mock_runner.failures.append(['TEST-UNEXPECTED-FAIL'])
    assert not reset_successful(mock_runner)
    mock_runner.run_tests([u'test_fake_thing.py'])
    assert reset_successful(mock_runner)


def test_initialize_test_run(mock_runner):
    tests = [u'test_fake_thing.py']
    mock_runner.reset_test_stats = Mock()
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
    mock_runner.run_tests(fake_tests)
    assert len(mock_runner.tests) == 3
    for (test_name, added_test) in zip(fake_tests, mock_runner.tests):
        assert added_test['filepath'].endswith(test_name)


def test_catch_invalid_test_names(runner):
    good_tests = [u'test_ok.py', u'test_is_ok.py']
    bad_tests = [u'bad_test.py', u'testbad.py', u'_test_bad.py',
                 u'test_bad.notpy', u'test_bad',
                 u'test.py', u'test_.py']
    with pytest.raises(Exception) as exc:
        runner._add_tests(good_tests + bad_tests)
    msg = exc.value.message
    assert "Test file names must be of the form" in msg
    for bad_name in bad_tests:
        assert bad_name in msg
    for good_name in good_tests:
        assert good_name not in msg

@pytest.mark.parametrize('e10s', (True, False))
def test_e10s_option_sets_prefs(mach_parsed_kwargs, e10s):
    mach_parsed_kwargs['e10s'] = e10s
    runner = MarionetteTestRunner(**mach_parsed_kwargs)
    e10s_prefs = {
        'browser.tabs.remote.autostart': True,
        'browser.tabs.remote.force-enable': True,
        'extensions.e10sBlocksEnabling': False
    }
    for k,v in e10s_prefs.iteritems():
        if k == 'extensions.e10sBlocksEnabling' and not e10s:
            continue
        assert runner.prefs.get(k, False) == (v and e10s)

def test_e10s_option_clash_raises(mock_runner):
    mock_runner.e10s = False
    with pytest.raises(AssertionError) as e:
        mock_runner.run_tests([u'test_fake_thing.py'])
        assert "configuration (self.e10s) does not match browser appinfo" in e.value.message

if __name__ == '__main__':
    import sys
    sys.exit(pytest.main(
        ['--log-tbpl=-', __file__]))
