# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit

from mozversioncontrol import get_repository_object

STEPS = {
    "hg": [
        """
        echo "bar" >> bar
        echo "baz" > foo
        """,
    ],
    "git": [
        """
        echo "bar" >> bar
        echo "baz" > foo
        """,
    ],
}


def test_commit(repo):
    vcs = get_repository_object(repo.dir)
    assert vcs.working_directory_clean()

    # Modify both foo and bar
    repo.execute_next_step()
    assert not vcs.working_directory_clean()

    # Commit just bar
    vcs.commit(
        message="Modify bar\n\nbut not baz",
        author="Testing McTesterson <test@example.org>",
        date="2017-07-14 02:40:00 UTC",
        paths=["bar"],
    )

    # We only committed bar, so foo is still keeping the working dir dirty
    assert not vcs.working_directory_clean()

    if repo.vcs == "git":
        log_cmd = ["log", "-1", "--format=%an,%ae,%aD,%B"]
        patch_cmd = ["log", "-1", "-p"]
    else:
        log_cmd = [
            "log",
            "-l",
            "1",
            "-T",
            "{person(author)},{email(author)},{date|rfc822date},{desc}",
        ]
        patch_cmd = ["log", "-l", "1", "-p"]

    # Verify commit metadata (we rstrip to normalize trivial git/hg differences)
    log = vcs._run(*log_cmd).rstrip()
    assert log == (
        "Testing McTesterson,test@example.org,Fri, 14 "
        "Jul 2017 02:40:00 +0000,Modify bar\n\nbut not baz"
    )

    # Verify only the intended file was added to the commit
    patch = vcs._run(*patch_cmd)
    diffs = [line for line in patch.splitlines() if "diff --git" in line]
    assert len(diffs) == 1
    assert diffs[0] == "diff --git a/bar b/bar"


if __name__ == "__main__":
    mozunit.main()
