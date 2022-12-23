import mozunit

LINTER = "codespell"
fixed = 0


def test_lint_codespell_fix(lint, create_temp_file):
    contents = """This is a file with some typos and informations.
But also testing false positive like optin (because this isn't always option)
or stuff related to our coding style like:
aparent (aParent).
but detects mistakes like mozila
""".lstrip()

    path = create_temp_file(contents, "ignore.rst")
    lint([path], fix=True)

    assert fixed == 2


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
