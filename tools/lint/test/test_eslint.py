import mozunit

from conftest import build

LINTER = 'eslint'


def test_lint_with_global_exclude(lint, config, paths):
    config['exclude'] = ['subdir']
    results = lint(paths(), config=config, root=build.topsrcdir)
    assert len(results) == 0


if __name__ == '__main__':
    mozunit.main()
