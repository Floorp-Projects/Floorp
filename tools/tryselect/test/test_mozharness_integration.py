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
    'browser-chrome-addons',
    'browser-chrome-chunked',
    'browser-chrome-coverage',
    'browser-chrome-instrumentation',
    'chrome-chunked',
    'cppunittest',
    'gtest',
    'jittest',
    'jittest-chunked',
    'jittest1',
    'jittest2',
    'jsreftest',
    'reftest-no-accel',
    'mochitest-devtools-chrome',
    'mochitest-devtools-chrome-chunked',
    'mochitest-devtools-chrome-coverage',
    'plain',
    'plain-chunked',
    'plain-chunked-coverage',
    'plain-clipboard',
    'plain-gpu',
    'valgrind-plain',
    'xpcshell-addons',
    'xpcshell-coverage',
)
"""A suite being listed here means it won't work properly with
MOZHARNESS_TEST_PATHS (the mechanism |mach try fuzzy <path>| uses.
"""


def generate_mozharness_suite_names():
    configdir = os.path.join(build.topsrcdir, 'testing', 'mozharness', 'configs', 'unittests')
    files = imp.find_module('linux_unittest', [configdir])
    mod = imp.load_module('config.linux_unittest', *files)
    config = mod.config

    for category in sorted(config['suite_definitions']):
        if category == 'mozmill':
            continue

        suites = config['all_{}_suites'.format(category)]

        for suite in sorted(suites):
            result = category, suite

            if suite in KNOWN_FAILURES:
                result = pytest.param(result, marks=pytest.mark.xfail)

            yield result


@pytest.mark.parametrize('mozharness_suite', list(generate_mozharness_suite_names()),
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
