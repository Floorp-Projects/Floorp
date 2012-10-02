# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os

from mozbuild.testing.test import TestRunner


class ReftestRunner(TestRunner):
    """Easily run reftests.

    This currently contains just the basics for running reftests. We may want
    to hook up result parsing, etc.
    """

    def _manifest_file(self, suite):
        """Returns the manifest file used for a given test suite."""
        files = {
          'reftest': 'reftest.list',
          'crashtest': 'crashtests.list'
        }
        assert suite in files
        return files[suite]

    def _find_manifest(self, suite, test_file):
        assert test_file
        parsed = self._parse_test_path(test_file)
        if parsed['is_dir']:
            return os.path.join(parsed['normalized'], self._manifest_file(suite))

        if parsed['normalized'].endswith('.list'):
            return parsed['normalized']

        raise Exception('Running a single test is not currently supported')

    def run_reftest_test(self, test_file=None, suite=None):
        """Runs a reftest.

        test_file is a path to a test file. It can be a relative path from the
        top source directory, an absolute filename, or a directory containing
        test files.

        suite is the type of reftest to run. It can be one of ('reftest',
        'crashtest').
        """

        if suite not in ('reftest', 'crashtest'):
            raise Exception('None or unrecognized reftest suite type.')

        if test_file:
            path = self._find_manifest(suite, test_file)
            if not os.path.exists(path):
                raise Exception('No manifest file was found at %s.' % path)
            env = {'TEST_PATH': path}
        else:
            env = {}

        # TODO hook up harness via native Python
        self._run_make(directory='.', target=suite, append_env=env)
