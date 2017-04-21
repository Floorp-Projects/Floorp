# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

import pytest

from mozlint import ResultContainer
from mozlint.errors import LintersNotConfigured, LintException


here = os.path.abspath(os.path.dirname(__file__))


linters = ('string.lint.py', 'regex.lint.py', 'external.lint.py')


def test_roll_no_linters_configured(lint, files):
    with pytest.raises(LintersNotConfigured):
        lint.roll(files)


def test_roll_successful(lint, linters, files):
    lint.read(linters)

    result = lint.roll(files)
    assert len(result) == 1
    assert lint.failed == []

    path = result.keys()[0]
    assert os.path.basename(path) == 'foobar.js'

    errors = result[path]
    assert isinstance(errors, list)
    assert len(errors) == 6

    container = errors[0]
    assert isinstance(container, ResultContainer)
    assert container.rule == 'no-foobar'


def test_roll_catch_exception(lint, lintdir, files):
    lint.read(os.path.join(lintdir, 'raises.lint.py'))

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
    assert lint.failed == []


def test_roll_with_invalid_extension(lint, lintdir, filedir):
    lint.read(os.path.join(lintdir, 'external.lint.py'))
    result = lint.roll(os.path.join(filedir, 'foobar.py'))
    assert len(result) == 0
    assert lint.failed == []


def test_roll_with_failure_code(lint, lintdir, files):
    lint.read(os.path.join(lintdir, 'badreturncode.lint.py'))

    assert lint.failed is None
    result = lint.roll(files)
    assert len(result) == 0
    assert lint.failed == ['BadReturnCodeLinter']


if __name__ == '__main__':
    sys.exit(pytest.main(['--verbose', __file__]))
