# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

import pytest

from mozlint import cli

here = os.path.abspath(os.path.dirname(__file__))


@pytest.fixture
def parser():
    return cli.MozlintParser()


@pytest.fixture
def run(parser, lintdir, files):
    if lintdir not in cli.SEARCH_PATHS:
        cli.SEARCH_PATHS.append(lintdir)

    def inner(args=None):
        args = args or []
        args.extend(files)
        lintargs = vars(parser.parse_args(args))
        lintargs['root'] = here
        return cli.run(**lintargs)
    return inner


def test_cli_run_with_fix(run, capfd):
    ret = run(['-f', 'json', '--fix', '--linter', 'external'])
    out, err = capfd.readouterr()
    assert ret == 0
    assert out.endswith('{}\n')


def test_cli_run_with_edit(run, parser, capfd):
    os.environ['EDITOR'] = 'echo'

    ret = run(['-f', 'json', '--edit', '--linter', 'external'])
    out = capfd.readouterr()[0].strip()
    assert ret == 0
    assert os.path.basename(out) == 'foobar.js'

    del os.environ['EDITOR']
    with pytest.raises(SystemExit):
        parser.parse_args(['--edit'])


if __name__ == '__main__':
    sys.exit(pytest.main(['--verbose', __file__]))
