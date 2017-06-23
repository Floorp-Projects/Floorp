# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
import sys

import pytest

from mozlint.vcs import VCSHelper, vcs_class


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


def test_vcs_helper(repo):
    vcs = VCSHelper.create()
    assert vcs.__class__ == vcs_class[repo.vcs]
    assert vcs.root == repo.strpath

    remotepath = '../remoterepo' if repo.vcs == 'hg' else 'upstream/master'

    next(repo.setup)

    assert_files(vcs.by_workdir(), ['bar', 'baz'])
    assert_files(vcs.by_outgoing(), [])
    assert_files(vcs.by_outgoing(remotepath), [])

    next(repo.setup)

    assert_files(vcs.by_workdir(), [])
    assert_files(vcs.by_outgoing(), ['bar', 'baz'])
    assert_files(vcs.by_outgoing(remotepath), ['bar', 'baz'])


if __name__ == '__main__':
    sys.exit(pytest.main(['--verbose', __file__]))
