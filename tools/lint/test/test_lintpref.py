import mozunit

LINTER = "lintpref"


def test_lintpref(lint, paths):
    results = lint(paths())
    assert len(results) == 1
    assert results[0].level == "error"
    assert 'pref("dom.webidl.test1", true);' in results[0].message
    assert "bad.js" in results[0].relpath
    assert results[0].lineno == 2


if __name__ == "__main__":
    mozunit.main()
