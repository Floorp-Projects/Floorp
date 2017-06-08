# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

import pytest

from mozlint.result import ResultContainer


@pytest.fixture
def path(filedir):
    def _path(name):
        return os.path.join(filedir, name)
    return _path


@pytest.fixture(params=[
    'string.yml',
    'regex.yml',
    'external.yml',
    'structured.yml'])
def linter(lintdir, request):
    return os.path.join(lintdir, request.param)


def test_linter_types(lint, linter, files, path):
    lint.read(linter)
    result = lint.roll(files)
    assert isinstance(result, dict)
    assert path('foobar.js') in result
    assert path('no_foobar.js') not in result

    result = result[path('foobar.js')][0]
    assert isinstance(result, ResultContainer)

    name = os.path.basename(linter).split('.')[0]
    assert result.linter.lower().startswith(name)


def test_no_filter(lint, lintdir, files):
    lint.read(os.path.join(lintdir, 'explicit_path.yml'))
    result = lint.roll(files)
    assert len(result) == 0

    lint.lintargs['use_filters'] = False
    result = lint.roll(files)
    assert len(result) == 2


if __name__ == '__main__':
    sys.exit(pytest.main(['--verbose', __file__]))
