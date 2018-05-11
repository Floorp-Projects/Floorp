# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import platform
import signal
import subprocess
import sys
import time

import mozunit
import pytest

from mozlint import ResultContainer
from mozlint.errors import LintersNotConfigured


here = os.path.abspath(os.path.dirname(__file__))


linters = ('string.yml', 'regex.yml', 'external.yml')


def test_roll_no_linters_configured(lint, files):
    with pytest.raises(LintersNotConfigured):
        lint.roll(files)


def test_roll_successful(lint, linters, files):
    lint.read(linters)

    assert lint.results is None
    result = lint.roll(files)
    assert len(result) == 1
    assert lint.results == result
    assert lint.failed == set([])

    path = result.keys()[0]
    assert os.path.basename(path) == 'foobar.js'

    errors = result[path]
    assert isinstance(errors, list)
    assert len(errors) == 6

    container = errors[0]
    assert isinstance(container, ResultContainer)
    assert container.rule == 'no-foobar'


def test_roll_from_subdir(lint, linters):
    lint.read(linters)

    oldcwd = os.getcwd()
    try:
        os.chdir(os.path.join(lint.root, 'files'))

        # Path relative to cwd works
        result = lint.roll('no_foobar.js')
        assert len(result) == 0
        assert len(lint.failed) == 0

        # Path relative to root doesn't work
        result = lint.roll(os.path.join('files', 'no_foobar.js'))
        assert len(result) == 0
        assert len(lint.failed) == 3

        # Paths from vcs are always joined to root instead of cwd
        lint.mock_vcs([os.path.join('files', 'no_foobar.js')])
        result = lint.roll(outgoing=True)
        assert len(result) == 0
        assert len(lint.failed) == 0

        result = lint.roll(workdir=True)
        assert len(result) == 0
        assert len(lint.failed) == 0
    finally:
        os.chdir(oldcwd)


def test_roll_catch_exception(lint, lintdir, files, capfd):
    lint.read(os.path.join(lintdir, 'raises.yml'))

    lint.roll(files)  # assert not raises
    out, err = capfd.readouterr()
    assert 'LintException' in err


def test_roll_with_excluded_path(lint, linters, files):
    lint.lintargs.update({'exclude': ['**/foobar.js']})

    lint.read(linters)
    result = lint.roll(files)

    assert len(result) == 0
    assert lint.failed == set([])


def test_roll_with_invalid_extension(lint, lintdir, filedir):
    lint.read(os.path.join(lintdir, 'external.yml'))
    result = lint.roll(os.path.join(filedir, 'foobar.py'))
    assert len(result) == 0
    assert lint.failed == set([])


def test_roll_with_failure_code(lint, lintdir, files):
    lint.read(os.path.join(lintdir, 'badreturncode.yml'))

    assert lint.failed is None
    result = lint.roll(files, num_procs=1)
    assert len(result) == 0
    assert lint.failed == set(['BadReturnCodeLinter'])


def fake_run_worker(config, paths, **lintargs):
    return {'count': [1]}, []


@pytest.mark.skipif(platform.system() == 'Windows',
                    reason="monkeypatch issues with multiprocessing on Windows")
@pytest.mark.parametrize('num_procs', [1, 4, 8, 16])
def test_number_of_jobs(monkeypatch, lint, linters, files, num_procs):
    monkeypatch.setattr(sys.modules[lint.__module__], '_run_worker', fake_run_worker)

    lint.read(linters)
    num_jobs = len(lint.roll(files, num_procs=num_procs)['count'])

    if len(files) >= num_procs:
        assert num_jobs == num_procs * len(linters)
    else:
        assert num_jobs == len(files) * len(linters)


@pytest.mark.skipif(platform.system() == 'Windows',
                    reason="monkeypatch issues with multiprocessing on Windows")
@pytest.mark.parametrize('max_paths,expected_jobs', [(1, 12), (4, 6), (16, 6)])
def test_max_paths_per_job(monkeypatch, lint, linters, files, max_paths, expected_jobs):
    monkeypatch.setattr(sys.modules[lint.__module__], '_run_worker', fake_run_worker)

    files = files[:4]
    assert len(files) == 4

    linters = linters[:3]
    assert len(linters) == 3

    lint.MAX_PATHS_PER_JOB = max_paths
    lint.read(linters)
    num_jobs = len(lint.roll(files, num_procs=2)['count'])
    assert num_jobs == expected_jobs


@pytest.mark.skipif(platform.system() == 'Windows',
                    reason="signal.CTRL_C_EVENT isn't causing a KeyboardInterrupt on Windows")
def test_keyboard_interrupt():
    # We use two linters so we'll have two jobs. One (string.yml) will complete
    # quickly. The other (slow.yml) will run slowly.  This way the first worker
    # will be be stuck blocking on the ProcessPoolExecutor._call_queue when the
    # signal arrives and the other still be doing work.
    cmd = [sys.executable, 'runcli.py', '-l=string', '-l=slow']
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=here)
    time.sleep(1)
    proc.send_signal(signal.SIGINT)

    out = proc.communicate()[0]
    assert 'warning: not all files were linted' in out
    assert 'Traceback' not in out


def test_support_files(lint, lintdir, filedir, monkeypatch):
    jobs = []

    # Replace the original _generate_jobs with a new one that simply
    # adds jobs to a list (and then doesn't return anything).
    orig_generate_jobs = lint._generate_jobs

    def fake_generate_jobs(*args, **kwargs):
        jobs.extend([job[1] for job in orig_generate_jobs(*args, **kwargs)])
        return []

    monkeypatch.setattr(lint, '_generate_jobs', fake_generate_jobs)

    linter_path = os.path.join(lintdir, 'support_files.yml')
    lint.read(linter_path)

    # Modified support files only lint entire root if --outgoing or --workdir
    # are used.
    path = os.path.join(filedir, 'foobar.js')
    lint.mock_vcs([os.path.join(filedir, 'foobar.py')])
    lint.roll(path)
    assert jobs[0] == [path]

    jobs = []
    lint.roll(path, workdir=True)
    assert jobs[0] == [lint.root]

    jobs = []
    lint.roll(path, outgoing=True)
    assert jobs[0] == [lint.root]

    # Lint config file is implicitly added as a support file
    lint.mock_vcs([linter_path])
    jobs = []
    lint.roll(path, outgoing=True, workdir=True)
    assert jobs[0] == [lint.root]


linters = ('setup.yml', 'setupfailed.yml', 'setupraised.yml')


def test_setup(lint, linters, filedir, capfd):
    with pytest.raises(LintersNotConfigured):
        lint.setup()

    lint.read(linters)
    lint.setup()
    out, err = capfd.readouterr()
    assert 'setup passed' in out
    assert 'setup failed' in out
    assert 'setup raised' in out
    assert 'error: problem with lint setup, skipping' in out
    assert lint.failed_setup == set(['SetupFailedLinter', 'SetupRaisedLinter'])


if __name__ == '__main__':
    mozunit.main()
