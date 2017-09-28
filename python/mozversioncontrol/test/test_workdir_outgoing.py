# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import subprocess

import mozunit
import pytest

from mozversioncontrol import get_repository_object


setup = {
    'hg': [
        """
        echo "foo" > foo
        echo "bar" > bar
        hg init
        hg add *
        hg commit -m "Initial commit"
        """,
        """
        echo "[paths]\ndefault = ../remoterepo" > .hg/hgrc
        """,
        """
        echo "bar" >> bar
        echo "baz" > baz
        hg add baz
        hg rm foo
        """,
        """
        hg commit -m "Remove foo; modify bar; add baz"
        """,
    ],
    'git': [
        """
        echo "foo" > foo
        echo "bar" > bar
        git init
        git add *
        git commit -am "Initial commit"
        """,
        """
        git remote add upstream ../remoterepo
        git fetch upstream
        git branch -u upstream/master
        """,
        """
        echo "bar" >> bar
        echo "baz" > baz
        git add baz
        git rm foo
        """,
        """
        git commit -am "Remove foo; modify bar; add baz"
        """
    ]
}


def shell(cmd):
    subprocess.check_call(cmd, shell=True)


@pytest.yield_fixture(params=['git', 'hg'])
def repo(tmpdir, request):
    vcs = request.param

    # tmpdir and repo are py.path objects
    # http://py.readthedocs.io/en/latest/path.html
    repo = tmpdir.mkdir('repo')
    repo.vcs = vcs

    # This creates a setup iterator. Each time next() is called
    # on it, the next set of instructions will be executed.
    repo.setup = (shell(cmd) for cmd in setup[vcs])

    oldcwd = os.getcwd()
    os.chdir(repo.strpath)

    next(repo.setup)

    repo.copy(tmpdir.join('remoterepo'))

    next(repo.setup)

    yield repo
    os.chdir(oldcwd)


def assert_files(actual, expected):
    assert set(map(os.path.basename, actual)) == set(expected)


def test_workdir_outgoing(repo):
    vcs = get_repository_object(repo.strpath)
    assert vcs.path == repo.strpath

    remotepath = '../remoterepo' if repo.vcs == 'hg' else 'upstream/master'

    next(repo.setup)

    assert_files(vcs.get_changed_files('AM', 'all'), ['bar', 'baz'])
    if repo.vcs == 'git':
        assert_files(vcs.get_changed_files('AM', mode='staged'), ['baz'])
    elif repo.vcs == 'hg':
        assert_files(vcs.get_changed_files('AM', 'staged'), ['bar', 'baz'])
    assert_files(vcs.get_outgoing_files('AM'), [])
    assert_files(vcs.get_outgoing_files('AM', remotepath), [])

    next(repo.setup)

    assert_files(vcs.get_changed_files('AM', 'all'), [])
    assert_files(vcs.get_changed_files('AM', 'staged'), [])
    assert_files(vcs.get_outgoing_files('AM'), ['bar', 'baz'])
    assert_files(vcs.get_outgoing_files('AM', remotepath), ['bar', 'baz'])


if __name__ == '__main__':
    mozunit.main()
