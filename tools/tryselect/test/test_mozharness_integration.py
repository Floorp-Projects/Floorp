# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import imp
import json
import os

import mozunit
import pytest

from tryselect.tasks import build, resolve_tests_by_suite


@pytest.fixture(scope='module')
def get_mozharness_test_paths():
    scriptdir = os.path.join(build.topsrcdir, 'testing', 'mozharness', 'scripts')
    files = imp.find_module('desktop_unittest', [scriptdir])
    mod = imp.load_module('scripts.desktop_unittest', *files)
    du = mod.DesktopUnittest(require_config_file=False)
    return du._get_mozharness_test_paths


@pytest.fixture
def all_suites(patch_resolver):
    from moztest.resolve import _test_flavors, _test_subsuites
    all_suites = []
    for flavor in _test_flavors:
        all_suites.append({'flavor': flavor, 'srcdir_relpath': 'test'})

    for flavor, subsuite in _test_subsuites:
        all_suites.append({'flavor': flavor, 'subsuite': subsuite, 'srcdir_relpath': 'test'})

    patch_resolver([], all_suites)
    return resolve_tests_by_suite(['test'])


KNOWN_FAILURES = (
    'browser-chrome-instrumentation',
    'cppunittest',
    'gtest',
    'jittest',
    'jittest-chunked',
    'jittest1',
    'jittest2',
    'jsreftest',
    'mochitest-devtools-webreplay',
    'reftest-gpu',
    'reftest-no-accel',
    'reftest-qr',
    'valgrind-plain',
)
"""A suite being listed here means it won't work properly with
MOZHARNESS_TEST_PATHS (the mechanism |mach try fuzzy <path>| uses.
"""


def generate_mozharness_suite_names():
    configdir = os.path.join(build.topsrcdir, 'testing', 'mozharness', 'configs', 'unittests')
    seen = set()

    for platform in ('linux', 'mac', 'win'):
        files = imp.find_module('{}_unittest'.format(platform), [configdir])
        mod = imp.load_module('config.{}_unittest'.format(platform), *files)
        config = mod.config

        for category in sorted(config['suite_definitions']):
            if category == 'mozmill':
                continue

            for suite in sorted(config['all_{}_suites'.format(category)]):
                result = category, suite

                if result in seen:
                    continue

                seen.add(result)

                if suite in KNOWN_FAILURES:
                    result = pytest.param(result, marks=pytest.mark.xfail)

                yield result


@pytest.mark.parametrize('mozharness_suite', generate_mozharness_suite_names(),
                         ids=lambda val: val[1])
def test_mozharness_suites(get_mozharness_test_paths, all_suites, mozharness_suite):
    """An integration test to make sure the suites returned by
    `tasks.resolve_tests_by_suite` match up with the names defined in
    mozharness' desktop_unittest.py script.
    """
    os.environ['MOZHARNESS_TEST_PATHS'] = json.dumps(all_suites)
    assert get_mozharness_test_paths(*mozharness_suite)


if __name__ == '__main__':
    mozunit.main()
