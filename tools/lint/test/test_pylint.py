import mozunit

LINTER = "pylint"


def test_lint_single_file(lint, paths):
    results = lint(paths("bad.py"))
    assert len(results) == 3
    assert results[0].rule == "E0602"
    assert results[1].rule == "W0101"
    assert results[1].lineno == 5

    # run lint again to make sure the previous results aren't counted twice
    results = lint(paths("bad.py"))
    assert len(results) == 3


def test_lint_single_file_good(lint, paths):
    results = lint(paths("good.py"))
    assert len(results) == 0


if __name__ == "__main__":
    mozunit.main()
