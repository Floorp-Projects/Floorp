# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import shutil
import subprocess
import sys
from pathlib import Path

import toml
from packaging.requirements import Requirement


class PoetryLockfiles:
    def __init__(
        self,
        poetry_lockfile: Path,
        pip_lockfile: Path,
    ):
        self.poetry_lockfile = poetry_lockfile
        self.pip_lockfile = pip_lockfile


class PoetryHandle:
    def __init__(self, work_dir: Path):
        self._work_dir = work_dir
        self._dependencies = {}

    def add_requirement(self, requirement: Requirement):
        self._dependencies[requirement.name] = str(requirement.specifier)

    def add_requirements_in_file(self, requirements_in: Path):
        with open(requirements_in) as requirements_in:
            for line in requirements_in.readlines():
                if line.startswith("#"):
                    continue

                req = Requirement(line)
                self.add_requirement(req)

    def reuse_existing_lockfile(self, lockfile_path: Path):
        """Make minimal number of changes to the lockfile to satisfy new requirements"""
        shutil.copy(str(lockfile_path), str(self._work_dir / "poetry.lock"))

    def generate_lockfiles(self, do_update):
        """Generate pip-style lockfiles that satisfy provided requirements

        One lockfile will be made for all mandatory requirements, and then an extra,
        compatible lockfile will be created for each optional requirement.

        Args:
            do_update: if True, then implicitly upgrade the versions of transitive
                dependencies
        """

        poetry_config = {
            "name": "poetry-test",
            "description": "",
            "version": "0",
            "authors": [],
            "dependencies": {"python": "^3.8"},
        }
        poetry_config["dependencies"].update(self._dependencies)

        pyproject = {"tool": {"poetry": poetry_config}}
        with open(self._work_dir / "pyproject.toml", "w") as pyproject_file:
            toml.dump(pyproject, pyproject_file)

        self._run_poetry(["lock"] + (["--no-update"] if not do_update else []))
        self._run_poetry(["export", "-o", "requirements.txt"])

        return PoetryLockfiles(
            self._work_dir / "poetry.lock",
            self._work_dir / "requirements.txt",
        )

    def _run_poetry(self, args):
        subprocess.check_call(
            [sys.executable, "-m", "poetry"] + args, cwd=self._work_dir
        )
