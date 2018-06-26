# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os

import mozunit

from mozversioncontrol import get_repository_object


STEPS = {
    'hg': [
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


def assert_files(actual, expected):
    assert set(map(os.path.basename, actual)) == set(expected)


def test_workdir_outgoing(repo):
    vcs = get_repository_object(repo.strpath)
    assert vcs.path == repo.strpath

    remotepath = '../remoterepo' if repo.vcs == 'hg' else 'upstream/master'

    next(repo.step)

    assert_files(vcs.get_changed_files('AM', 'all'), ['bar', 'baz'])
    if repo.vcs == 'git':
        assert_files(vcs.get_changed_files('AM', mode='staged'), ['baz'])
    elif repo.vcs == 'hg':
        assert_files(vcs.get_changed_files('AM', 'staged'), ['bar', 'baz'])
    assert_files(vcs.get_outgoing_files('AM'), [])
    assert_files(vcs.get_outgoing_files('AM', remotepath), [])

    next(repo.step)

    assert_files(vcs.get_changed_files('AM', 'all'), [])
    assert_files(vcs.get_changed_files('AM', 'staged'), [])
    assert_files(vcs.get_outgoing_files('AM'), ['bar', 'baz'])
    assert_files(vcs.get_outgoing_files('AM', remotepath), ['bar', 'baz'])


if __name__ == '__main__':
    mozunit.main()
