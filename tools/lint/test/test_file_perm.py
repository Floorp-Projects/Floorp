from __future__ import absolute_import, print_function

import mozunit

LINTER = 'file-perm'


def test_lint_file_perm(lint, paths):
    results = lint(paths())
    print(results)
    assert len(results) == 2

    assert "permissions on a source" in results[0].message
    assert results[0].level == "error"
    assert results[0].relpath == "bad.c"

    assert "permissions on a source" in results[1].message
    assert results[1].level == "error"
    assert results[1].relpath == "bad.js"


if __name__ == '__main__':
    mozunit.main()
