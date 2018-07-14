# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import mozunit

from mozversioncontrol import get_repository_object

STEPS = {
    'hg': [
        """
        echo "bar" >> bar
        echo "baz" > baz
        hg rm foo
        """,
        """
        hg commit -m "Remove foo; modify bar; touch baz (but don't add it)"
        """,
    ],
    'git': [
        """
        echo "bar" >> bar
        echo "baz" > baz
        git rm foo
        """,
        """
        git commit -am "Remove foo; modify bar; touch baz (but don't add it)"
        """
    ]
}


def test_working_directory_clean_untracked_files(repo):
    vcs = get_repository_object(repo.strpath)
    assert vcs.working_directory_clean()

    next(repo.step)
    assert not vcs.working_directory_clean()

    next(repo.step)
    assert vcs.working_directory_clean()
    assert not vcs.working_directory_clean(untracked=True)


if __name__ == '__main__':
    mozunit.main()
