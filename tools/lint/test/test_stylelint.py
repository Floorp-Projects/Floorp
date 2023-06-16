import mozunit
import pytest
from conftest import build

LINTER = "stylelint"
fixed = 0


@pytest.fixture
def stylelint(lint):
    def inner(*args, **kwargs):
        # --ignore-path is because stylelint doesn't have the --no-ignore option
        # and therefore needs to be given an empty file for the tests to work.
        kwargs["extra_args"] = [
            "--ignore-path=tools/lint/test/files/eslint/testprettierignore",
        ]
        return lint(*args, **kwargs)

    return inner


def test_lint_with_global_exclude(lint, config, paths):
    config["exclude"] = ["subdir", "import"]
    # This uses lint directly as we need to not ignore the excludes.
    results = lint(paths(), config=config, root=build.topsrcdir)
    assert len(results) == 0


def test_no_files_to_lint(stylelint, config, paths):
    # A directory with no files to lint.
    results = stylelint(paths("nolint"), root=build.topsrcdir)
    assert results == []

    # Errors still show up even when a directory with no files is passed in.
    results = stylelint(paths("nolint", "subdir/bad.css"), root=build.topsrcdir)
    assert len(results) == 1


def test_stylelint(stylelint, config, create_temp_file):
    contents = """#foo {
    font-size: 12px;
    font-size: 12px;
}
"""
    path = create_temp_file(contents, "bad.css")
    results = stylelint([path], config=config, root=build.topsrcdir)

    assert len(results) == 1


def test_stylelint_fix(stylelint, config, create_temp_file):
    contents = """#foo {
    font-size: 12px;
    font-size: 12px;
}
"""
    path = create_temp_file(contents, "bad.css")
    stylelint([path], config=config, root=build.topsrcdir, fix=True)

    # stylelint returns counts of files fixed, not errors fixed.
    assert fixed == 1


if __name__ == "__main__":
    mozunit.main()
