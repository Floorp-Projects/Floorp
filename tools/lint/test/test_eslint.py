import mozunit

from conftest import build

LINTER = 'eslint'


def test_lint_with_global_exclude(lint, config, paths):
    config['exclude'] = ['subdir']
    results = lint(paths(), config=config, root=build.topsrcdir)
    assert len(results) == 0


def test_no_files_to_lint(lint, config, paths):
    # A directory with no files to lint.
    results = lint(paths('nolint'), root=build.topsrcdir)
    assert results == []

    # Errors still show up even when a directory with no files is passed in.
    results = lint(paths('nolint', 'subdir/bad.js'), root=build.topsrcdir)
    assert len(results) == 1


if __name__ == '__main__':
    mozunit.main()
