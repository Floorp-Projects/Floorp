# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This modules contains code for interacting with xpcshell tests.

from __future__ import unicode_literals

import os.path

from StringIO import StringIO

from mozbuild.base import MozbuildObject


class XPCShellRunner(MozbuildObject):
    """Run xpcshell tests."""
    def run_suite(self):
        # TODO hook up to harness runner and support things like shuffle,
        # proper progress updates, etc.
        self._run_make(directory='.', target='xpcshell-tests')

    def run_test(self, test_file, debug=False):
        """Runs an individual xpcshell test."""
        if test_file == 'all':
            self.run_suite()
            return

        # dirname() gets confused if there isn't a trailing slash.
        if os.path.isdir(test_file) and not test_file.endswith(os.path.sep):
            test_file += os.path.sep

        relative_dir = test_file

        if test_file.startswith(self.topsrcdir):
            relative_dir = test_file[len(self.topsrcdir):]

        test_dir = os.path.join(self.topobjdir, '_tests', 'xpcshell',
                os.path.dirname(relative_dir))

        args = {
            'debug': debug,
            'test_dirs': [test_dir],
        }

        if os.path.isfile(test_file):
            args['test_path'] = os.path.basename(test_file)

        self._run_xpcshell_harness(**args)

    def _run_xpcshell_harness(self, test_dirs=None, manifest=None,
        test_path=None, debug=False):

        # Obtain a reference to the xpcshell test runner.
        import runxpcshelltests

        dummy_log = StringIO()
        xpcshell = runxpcshelltests.XPCShellTests(log=dummy_log)
        self.log_manager.enable_unstructured()

        tests_dir = os.path.join(self.topobjdir, '_tests', 'xpcshell')
        modules_dir = os.path.join(self.topobjdir, '_tests', 'modules')

        args = {
            'xpcshell': os.path.join(self.bindir, 'xpcshell'),
            'mozInfo': os.path.join(self.topobjdir, 'mozinfo.json'),
            'symbolsPath': os.path.join(self.distdir, 'crashreporter-symbols'),
            'logfiles': False,
            'testsRootDir': tests_dir,
            'testingModulesDir': modules_dir,
            'profileName': 'firefox',
            'verbose': test_path is not None,
        }

        if manifest is not None:
            args['manifest'] = manifest
        elif test_dirs is not None:
            if isinstance(test_dirs, list):
                args['testdirs'] = test_dirs
            else:
                args['testdirs'] = [test_dirs]
        else:
            raise Exception('One of test_dirs or manifest must be provided.')

        if test_path is not None:
            args['testPath'] = test_path

        # TODO do something with result.
        xpcshell.runTests(**args)

        self.log_manager.disable_unstructured()
