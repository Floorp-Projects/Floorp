import mozunit

LINTER = "trojan-source"


def test_lint_trojan_source(lint, paths):
    results = lint(paths())
    print(results)
    assert len(results) == 3

    assert "disallowed characters" in results[0].message
    assert results[0].level == "error"
    assert "commenting-out.cpp" in results[0].relpath

    assert "disallowed characters" in results[1].message
    assert results[1].level == "error"
    assert "early-return.py" in results[1].relpath

    assert "disallowed characters" in results[2].message
    assert results[2].level == "error"
    assert "invisible-function.rs" in results[2].relpath


if __name__ == "__main__":
    mozunit.main()
