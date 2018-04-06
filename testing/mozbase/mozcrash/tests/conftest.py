from __future__ import absolute_import

import uuid

import pytest
from py._path.common import fspath

import mozcrash


@pytest.fixture(scope='session')
def stackwalk(tmpdir_factory):
    stackwalk = tmpdir_factory.mktemp('stackwalk_binary').join('stackwalk')
    stackwalk.write('fake binary')
    stackwalk.chmod(0o744)
    return stackwalk


@pytest.fixture
def check_for_crashes(tmpdir, stackwalk, monkeypatch):
    monkeypatch.delenv('MINIDUMP_SAVE_PATH', raising=False)

    def wrapper(dump_directory=fspath(tmpdir),
                symbols_path='symbols_path',
                stackwalk_binary=fspath(stackwalk),
                dump_save_path=None,
                test_name=None,
                quiet=True):
        return mozcrash.check_for_crashes(dump_directory,
                                          symbols_path,
                                          stackwalk_binary,
                                          dump_save_path,
                                          test_name,
                                          quiet)

    return wrapper


@pytest.fixture
def check_for_java_exception():

    def wrapper(logcat=None,
                test_name=None,
                quiet=True):
        return mozcrash.check_for_java_exception(logcat,
                                                 test_name,
                                                 quiet)

    return wrapper


@pytest.fixture
def minidump_files(request, tmpdir):
    files = []

    for i in range(getattr(request, 'param', 1)):
        name = uuid.uuid4()

        dmp = tmpdir.join('{}.dmp'.format(name))
        dmp.write('foo')

        extra = tmpdir.join('{}.extra'.format(name))
        extra.write('bar')

        files.append({'dmp': dmp, 'extra': extra})

    return files


@pytest.fixture(autouse=True)
def mock_popen(monkeypatch):
    """Generate a class that can mock subprocess.Popen.

    :param stdouts: Iterable that should return an iterable for the
                    stdout of each process in turn.
    """
    class MockPopen(object):
        def __init__(self, args, *args_rest, **kwargs):
            # all_popens.append(self)
            self.args = args
            self.returncode = 0

        def communicate(self):
            return (u'Stackwalk command: {}'.format(" ".join(self.args)), "")

        def wait(self):
            return self.returncode

    monkeypatch.setattr(mozcrash.mozcrash.subprocess, 'Popen', MockPopen)
