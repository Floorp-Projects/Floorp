import mozunit

LINTER = "updatebot"


def test_basic(lint, paths):
    results = lint(paths())

    assert len(results) == 1

    i = 0
    assert results[i].level == "error"
    assert "no-revision.yaml" in results[i].relpath
    assert (
        'If "vendoring" is present, "revision" must be present in "origin"'
        in results[i].message
    )


if __name__ == "__main__":
    mozunit.main()
