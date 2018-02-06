# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import platform
import sys

import mozunit
import pytest

from mozlint import ResultContainer
from mozlint.errors import LintersNotConfigured, LintException


here = os.path.abspath(os.path.dirname(__file__))


linters = ('string.yml', 'regex.yml', 'external.yml')


def test_roll_no_linters_configured(lint, files):
    with pytest.raises(LintersNotConfigured):
        lint.roll(files)


def test_roll_successful(lint, linters, files):
    lint.read(linters)

    result = lint.roll(files)
    assert len(result) == 1
    assert lint.failed == set([])

    path = result.keys()[0]
    assert os.path.basename(path) == 'foobar.js'

    errors = result[path]
    assert isinstance(errors, list)
    assert len(errors) == 6

    container = errors[0]
    assert isinstance(container, ResultContainer)
    assert container.rule == 'no-foobar'


def test_roll_catch_exception(lint, lintdir, files):
    lint.read(os.path.join(lintdir, 'raises.yml'))

    # suppress printed traceback from test output
    old_stderr = sys.stderr
    sys.stderr = open(os.devnull, 'w')
    with pytest.raises(LintException):
        lint.roll(files)
    sys.stderr = old_stderr


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

    assert lint.failed == set([])
    result = lint.roll(files, num_procs=1)
    assert len(result) == 0
    assert lint.failed == set(['BadReturnCodeLinter'])


def fake_run_linters(config, paths, **lintargs):
    return {'count': [1]}, []


@pytest.mark.skipif(platform.system() == 'Windows',
                    reason="monkeypatch issues with multiprocessing on Windows")
@pytest.mark.parametrize('num_procs', [1, 4, 8, 16])
def test_number_of_jobs(monkeypatch, lint, linters, files, num_procs):
    monkeypatch.setattr(sys.modules[lint.__module__], '_run_linters', fake_run_linters)

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
    monkeypatch.setattr(sys.modules[lint.__module__], '_run_linters', fake_run_linters)

    files = files[:4]
    assert len(files) == 4

    linters = linters[:3]
    assert len(linters) == 3

    lint.MAX_PATHS_PER_JOB = max_paths
    lint.read(linters)
    num_jobs = len(lint.roll(files, num_procs=2)['count'])
    assert num_jobs == expected_jobs


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
    assert lint.failed == set(['SetupFailedLinter', 'SetupRaisedLinter'])


if __name__ == '__main__':
    mozunit.main()
