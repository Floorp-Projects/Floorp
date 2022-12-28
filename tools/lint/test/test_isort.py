# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

import mozunit

LINTER = "isort"
fixed = 0


def test_lint_fix(lint, create_temp_file):
    contents = """
import prova
import collections


def foobar():
    c = collections.Counter()
    prova.ciao(c)
""".lstrip()

    path = create_temp_file(contents, name="bad.py")
    results = lint([path])
    assert len(results) == 1
    assert results[0].level == "error"

    lint([path], fix=True)
    assert fixed == 1


def test_lint_excluded_file(lint, paths, config):
    # Second file is excluded from .flake8 config.
    files = paths("bad.py", "subdir/exclude/bad.py", "subdir/exclude/exclude_subdir")
    results = lint(files, config)
    assert len(results) == 1

    # First file is globally excluded, second one is from .flake8 config.
    files = paths("bad.py", "subdir/exclude/bad.py", "subdir/exclude/exclude_subdir")
    config["exclude"] = paths("bad.py")
    results = lint(files, config)
    assert len(results) == 0

    # Make sure excludes also apply when running from a different cwd.
    cwd = paths("subdir")[0]
    os.chdir(cwd)

    results = lint(paths("subdir/exclude"))
    assert len(results) == 0


def test_lint_uses_all_configs(lint, paths, tmpdir):
    myself = tmpdir.join("myself")
    myself.mkdir()

    flake8_path = tmpdir.join(".flake8")
    flake8_path.write(
        """
[flake8]
exclude =
""".lstrip()
    )

    py_path = myself.join("good.py")
    py_path.write(
        """
import os

from myself import something_else
from third_party import something


def ciao():
    pass
""".lstrip()
    )

    results = lint([py_path.strpath])
    assert len(results) == 0

    isort_cfg_path = myself.join(".isort.cfg")
    isort_cfg_path.write(
        """
[settings]
known_first_party = myself
""".lstrip()
    )

    results = lint([py_path.strpath], root=tmpdir.strpath)
    assert len(results) == 1

    py_path.write(
        """
import os

from third_party import something

from myself import something_else


def ciao():
    pass
""".lstrip()
    )

    results = lint([py_path.strpath], root=tmpdir.strpath)
    assert len(results) == 0


if __name__ == "__main__":
    mozunit.main()
