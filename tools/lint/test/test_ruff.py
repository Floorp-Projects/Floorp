# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from pprint import pprint
from textwrap import dedent

import mozunit

LINTER = "ruff"
fixed = 0


def test_lint_fix(lint, create_temp_file):
    contents = dedent(
        """
    import distutils
    print("hello!")
    """
    )

    path = create_temp_file(contents, "bad.py")
    lint([path], fix=True)
    assert fixed == 1


def test_lint_ruff(lint, paths):
    results = lint(paths())
    pprint(results, indent=2)
    assert len(results) == 2
    assert results[0].level == "error"
    assert results[0].relpath == "bad.py"
    assert "`distutils` imported but unused" in results[0].message


if __name__ == "__main__":
    mozunit.main()
