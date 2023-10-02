# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit
import pytest

from mozversioncontrol import get_repository_object

STEPS = {
    "hg": [
        """
        echo "{}" > try_task_config.json
        hg add try_task_config.json
        hg commit -m "Try config commit"
        """
    ],
    "git": [
        """
        echo "{}" > try_task_config.json
        git add try_task_config.json
        git commit -m "Try config commit"
        """
    ],
}


@pytest.mark.xfail(reason="Requires the Mercurial evolve extension.", strict=False)
def test_remove_current_commit(repo):
    vcs = get_repository_object(repo.dir)
    initial_head_ref = vcs.head_ref

    # Create a new commit.
    repo.execute_next_step()

    vcs.remove_current_commit()

    assert (
        vcs.head_ref == initial_head_ref
    ), "Removing current commit should revert to previous head."


if __name__ == "__main__":
    mozunit.main()
