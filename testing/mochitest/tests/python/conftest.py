# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import json
import os
import shutil
import sys
from argparse import Namespace
from cStringIO import StringIO

import pytest
import requests

import mozfile
import mozinfo
import mozinstall
from manifestparser import TestManifest, expression
from mozbuild.base import MozbuildObject

here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)

HARNESS_ROOT_NOT_FOUND = """
Could not find test harness root. Either a build or the 'GECKO_INSTALLER_URL'
environment variable is required.
""".lstrip()


def filter_action(action, lines):
    return filter(lambda x: x['action'] == action, lines)


def _get_harness_root():
    # Check if there is a local build
    harness_root = os.path.join(build.topobjdir, '_tests', 'testing', 'mochitest')
    if os.path.isdir(harness_root):
        return harness_root

    # Check if it was previously set up by another test
    harness_root = os.path.join(os.environ['PYTHON_TEST_TMP'], 'tests', 'mochitest')
    if os.path.isdir(harness_root):
        return harness_root

    # Check if there is a test package to download
    if 'GECKO_INSTALLER_URL' in os.environ:
        base_url = os.environ['GECKO_INSTALLER_URL'].rsplit('/', 1)[0]
        test_packages = requests.get(base_url + '/target.test_packages.json').json()

        dest = os.path.join(os.environ['PYTHON_TEST_TMP'], 'tests')
        for name in test_packages['mochitest']:
            url = base_url + '/' + name
            bundle = os.path.join(os.environ['PYTHON_TEST_TMP'], name)

            r = requests.get(url, stream=True)
            with open(bundle, 'w+b') as fh:
                for chunk in r.iter_content(chunk_size=1024):
                    fh.write(chunk)

            mozfile.extract(bundle, dest)

        return os.path.join(dest, 'mochitest')

    # Couldn't find a harness root, let caller do error handling.
    return None


@pytest.fixture(scope='session')
def setup_harness_root():
    harness_root = _get_harness_root()
    if harness_root:
        sys.path.insert(0, harness_root)

        # Link the test files to the test package so updates are automatically
        # picked up. Fallback to copy on Windows.
        test_root = os.path.join(harness_root, 'tests', 'selftests')
        if not os.path.exists(test_root):
            files = os.path.join(here, 'files')
            if hasattr(os, 'symlink'):
                os.symlink(files, test_root)
            else:
                shutil.copytree(files, test_root)

    elif 'GECKO_INSTALLER_URL' in os.environ:
        # The mochitest tests will run regardless of whether a build exists or not.
        # In a local environment, they should simply be skipped if setup fails. But
        # in automation, we'll need to make sure an error is propagated up.
        pytest.fail(HARNESS_ROOT_NOT_FOUND)
    else:
        # Tests will be marked skipped by the calls to pytest.importorskip() below.
        # We are purposefully not failing here because running |mach python-test|
        # without a build is a perfectly valid use case.
        pass


@pytest.fixture(scope='session')
def binary():
    try:
        return build.get_binary_path()
    except:
        pass

    app = 'firefox'
    bindir = os.path.join(os.environ['PYTHON_TEST_TMP'], app)
    if os.path.isdir(bindir):
        try:
            return mozinstall.get_binary(bindir, app_name=app)
        except:
            pass

    if 'GECKO_INSTALLER_URL' in os.environ:
        bindir = mozinstall.install(
            os.environ['GECKO_INSTALLER_URL'], os.environ['PYTHON_TEST_TMP'])
        return mozinstall.get_binary(bindir, app_name='firefox')


@pytest.fixture(scope='function')
def parser(request):
    parser = pytest.importorskip('mochitest_options')

    app = getattr(request.module, 'APP', 'generic')
    return parser.MochitestArgumentParser(app=app)


@pytest.fixture(scope='function')
def runtests(setup_harness_root, binary, parser, request):
    """Creates an easy to use entry point into the mochitest harness.

    :returns: A function with the signature `*tests, **opts`. Each test is a file name
              (relative to the `files` dir). At least one is required. The opts are
              used to override the default mochitest options, they are optional.
    """
    runtests = pytest.importorskip('runtests')

    mochitest_root = runtests.SCRIPT_DIR
    test_root = os.path.join(mochitest_root, 'tests', 'selftests')

    buf = StringIO()
    options = vars(parser.parse_args([]))
    options.update({
        'app': binary,
        'keep_open': False,
        'log_raw': [buf],
    })

    if not os.path.isdir(runtests.build_obj.bindir):
        package_root = os.path.dirname(mochitest_root)
        options.update({
            'certPath': os.path.join(package_root, 'certs'),
            'utilityPath': os.path.join(package_root, 'bin'),
        })
        options['extraProfileFiles'].append(os.path.join(package_root, 'bin', 'plugins'))

    options.update(getattr(request.module, 'OPTIONS', {}))

    def normalize(test):
        return {
            'name': test,
            'relpath': test,
            'path': os.path.join(test_root, test),
            # add a dummy manifest file because mochitest expects it
            'manifest': os.path.join(test_root, 'mochitest.ini'),
        }

    def inner(*tests, **opts):
        assert len(tests) > 0

        manifest = TestManifest()
        manifest.tests.extend(map(normalize, tests))
        options['manifestFile'] = manifest
        options.update(opts)

        result = runtests.run_test_harness(parser, Namespace(**options))
        out = json.loads('[' + ','.join(buf.getvalue().splitlines()) + ']')
        buf.close()
        return result, out
    return inner


@pytest.fixture(autouse=True)
def skip_using_mozinfo(request, setup_harness_root):
    """Gives tests the ability to skip based on values from mozinfo.

    Example:
        @pytest.mark.skip_mozinfo("!e10s || os == 'linux'")
        def test_foo():
            pass
    """

    runtests = pytest.importorskip('runtests')
    runtests.update_mozinfo()

    skip_mozinfo = request.node.get_marker('skip_mozinfo')
    if skip_mozinfo:
        value = skip_mozinfo.args[0]
        if expression.parse(value, **mozinfo.info):
            pytest.skip("skipped due to mozinfo match: \n{}".format(value))
