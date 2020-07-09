from __future__ import absolute_import, print_function

import mozunit

LINTER = "codespell"


def test_lint_codespell(lint, paths):
    results = lint(paths())
    assert len(results) == 2

    assert results[0].message == "informations ==> information"
    assert results[0].level == "error"
    assert results[0].lineno == 1
    assert results[0].relpath == "ignore.rst"

    assert results[1].message == "mozila ==> mozilla"
    assert results[1].level == "error"
    assert results[1].lineno == 5
    assert results[1].relpath == "ignore.rst"


if __name__ == "__main__":
    mozunit.main()
