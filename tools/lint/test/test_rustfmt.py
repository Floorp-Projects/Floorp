import mozunit


LINTER = "rustfmt"


def test_good(lint, config, paths):
    results = lint(paths("subdir/good.rs"))
    print(results)
    assert len(results) == 0


def test_basic(lint, config, paths):
    results = lint(paths("subdir/bad.rs"))
    print(results)
    assert len(results) >= 1

    assert "Reformat rust" in results[0].message
    assert results[0].level == "warning"
    assert results[0].lineno == 4
    assert "bad.rs" in results[0].path
    assert "Print text to the console" in results[0].diff


def test_dir(lint, config, paths):
    results = lint(paths("subdir/"))
    print(results)
    assert len(results) >= 4

    assert "Reformat rust" in results[0].message
    assert results[0].level == "warning"
    assert results[0].lineno == 4
    assert "bad.rs" in results[0].path
    assert "Print text to the console" in results[0].diff

    assert "Reformat rust" in results[1].message
    assert results[1].level == "warning"
    assert results[1].lineno == 4
    assert "bad2.rs" in results[1].path
    assert "Print text to the console" in results[1].diff


if __name__ == "__main__":
    mozunit.main()
