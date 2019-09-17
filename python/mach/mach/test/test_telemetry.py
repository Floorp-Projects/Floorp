# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import, print_function

import json
import os
import subprocess
import sys

import buildconfig
import mozunit
import pytest
from six import text_type

from mozboot.bootstrap import update_or_create_build_telemetry_config

TELEMETRY_LOAD_ERROR = '''
Error loading telemetry. mach output:
=========================================================
%s
=========================================================
'''


@pytest.fixture
def run_mach(tmpdir):
    """Return a function that runs mach with the provided arguments and then returns
    a list of the data contained within any telemetry entries generated during the command.
    """
    # Use tmpdir as the mozbuild state path, and enable telemetry in
    # a machrc there.
    update_or_create_build_telemetry_config(text_type(tmpdir.join('machrc')))
    env = dict(os.environ)
    env[b'MOZBUILD_STATE_PATH'] = str(tmpdir)
    env[b'MACH_TELEMETRY_NO_SUBMIT'] = b'1'
    # Let whatever mach command we invoke from tests believe it's the main command.
    del env['MACH_MAIN_PID']
    mach = os.path.join(buildconfig.topsrcdir, 'mach')

    def run(*args, **kwargs):
        # Run mach with the provided arguments
        out = subprocess.check_output([sys.executable, mach] + list(args),
                                      stderr=subprocess.STDOUT,
                                      env=env,
                                      **kwargs)
        # Load any telemetry data that was written
        path = tmpdir.join('telemetry', 'outgoing')
        try:
            return [json.load(f.open('rb')) for f in path.listdir()]
        except EnvironmentError:
            print(TELEMETRY_LOAD_ERROR % out, file=sys.stderr)
            for p in path.parts(reverse=True):
                if not p.check(dir=1):
                    print('Path does not exist: "%s"' % p, file=sys.stderr)
            raise
    return run


def test_simple(run_mach, tmpdir):
    data = run_mach('python', '-c', 'pass')
    assert len(data) == 1
    d = data[0]
    assert d['command'] == 'python'
    assert d['argv'] == ['-c', 'pass']
    client_id_data = json.load(tmpdir.join('telemetry_client_id.json').open('rb'))
    assert 'client_id' in client_id_data
    assert client_id_data['client_id'] == d['client_id']


def test_path_filtering(run_mach, tmpdir):
    srcdir_path = os.path.join(buildconfig.topsrcdir, 'a')
    srcdir_path_2 = os.path.join(buildconfig.topsrcdir, 'a/b/c')
    objdir_path = os.path.join(buildconfig.topobjdir, 'x')
    objdir_path_2 = os.path.join(buildconfig.topobjdir, 'x/y/z')
    home_path = os.path.join(os.path.expanduser('~'), 'something_in_home')
    other_path = str(tmpdir.join('other'))
    data = run_mach('python', '-c', 'pass',
                    srcdir_path, srcdir_path_2,
                    objdir_path, objdir_path_2,
                    home_path,
                    other_path,
                    cwd=buildconfig.topsrcdir)
    assert len(data) == 1
    d = data[0]
    expected = [
        '-c', 'pass',
        'a', 'a/b/c',
        '$topobjdir/x', '$topobjdir/x/y/z',
        '$HOME/something_in_home',
        '<path omitted>',
    ]
    assert d['argv'] == expected


def test_path_filtering_in_objdir(run_mach, tmpdir):
    srcdir_path = os.path.join(buildconfig.topsrcdir, 'a')
    srcdir_path_2 = os.path.join(buildconfig.topsrcdir, 'a/b/c')
    objdir_path = os.path.join(buildconfig.topobjdir, 'x')
    objdir_path_2 = os.path.join(buildconfig.topobjdir, 'x/y/z')
    other_path = str(tmpdir.join('other'))
    data = run_mach('python', '-c', 'pass',
                    srcdir_path, srcdir_path_2,
                    objdir_path, objdir_path_2,
                    other_path,
                    cwd=buildconfig.topobjdir)
    assert len(data) == 1
    d = data[0]
    expected = [
        '-c', 'pass',
        '$topsrcdir/a', '$topsrcdir/a/b/c',
        'x', 'x/y/z',
        '<path omitted>',
    ]
    assert d['argv'] == expected


def test_path_filtering_other_cwd(run_mach, tmpdir):
    srcdir_path = os.path.join(buildconfig.topsrcdir, 'a')
    srcdir_path_2 = os.path.join(buildconfig.topsrcdir, 'a/b/c')
    other_path = str(tmpdir.join('other'))
    data = run_mach('python', '-c', 'pass',
                    srcdir_path,
                    srcdir_path_2,
                    other_path, cwd=str(tmpdir))
    assert len(data) == 1
    d = data[0]
    expected = [
        # non-path arguments should escape unscathed
        '-c', 'pass',
        '$topsrcdir/a', '$topsrcdir/a/b/c',
        # cwd-relative paths should be relativized
        'other',
    ]
    assert d['argv'] == expected


def test_mach_invoke_recursive(run_mach):
    data = run_mach('python',
                    os.path.join(os.path.dirname(__file__), 'invoke_mach_command.py'),
                    os.path.join(buildconfig.topsrcdir, 'mach'),
                    'uuid')
    assert len(data) == 1
    d = data[0]
    assert d['command'] == 'python'


def test_registrar_dispatch(run_mach):
    # Use --exec-file so this script can use Registrar.dispatch to dispatch a mach command
    # from within the same interpreter as the `mach python` command.
    data = run_mach('python', '--exec-file',
                    os.path.join(os.path.dirname(__file__), 'registrar_dispatch.py'))
    assert len(data) == 1
    d = data[0]
    assert d['command'] == 'python'


def test_zero_microseconds(run_mach):
    data = run_mach('python', '--exec-file',
                    os.path.join(os.path.dirname(__file__), 'zero_microseconds.py'))
    d = data[0]
    assert d['command'] == 'python'


if __name__ == '__main__':
    mozunit.main()
