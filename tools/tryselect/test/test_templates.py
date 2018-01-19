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
    'env': [
        ([], None),
        (['--env', 'foo=bar', '--env', 'num=10'], {'env': {'foo': 'bar', 'num': '10'}}),
    ],
    'path': [
        ([], None),
        (['dom/indexedDB'], {'env': {'MOZHARNESS_TEST_PATHS': 'dom/indexedDB'}}),
        (['dom/indexedDB', 'testing'],
         {'env': {'MOZHARNESS_TEST_PATHS': 'dom/indexedDB:testing'}}),
        (['invalid/path'], SystemExit),
    ],
    'rebuild': [
        ([], None),
        (['--rebuild', '10'], {'rebuild': 10}),
        (['--rebuild', '1'], SystemExit),
        (['--rebuild', '21'], SystemExit),
    ],
}


def test_templates(template, args, expected):
    parser = ArgumentParser()

    t = all_templates[template]()
    t.add_arguments(parser)

    if inspect.isclass(expected) and issubclass(expected, BaseException):
        with pytest.raises(expected):
            args = parser.parse_args(args)
            t.context(**vars(args))
    else:
        args = parser.parse_args(args)
        assert t.context(**vars(args)) == expected


if __name__ == '__main__':
    mozunit.main()
