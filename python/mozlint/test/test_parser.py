# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

import pytest

from mozlint.parser import Parser
from mozlint.errors import (
    LinterNotFound,
    LinterParseError,
)


@pytest.fixture(scope='module')
def parse(lintdir):
    parser = Parser()

    def _parse(name):
        path = os.path.join(lintdir, name)
        return parser(path)
    return _parse


def test_parse_valid_linter(parse):
    lintobj = parse('string.yml')
    assert isinstance(lintobj, list)
    assert len(lintobj) == 1

    lintobj = lintobj[0]
    assert isinstance(lintobj, dict)
    assert 'name' in lintobj
    assert 'description' in lintobj
    assert 'type' in lintobj
    assert 'payload' in lintobj
    assert 'extensions' in lintobj
    assert set(lintobj['extensions']) == set(['js', 'jsm'])


@pytest.mark.parametrize('linter', [
    'invalid_type.yml',
    'invalid_extension.ym',
    'invalid_include.yml',
    'invalid_exclude.yml',
    'missing_attrs.yml',
    'missing_definition.yml',
])
def test_parse_invalid_linter(parse, linter):
    with pytest.raises(LinterParseError):
        parse(linter)


def test_parse_non_existent_linter(parse):
    with pytest.raises(LinterNotFound):
        parse('missing_file.lint')


if __name__ == '__main__':
    sys.exit(pytest.main(['--verbose', __file__]))
