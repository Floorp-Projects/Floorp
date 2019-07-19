from __future__ import absolute_import, print_function

import mozunit

from conftest import build

LINTER = 'eslint'


def test_lint_with_global_exclude(lint, config, paths):
    config['exclude'] = ['subdir']
    results = lint(paths(), config=config, root=build.topsrcdir)
    assert len(results) == 0


def test_no_files_to_lint(lint, config, paths):
    ret = lint(paths('nolint'), root=build.topsrcdir)
    assert ret == []


if __name__ == '__main__':
    mozunit.main()
