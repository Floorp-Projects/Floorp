import mozunit
import pytest
from conftest import build

LINTER = "eslint"
fixed = 0


@pytest.fixture
def eslint(lint):
    def inner(*args, **kwargs):
        # --no-ignore is for ESLint to avoid the .eslintignore file.
        # --ignore-path is because Prettier doesn't have the --no-ignore option
        # and therefore needs to be given an empty file for the tests to work.
        kwargs["extra_args"] = [
            "--no-ignore",
            "--ignore-path=tools/lint/test/files/eslint/testprettierignore",
        ]
        return lint(*args, **kwargs)

    return inner


def test_lint_with_global_exclude(lint, config, paths):
    config["exclude"] = ["subdir", "import"]
    # This uses lint directly as we need to not ignore the excludes.
    results = lint(paths(), config=config, root=build.topsrcdir)
    assert len(results) == 0


def test_no_files_to_lint(eslint, config, paths):
    # A directory with no files to lint.
    results = eslint(paths("nolint"), root=build.topsrcdir)
    assert results == []

    # Errors still show up even when a directory with no files is passed in.
    results = eslint(paths("nolint", "subdir/bad.js"), root=build.topsrcdir)
    assert len(results) == 1


def test_bad_import(eslint, config, paths):
    results = eslint(paths("import"), config=config, root=build.topsrcdir)
    assert results == 1


def test_eslint_rule(eslint, config, create_temp_file):
    contents = """var re = /foo   bar/;
var re = new RegExp("foo   bar");
"""
    path = create_temp_file(contents, "bad.js")
    results = eslint(
        [path], config=config, root=build.topsrcdir, rules=["no-regex-spaces: error"]
    )

    assert len(results) == 2


def test_eslint_fix(eslint, config, create_temp_file):
    contents = """/*eslint no-regex-spaces: "error"*/

var re = /foo   bar/;
var re = new RegExp("foo   bar");

var re = /foo   bar/;
var re = new RegExp("foo   bar");

var re = /foo   bar/;
var re = new RegExp("foo   bar");
"""
    path = create_temp_file(contents, "bad.js")
    eslint([path], config=config, root=build.topsrcdir, fix=True)

    # ESLint returns counts of files fixed, not errors fixed.
    assert fixed == 1


def test_prettier_rule(eslint, config, create_temp_file):
    contents = """var re = /foobar/;
    var re = "foo";
"""
    path = create_temp_file(contents, "bad.js")
    results = eslint([path], config=config, root=build.topsrcdir)

    assert len(results) == 1


def test_prettier_fix(eslint, config, create_temp_file):
    contents = """var re = /foobar/;
    var re = "foo";
"""
    path = create_temp_file(contents, "bad.js")
    eslint([path], config=config, root=build.topsrcdir, fix=True)

    # Prettier returns counts of files fixed, not errors fixed.
    assert fixed == 1


if __name__ == "__main__":
    mozunit.main()
