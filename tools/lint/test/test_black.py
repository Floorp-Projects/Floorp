# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit

LINTER = "black"
fixed = 0


def test_lint_fix(lint, create_temp_file):
    contents = """def is_unique(
               s
               ):
    s = list(s
                )
    s.sort()


    for i in range(len(s) - 1):
        if s[i] == s[i + 1]:
            return 0
    else:
        return 1


if __name__ == "__main__":
    print(
          is_unique(input())
         ) """

    path = create_temp_file(contents, "bad.py")
    lint([path], fix=True)
    assert fixed == 1


def test_lint_black(lint, paths):
    results = lint(paths())
    assert len(results) == 2

    assert results[0].level == "error"
    assert results[0].relpath == "bad.py"

    assert "EOF" in results[1].message
    assert results[1].level == "error"
    assert results[1].relpath == "invalid.py"


if __name__ == "__main__":
    mozunit.main()
