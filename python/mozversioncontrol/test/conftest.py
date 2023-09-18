# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import subprocess
from pathlib import Path

import pytest

SETUP = {
    "hg": [
        """
        echo "foo" > foo
        echo "bar" > bar
        hg init
        hg add *
        hg commit -m "Initial commit"
        hg phase --public .
        """,
        """
        echo [paths] > .hg/hgrc
        echo "default = ../remoterepo" >> .hg/hgrc
        """,
    ],
    "git": [
        """
        echo "foo" > foo
        echo "bar" > bar
        git init --initial-branch main
        git config user.name "Testing McTesterson"
        git config user.email "<test@example.org>"
        git add *
        git commit -am "Initial commit"
        """,
        """
        git remote add upstream ../remoterepo
        git fetch upstream
        git branch -u upstream/main
        """,
    ],
}


class RepoTestFixture:
    def __init__(self, repo_dir: Path, vcs: str, steps: [str]):
        self.dir = repo_dir
        self.vcs = vcs

        # This creates a step iterator. Each time execute_next_step()
        # is called the next set of instructions will be executed.
        self.steps = (shell(cmd, self.dir) for cmd in steps)

    def execute_next_step(self):
        next(self.steps)


def shell(cmd, working_dir):
    for step in cmd.split(os.linesep):
        subprocess.check_call(step, shell=True, cwd=working_dir)


@pytest.fixture(params=["git", "hg"])
def repo(tmpdir, request):
    tmpdir = Path(tmpdir)
    vcs = request.param
    steps = SETUP[vcs]

    if hasattr(request.module, "STEPS"):
        steps.extend(request.module.STEPS[vcs])

    repo_dir = (tmpdir / "repo").resolve()
    (tmpdir / "repo").mkdir()

    repo_test_fixture = RepoTestFixture(repo_dir, vcs, steps)

    repo_test_fixture.execute_next_step()

    shutil.copytree(str(repo_dir), str(tmpdir / "remoterepo"))

    repo_test_fixture.execute_next_step()

    yield repo_test_fixture
