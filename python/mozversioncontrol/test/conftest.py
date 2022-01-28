# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
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
        git init
        git config user.name "Testing McTesterson"
        git config user.email "<test@example.org>"
        git add *
        git commit -am "Initial commit"
        """,
        """
        git remote add upstream ../remoterepo
        git fetch upstream
        git branch -u upstream/master
        """,
    ],
}


def shell(cmd, working_dir):
    for step in cmd.split(os.linesep):
        subprocess.check_call(step, shell=True, cwd=working_dir)


@pytest.yield_fixture(params=["git", "hg"])
def repo(tmpdir, request):
    vcs = request.param
    steps = SETUP[vcs]

    if hasattr(request.module, "STEPS"):
        steps.extend(request.module.STEPS[vcs])

    # tmpdir and repo are py.path objects
    # http://py.readthedocs.io/en/latest/path.html
    repo = tmpdir.mkdir("repo")
    repo.vcs = vcs

    working_dir = str(Path(repo.strpath).resolve())

    # This creates a step iterator. Each time next() is called
    # on it, the next set of instructions will be executed.
    repo.step = (shell(cmd, working_dir) for cmd in steps)

    next(repo.step)

    repo.copy(tmpdir.join("remoterepo"))

    next(repo.step)

    yield repo
