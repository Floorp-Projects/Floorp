# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from subprocess import CalledProcessError

import mozunit
import pytest

from mozversioncontrol import get_repository_object


STEPS = {
    "hg": [
        """
        echo "bar" >> bar
        echo "baz" > foo
        hg commit -m "second commit"
        """,
        """
        echo "foobar" > foo
        """,
    ],
    "git": [
        """
        echo "bar" >> bar
        echo "baz" > foo
        git add *
        git commit -m "second commit"
        """,
        """
        echo "foobar" > foo
        """,
    ],
}


def test_commit(repo):
    vcs = get_repository_object(repo.strpath)
    rev0 = vcs.head_ref

    next(repo.step)
    rev1 = vcs.head_ref
    assert rev0 != rev1

    if repo.vcs == "hg":
        vcs.update(".~1")
    else:
        vcs.update("HEAD~1")
    assert vcs.head_ref == rev0

    vcs.update(rev1)
    assert vcs.head_ref == rev1

    # Update should fail with dirty working directory.
    next(repo.step)
    with pytest.raises(CalledProcessError):
        vcs.update(rev0)

    assert vcs.head_ref == rev1


if __name__ == "__main__":
    mozunit.main()
