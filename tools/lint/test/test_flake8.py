import mozunit
import pytest

LINTER = 'flake8'


def test_lint_single_file(lint, paths):
    results = lint(paths('bad.py'))
    assert len(results) == 2
    assert results[0].rule == 'F401'
    assert results[1].rule == 'E501'
    assert results[1].lineno == 5

    # run lint again to make sure the previous results aren't counted twice
    results = lint(paths('bad.py'))
    assert len(results) == 2


def test_lint_custom_config(lint, paths):
    results = lint(paths('custom'))
    assert len(results) == 0

    results = lint(paths('custom/good.py'))
    assert len(results) == 0

    results = lint(paths('custom', 'bad.py'))
    assert len(results) == 2


@pytest.mark.xfail(
    strict=True, reason="Bug 1277851 - custom configs are ignored if specifying a parent path")
def test_lint_custom_config_from_parent_path(lint, paths):
    results = lint(paths(), collapse_results=True)
    assert paths('custom/good.py')[0] not in results


@pytest.mark.xfail(strict=True, reason="Bug 1277851 - 'exclude' argument is ignored")
def test_lint_excluded_file(lint, paths):
    paths = paths('bad.py')
    results = lint(paths, exclude=paths)
    assert len(results) == 0


if __name__ == '__main__':
    mozunit.main()
