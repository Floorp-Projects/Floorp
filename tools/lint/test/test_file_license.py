import mozunit

LINTER = "license"


def test_lint_license(lint, paths):
    results = lint(paths())
    print(results)
    assert len(results) == 3

    assert ".eslintrc.js" in results[0].relpath

    assert "No matching license strings" in results[1].message
    assert results[1].level == "error"
    assert "bad.c" in results[1].relpath

    assert "No matching license strings" in results[2].message
    assert results[2].level == "error"
    assert "bad.js" in results[2].relpath


if __name__ == "__main__":
    mozunit.main()
