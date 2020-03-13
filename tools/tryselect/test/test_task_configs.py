# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import inspect
from argparse import ArgumentParser

import mozunit
import pytest

from tryselect.task_config import all_task_configs


# task configs have a list of tests of the form (input, expected)
TASK_CONFIG_TESTS = {
    'artifact': [
        (['--no-artifact'], None),
        (['--artifact'], {'use-artifact-builds': True}),
    ],
    'chemspill-prio': [
        ([], None),
        (['--chemspill-prio'], {'chemspill-prio': {}}),
    ],
    'env': [
        ([], None),
        (['--env', 'foo=bar', '--env', 'num=10'], {'env': {'foo': 'bar', 'num': '10'}}),
    ],
    'path': [
        ([], None),
        (['dom/indexedDB'], {'env': {'MOZHARNESS_TEST_PATHS': '{"xpcshell": ["dom/indexedDB"]}'}}),
        (['dom/indexedDB', 'testing'],
         {'env': {'MOZHARNESS_TEST_PATHS': '{"xpcshell": ["dom/indexedDB", "testing"]}'}}),
        (['invalid/path'], SystemExit),
    ],
    'rebuild': [
        ([], None),
        (['--rebuild', '10'], {'rebuild': 10}),
        (['--rebuild', '1'], SystemExit),
        (['--rebuild', '21'], SystemExit),
    ],
    'strategy': [
        ([], None),
        (['--strategy', 'foo'], {'optimize-strategies': 'taskgraph.optimize:foo'}),
        (['--strategy', 'foo:bar'], {'optimize-strategies': 'foo:bar'}),
    ],
    'worker-overrides': [
        ([], None),
        (['--worker-override', 'alias=worker/pool'],
         {'worker-overrides': {'alias': 'worker/pool'}}),
        (['--worker-override', 'alias=worker/pool', '--worker-override', 'alias=other/pool'],
         SystemExit),
        (['--worker-suffix', 'b-linux=-dev'],
         {'worker-overrides': {'b-linux': 'gecko-1/b-linux-dev'}}),
        (['--worker-override', 'b-linux=worker/pool' '--worker-suffix', 'b-linux=-dev'],
         SystemExit),
    ]
}


@pytest.fixture
def config_patch_resolver(patch_resolver):
    def inner(paths):
        patch_resolver([], [{'flavor': 'xpcshell', 'srcdir_relpath': path} for path in paths])
    return inner


def test_task_configs(config_patch_resolver, task_config, args, expected):
    parser = ArgumentParser()

    cfg = all_task_configs[task_config]()
    cfg.add_arguments(parser)

    if inspect.isclass(expected) and issubclass(expected, BaseException):
        with pytest.raises(expected):
            args = parser.parse_args(args)
            if task_config == 'path':
                config_patch_resolver(**vars(args))

            cfg.try_config(**vars(args))
    else:
        args = parser.parse_args(args)
        if task_config == 'path':
            config_patch_resolver(**vars(args))
        assert cfg.try_config(**vars(args)) == expected


if __name__ == '__main__':
    mozunit.main()
