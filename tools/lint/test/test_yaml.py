import mozunit

LINTER = "yaml"


def test_basic(lint, paths):
    results = lint(paths())

    assert len(results) == 3

    assert "line too long (122 > 80 characters)" in results[0].message
    assert results[0].level == "error"
    assert "bad.yml" in results[0].relpath
    assert results[0].lineno == 3

    assert "wrong indentation: expected 4 but found 8" in results[1].message
    assert results[1].level == "error"
    assert "bad.yml" in results[1].relpath
    assert results[0].lineno == 3

    assert "could not find expected" in results[2].message
    assert results[2].level == "error"
    assert "bad.yml" in results[2].relpath
    assert results[2].lineno == 9


if __name__ == "__main__":
    mozunit.main()
