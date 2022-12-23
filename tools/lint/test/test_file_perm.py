import mozunit
import pytest

LINTER = "file-perm"


@pytest.mark.lint_config(name="file-perm")
def test_lint_file_perm(lint, paths):
    results = lint(paths("no-shebang"), collapse_results=True)

    assert results.keys() == {
        "no-shebang/bad.c",
        "no-shebang/bad-shebang.c",
        "no-shebang/bad.png",
    }

    for path, issues in results.items():
        for issue in issues:
            assert "permissions on a source" in issue.message
            assert issue.level == "error"


@pytest.mark.lint_config(name="maybe-shebang-file-perm")
def test_lint_shebang_file_perm(config, lint, paths):
    results = lint(paths("maybe-shebang"))

    assert len(results) == 1

    assert "permissions on a source" in results[0].message
    assert results[0].level == "error"
    assert results[0].relpath == "maybe-shebang/bad.js"


if __name__ == "__main__":
    mozunit.main()
