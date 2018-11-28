# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import inspect
from argparse import ArgumentParser

import mozunit
import pytest

from tryselect.templates import all_templates


# templates have a list of tests of the form (input, expected)
TEMPLATE_TESTS = {
    'artifact': [
        (['--no-artifact'], None),
        (['--artifact'], {'artifact': {'enabled': '1'}}),
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
    'gecko-profile': [
        ([], None),
        (['--talos-profile'], {'gecko-profile': True}),
        (['--geckoProfile'], {'gecko-profile': True}),
        (['--gecko-profile'], {'gecko-profile': True}),
    ],
}


@pytest.fixture
def template_patch_resolver(patch_resolver):
    def inner(paths):
        patch_resolver([], [{'flavor': 'xpcshell', 'srcdir_relpath': path} for path in paths])
    return inner


def test_templates(template_patch_resolver, template, args, expected):
    parser = ArgumentParser()

    t = all_templates[template]()
    t.add_arguments(parser)

    if inspect.isclass(expected) and issubclass(expected, BaseException):
        with pytest.raises(expected):
            args = parser.parse_args(args)
            if template == 'path':
                template_patch_resolver(**vars(args))
            t.context(**vars(args))
    else:
        args = parser.parse_args(args)
        if template == 'path':
            template_patch_resolver(**vars(args))
        assert t.context(**vars(args)) == expected


if __name__ == '__main__':
    mozunit.main()
