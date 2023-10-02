# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit

from mozversioncontrol import get_repository_object

STEPS = {
    "hg": [
        """
        echo bar >> bar
        hg commit -m "FIRST PATCH"
        """,
        """
        echo baz > baz
        hg add baz
        hg commit -m "SECOND PATCH"
        """,
    ],
    "git": [
        """
        echo bar >> bar
        git add bar
        git commit -m "FIRST PATCH"
        """,
        """
        echo baz > baz
        git add baz
        git commit -m "SECOND PATCH"
        """,
    ],
}


def test_get_commit_patches(repo):
    vcs = get_repository_object(repo.dir)
    nodes = []

    # Create some commits and note the SHAs.
    repo.execute_next_step()
    nodes.append(vcs.head_ref)

    repo.execute_next_step()
    nodes.append(vcs.head_ref)

    patches = vcs.get_commit_patches(nodes)

    assert len(patches) == 2
    # Assert the patches are returned in the correct order.
    assert b"FIRST PATCH" in patches[0]
    assert b"SECOND PATCH" in patches[1]


if __name__ == "__main__":
    mozunit.main()
