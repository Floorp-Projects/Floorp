from __future__ import absolute_import, print_function

import mozunit

LINTER = 'file-whitespace'


def test_lint_file_whitespace(lint, paths):
    results = lint(paths())
    print(results)
    assert len(results) == 3

    assert "Windows line return" in results[0].message
    assert results[0].level == "error"
    assert "bad-windows.c" in results[0].relpath

    assert "Trailing whitespace" in results[1].message
    assert results[1].level == "error"
    assert "bad.c" in results[1].relpath
    assert results[1].lineno == 1

    assert "Trailing whitespace" in results[2].message
    assert results[2].level == "error"
    assert "bad.c" in results[2].relpath
    assert results[2].lineno == 2


if __name__ == '__main__':
    mozunit.main()
