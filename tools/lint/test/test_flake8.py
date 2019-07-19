from __future__ import absolute_import, print_function

import os

import mozunit

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


def test_lint_custom_config_ignored(lint, paths):
    results = lint(paths('custom'))
    assert len(results) == 2

    results = lint(paths('custom/good.py'))
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


def test_lint_excluded_file(lint, paths, config):
    # First file is globally excluded, second one is from .flake8 config.
    files = paths('bad.py', 'subdir/exclude/bad.py')
    config['exclude'] = paths('bad.py')
    results = lint(files, config)
    print(results)
    assert len(results) == 0

    # Make sure excludes also apply when running from a different cwd.
    cwd = paths('subdir')[0]
    os.chdir(cwd)

    results = lint(paths('subdir/exclude'))
    assert len(results) == 0


def test_lint_excluded_file_with_glob(lint, paths, config):
    config['exclude'] = paths('ext/*.configure')

    files = paths('ext')
    results = lint(files, config)
    print(results)
    assert len(results) == 0

    files = paths('ext/bad.configure')
    results = lint(files, config)
    print(results)
    assert len(results) == 0


def test_lint_uses_custom_extensions(lint, paths):
    assert len(lint(paths('ext'))) == 1
    assert len(lint(paths('ext/bad.configure'))) == 1


if __name__ == '__main__':
    mozunit.main()
