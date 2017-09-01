# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from distutils.spawn import find_executable

import mozunit
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


@pytest.mark.skipif(not find_executable("echo"), reason="No `echo` executable found.")
def test_cli_run_with_edit(run, parser, capfd):
    os.environ['EDITOR'] = 'echo'

    ret = run(['-f', 'compact', '--edit', '--linter', 'external'])
    out, err = capfd.readouterr()
    out = out.splitlines()
    assert ret == 1
    assert len(out) == 5
    assert out[0].endswith('foobar.js')  # from the `echo` editor
    assert "foobar.js: line 1, col 1, Error" in out[1]
    assert "foobar.js: line 2, col 1, Error" in out[2]

    del os.environ['EDITOR']
    with pytest.raises(SystemExit):
        parser.parse_args(['--edit'])


if __name__ == '__main__':
    mozunit.main()
