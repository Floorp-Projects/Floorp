import os

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


def test_lint_fix(lint, create_temp_file):
    contents = """
import distutils

def foobar():
    pass
""".lstrip()

    path = create_temp_file(contents, name='bad.py')
    results = lint([path])
    assert len(results) == 2

    # Make sure the missing blank line is fixed, but the unused import isn't.
    results = lint([path], fix=True)
    assert len(results) == 1

    # Also test with a directory
    path = os.path.dirname(create_temp_file(contents, name='bad2.py'))
    results = lint([path], fix=True)
    # There should now be two files with 2 combined errors
    assert len(results) == 2
    assert all(r.rule != 'E501' for r in results)


def test_lint_fix_uses_config(lint, create_temp_file):
    contents = """
foo = ['A list of strings', 'that go over 80 characters', 'to test if autopep8 fixes it']
""".lstrip()

    path = create_temp_file(contents, name='line_length.py')
    lint([path], fix=True)

    # Make sure autopep8 reads the global config under lintargs['root']. If it
    # didn't, then the line-length over 80 would get fixed.
    with open(path, 'r') as fh:
        assert fh.read() == contents


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
