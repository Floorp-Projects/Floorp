# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import subprocess

import mozunit
import pytest

from mozversioncontrol import MissingVCSExtension, get_repository_object


def test_push_to_try(repo, monkeypatch):
    commit_message = "commit message"
    vcs = get_repository_object(repo.dir)

    captured_commands = []

    def fake_run(*args, **kwargs):
        captured_commands.append(args[0])

    monkeypatch.setattr(subprocess, "check_output", fake_run)
    monkeypatch.setattr(subprocess, "check_call", fake_run)

    vcs.push_to_try(commit_message)
    tool = vcs._tool

    if repo.vcs == "hg":
        expected = [
            (str(tool), "push-to-try", "-m", commit_message),
            (str(tool), "revert", "-a"),
        ]
    else:
        expected = [
            (str(tool), "cinnabar", "--version"),
            (
                str(tool),
                "-c",
                "commit.gpgSign=false",
                "commit",
                "--allow-empty",
                "-m",
                commit_message,
            ),
            (
                str(tool),
                "push",
                "hg::ssh://hg.mozilla.org/try",
                "+HEAD:refs/heads/branches/default/tip",
            ),
            (str(tool), "reset", "HEAD~"),
        ]

    for i, value in enumerate(captured_commands):
        assert value == expected[i]

    assert len(captured_commands) == len(expected)


def test_push_to_try_missing_extensions(repo, monkeypatch):
    if repo.vcs != "git":
        return

    vcs = get_repository_object(repo.dir)

    orig = vcs._run

    def cinnabar_raises(*args, **kwargs):
        # Simulate not having git cinnabar
        if args[0] == "cinnabar":
            raise subprocess.CalledProcessError(1, args)
        return orig(*args, **kwargs)

    monkeypatch.setattr(vcs, "_run", cinnabar_raises)

    with pytest.raises(MissingVCSExtension):
        vcs.push_to_try("commit message")


if __name__ == "__main__":
    mozunit.main()
