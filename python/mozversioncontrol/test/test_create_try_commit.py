# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit

from mozversioncontrol import get_repository_object

STEPS = {
    "hg": [
        """
        echo "{}" > try_task_config.json
        hg add try_task_config.json
        """,
    ],
    "git": [
        """
        echo "{}" > try_task_config.json
        git add try_task_config.json
        """,
    ],
}


def test_create_try_commit(repo):
    commit_message = "try commit message"
    vcs = get_repository_object(repo.dir)

    # Create a non-empty commit.
    repo.execute_next_step()
    vcs.create_try_commit(commit_message)
    non_empty_commit_sha = vcs.head_ref

    assert vcs.get_changed_files(rev=non_empty_commit_sha) == ["try_task_config.json"]

    # Create an empty commit.
    vcs.create_try_commit(commit_message)
    empty_commit_sha = vcs.head_ref

    # Commit should be created with no changed files.
    assert vcs.get_changed_files(rev=empty_commit_sha) == []


if __name__ == "__main__":
    mozunit.main()
