# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""Pytest fixtures to help set up Firefox and a tests archive
in test harness selftests.
"""

from __future__ import absolute_import

import os
import shutil
import sys

import mozinstall
import pytest

here = os.path.abspath(os.path.dirname(__file__))

try:
    from mozbuild.base import MozbuildObject
    build = MozbuildObject.from_environment(cwd=here)
except ImportError:
    build = None


HARNESS_ROOT_NOT_FOUND = """
Could not find test harness root. Either a build or the 'GECKO_INSTALLER_URL'
environment variable is required.
""".lstrip()


def _get_test_harness(suite, install_dir):
    # Check if there is a local build
    if build:
        harness_root = os.path.join(build.topobjdir, '_tests', install_dir)
        if os.path.isdir(harness_root):
            return harness_root

    if 'TEST_HARNESS_ROOT' in os.environ:
        harness_root = os.path.join(os.environ['TEST_HARNESS_ROOT'], suite)
        if os.path.isdir(harness_root):
            return harness_root

    # Couldn't find a harness root, let caller do error handling.
    return None


@pytest.fixture(scope='session')
def setup_test_harness(request):
    """Fixture for setting up a mozharness-based test harness like
    mochitest or reftest"""
    def inner(files_dir, *args, **kwargs):
        harness_root = _get_test_harness(*args, **kwargs)
        if harness_root:
            sys.path.insert(0, harness_root)

            # Link the test files to the test package so updates are automatically
            # picked up. Fallback to copy on Windows.
            if files_dir:
                test_root = os.path.join(harness_root, 'tests', 'selftests')
                if not os.path.exists(test_root):
                    if hasattr(os, 'symlink'):
                        os.symlink(files_dir, test_root)
                    else:
                        shutil.copytree(files_dir, test_root)

        elif 'TEST_HARNESS_ROOT' in os.environ:
            # The mochitest tests will run regardless of whether a build exists or not.
            # In a local environment, they should simply be skipped if setup fails. But
            # in automation, we'll need to make sure an error is propagated up.
            pytest.fail(HARNESS_ROOT_NOT_FOUND)
        else:
            # Tests will be marked skipped by the calls to pytest.importorskip() below.
            # We are purposefully not failing here because running |mach python-test|
            # without a build is a perfectly valid use case.
            pass
    return inner


@pytest.fixture(scope='session')
def binary():
    """Return a Firefox binary"""
    try:
        return build.get_binary_path()
    except Exception:
        pass

    app = 'firefox'
    bindir = os.path.join(os.environ['PYTHON_TEST_TMP'], app)
    if os.path.isdir(bindir):
        try:
            return mozinstall.get_binary(bindir, app_name=app)
        except Exception:
            pass

    if 'GECKO_BINARY_PATH' in os.environ:
        return os.environ['GECKO_BINARY_PATH']
