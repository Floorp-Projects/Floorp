import mozunit

from conftest import build

LINTER = "eslint"
fixed = 0


def test_lint_with_global_exclude(lint, config, paths):
    config["exclude"] = ["subdir", "import"]
    results = lint(paths(), config=config, root=build.topsrcdir)
    assert len(results) == 0


def test_no_files_to_lint(lint, config, paths):
    # A directory with no files to lint.
    results = lint(paths("nolint"), root=build.topsrcdir)
    assert results == []

    # Errors still show up even when a directory with no files is passed in.
    results = lint(paths("nolint", "subdir/bad.js"), root=build.topsrcdir)
    assert len(results) == 1


def test_bad_import(lint, config, paths):
    results = lint(paths("import"), config=config, root=build.topsrcdir)
    assert results == 1


def test_fix(lint, config, create_temp_file):
    contents = """/*eslint no-regex-spaces: "error"*/

    var re = /foo   bar/;
    var re = new RegExp("foo   bar");


    var re = /foo   bar/;
    var re = new RegExp("foo   bar");

    var re = /foo   bar/;
    var re = new RegExp("foo   bar");

"""
    path = create_temp_file(contents, "bad.js")
    lint([path], config=config, root=build.topsrcdir, fix=True)

    assert fixed == 6


if __name__ == "__main__":
    mozunit.main()
