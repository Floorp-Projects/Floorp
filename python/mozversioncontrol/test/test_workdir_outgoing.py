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

        """
        echo ooka >> baz
        echo newborn > baby
        hg add baby
        """,

        """
        hg commit -m "Modify baz; add baby"
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
        """,

        """
        echo ooka >> baz
        echo newborn > baby
        git add baz baby
        """,

        """
        git commit -m "Modify baz; add baby"
        """,
    ]
}


def assert_files(actual, expected):
    assert set(map(os.path.basename, actual)) == set(expected)


def test_workdir_outgoing(repo):
    vcs = get_repository_object(repo.strpath)
    assert vcs.path == repo.strpath

    remotepath = '../remoterepo' if repo.vcs == 'hg' else 'upstream/master'

    # Mutate files.
    next(repo.step)

    assert_files(vcs.get_changed_files('A', 'all'), ['baz'])
    assert_files(vcs.get_changed_files('AM', 'all'), ['bar', 'baz'])
    assert_files(vcs.get_changed_files('D', 'all'), ['foo'])
    if repo.vcs == 'git':
        assert_files(vcs.get_changed_files('AM', mode='staged'), ['baz'])
    elif repo.vcs == 'hg':
        # Mercurial does not use a staging area (and ignores the mode parameter.)
        assert_files(vcs.get_changed_files('AM', 'unstaged'), ['bar', 'baz'])
    assert_files(vcs.get_outgoing_files('AMD'), [])
    assert_files(vcs.get_outgoing_files('AMD', remotepath), [])

    # Create a commit.
    next(repo.step)

    assert_files(vcs.get_changed_files('AMD', 'all'), [])
    assert_files(vcs.get_changed_files('AMD', 'staged'), [])
    assert_files(vcs.get_outgoing_files('AMD'), ['bar', 'baz', 'foo'])
    assert_files(vcs.get_outgoing_files('AMD', remotepath), ['bar', 'baz', 'foo'])

    # Mutate again.
    next(repo.step)

    assert_files(vcs.get_changed_files('A', 'all'), ['baby'])
    assert_files(vcs.get_changed_files('AM', 'all'), ['baby', 'baz'])
    assert_files(vcs.get_changed_files('D', 'all'), [])

    # Create a second commit.
    next(repo.step)

    assert_files(vcs.get_outgoing_files('AM'), ['bar', 'baz', 'baby'])
    assert_files(vcs.get_outgoing_files('AM', remotepath), ['bar', 'baz', 'baby'])
    if repo.vcs == 'git':
        assert_files(vcs.get_changed_files('AM', rev='HEAD~1'), ['bar', 'baz'])
        assert_files(vcs.get_changed_files('AM', rev='HEAD'), ['baby', 'baz'])
    else:
        assert_files(vcs.get_changed_files('AM', rev='.^'), ['bar', 'baz'])
        assert_files(vcs.get_changed_files('AM', rev='.'), ['baby', 'baz'])
        assert_files(vcs.get_changed_files('AM', rev='.^::'), ['bar', 'baz', 'baby'])
        assert_files(vcs.get_changed_files('AM', rev='modifies(baz)'), ['baz', 'baby'])


if __name__ == '__main__':
    mozunit.main()
