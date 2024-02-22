# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit

LINTER = "ignorefile"


def test_same(lint, create_temp_file):
    hgContents = """
\\.pyc$
\\.pyo$
    """
    gitContents = """
*.pyc
*.pyo
    """

    path = create_temp_file(hgContents, ".hgignore")
    create_temp_file(gitContents, ".gitignore")

    results = lint([path])

    assert len(results) == 0


def test_replace(lint, create_temp_file):
    hgContents = """
\\.pyc$
\\.pyo$
    """
    gitContents = """
*.pyc
*.pyp
    """

    path = create_temp_file(hgContents, ".hgignore")
    path2 = create_temp_file(gitContents, ".gitignore")

    results = lint([path])

    assert len(results) == 1
    assert results[0].level == "error"
    assert results[0].lineno == 3
    assert (
        results[0].message
        == 'Pattern mismatch: "\\.pyo$" in .hgignore vs "*.pyp" in .gitignore'
    )

    results = lint([path2])

    assert len(results) == 1
    assert results[0].level == "error"
    assert results[0].lineno == 3
    assert (
        results[0].message
        == 'Pattern mismatch: "*.pyp" in .gitignore vs "\\.pyo$" in .hgignore'
    )


def test_insert(lint, create_temp_file):
    hgContents = """
\\.pyc$
\\.pyo$
    """
    gitContents = """
*.pyc
foo
*.pyo
    """

    path = create_temp_file(hgContents, ".hgignore")
    create_temp_file(gitContents, ".gitignore")

    results = lint([path])

    assert len(results) == 1
    assert results[0].level == "error"
    assert results[0].lineno == 3
    assert results[0].message == 'Pattern "foo" not found in .hgignore'


def test_delete(lint, create_temp_file):
    hgContents = """
\\.pyc$
foo
\\.pyo$
    """
    gitContents = """
*.pyc
*.pyo
    """

    path = create_temp_file(hgContents, ".hgignore")
    create_temp_file(gitContents, ".gitignore")

    results = lint([path])

    assert len(results) == 1
    assert results[0].level == "error"
    assert results[0].lineno == 3
    assert results[0].message == 'Pattern "foo" not found in .gitignore'


def test_ignore(lint, create_temp_file):
    hgContents = """
\\.pyc$
# lint-ignore-next-line: hg-only
foo
\\.pyo$
# lint-ignore-next-line: syntax-difference
(file1|file2)
diff1
    """
    gitContents = """
*.pyc
*.pyo
# lint-ignore-next-line: git-only
bar
# lint-ignore-next-line: syntax-difference
file1
# lint-ignore-next-line: syntax-difference
file2
diff2
    """

    path = create_temp_file(hgContents, ".hgignore")
    create_temp_file(gitContents, ".gitignore")

    results = lint([path])

    # Only the line without lint-ignore-next-line should be reported
    assert len(results) == 1
    assert results[0].level == "error"
    assert results[0].lineno == 8
    assert (
        results[0].message
        == 'Pattern mismatch: "diff1" in .hgignore vs "diff2" in .gitignore'
    )


def test_invalid_syntax(lint, create_temp_file):
    hgContents = """
\\.pyc$
# lint-ignore-next-line: random
foo
\\.pyo$
    """
    gitContents = """
*.pyc
*.pyo
    """

    path = create_temp_file(hgContents, ".hgignore")
    create_temp_file(gitContents, ".gitignore")

    results = lint([path])

    assert len(results) == 2
    assert results[0].level == "error"
    assert results[0].lineno == 3
    assert results[0].message == 'Unknown lint rule: "random"'
    assert results[1].level == "error"
    assert results[1].lineno == 4
    assert results[1].message == 'Pattern "foo" not found in .gitignore'


if __name__ == "__main__":
    mozunit.main()
