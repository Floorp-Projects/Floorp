import mozunit

LINTER = "updatebot"


def test_basic(lint, paths):
    results = lint(paths())

    assert len(results) == 2

    i = 0
    assert results[i].level == "error"
    assert "cargo-mismatch.yaml" in results[i].relpath
    assert "was not found in Cargo.lock" in results[i].message

    i += 1
    assert results[i].level == "error"
    assert "no-revision.yaml" in results[i].relpath
    assert (
        'If "vendoring" is present, "revision" must be present in "origin"'
        in results[i].message
    )


if __name__ == "__main__":
    mozunit.main()
