#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import os
import sys
import glob
import subprocess
import json

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.mozilla.testing.gaia_test import GaiaTest
from mozharness.mozilla.testing.unittest import TestSummaryOutputParserHelper


class GaiaUnitTest(GaiaTest):
    def __init__(self, require_config_file=False):
        GaiaTest.__init__(self, require_config_file)

    def pull(self, **kwargs):
        GaiaTest.pull(self, **kwargs)

    def run_tests(self):
        """
        Run the unit test suite.
        """
        dirs = self.query_abs_dirs()

        self.make_node_modules()

        # make the gaia profile
        self.make_gaia(dirs['abs_gaia_dir'],
                       self.config.get('xre_path'),
                       xre_url=self.config.get('xre_url'),
                       debug=True)

        # build the testrunner command arguments
        python = self.query_python_path('python')
        cmd = [python, '-u', os.path.join(dirs['abs_runner_dir'],
                                          'gaia_unit_test',
                                          'main.py')]
        executable = 'firefox'
        if 'b2g' in self.binary_path:
                executable = 'b2g-bin'

        profile = os.path.join(dirs['abs_gaia_dir'], 'profile-debug')
        binary = os.path.join(os.path.dirname(self.binary_path), executable)
        cmd.extend(self._build_arg('--binary', binary))
        cmd.extend(self._build_arg('--profile', profile))
        cmd.extend(self._build_arg('--symbols-path', self.symbols_path))
        cmd.extend(self._build_arg('--browser-arg', self.config.get('browser_arg')))

        # Add support for chunking
        if self.config.get('total_chunks') and self.config.get('this_chunk'):
                chunker = [ os.path.join(dirs['abs_gaia_dir'], 'bin', 'chunk'),
                            self.config.get('total_chunks'), self.config.get('this_chunk') ]

                disabled_tests = []
                disabled_manifest = os.path.join(dirs['abs_runner_dir'],
                                                 'gaia_unit_test',
                                                 'disabled.json')
                with open(disabled_manifest, 'r') as m:
                    try:
                        disabled_tests = json.loads(m.read())
                    except:
                        print "Error while decoding disabled.json; please make sure this file has valid JSON syntax."
                        sys.exit(1)

                # Construct a list of all tests
                unit_tests  = []
                for path in ('apps', 'tv_apps'):
                    test_root    = os.path.join(dirs['abs_gaia_dir'], path)
                    full_paths   = glob.glob(os.path.join(test_root, '*/test/unit/*_test.js'))
                    unit_tests  += map(lambda x: os.path.relpath(x, test_root), full_paths)

                # Remove the tests that are disabled
                active_unit_tests = filter(lambda x: x not in disabled_tests, unit_tests)

                # Chunk the list as requested
                tests_to_run = subprocess.check_output(chunker + active_unit_tests).strip().split(' ')

                cmd.extend(tests_to_run)

        output_parser = TestSummaryOutputParserHelper(config=self.config,
                                                      log_obj=self.log_obj,
                                                      error_list=self.error_list)

        upload_dir = self.query_abs_dirs()['abs_blob_upload_dir']
        if not os.path.isdir(upload_dir):
            self.mkdir_p(upload_dir)

        env = self.query_env()
        env['MOZ_UPLOAD_DIR'] = upload_dir
        # I don't like this output_timeout hardcode, but bug 920153
        code = self.run_command(cmd, env=env,
                                output_parser=output_parser,
                                output_timeout=1760)

        output_parser.print_summary('gaia-unit-tests')
        self.publish(code)

if __name__ == '__main__':
    gaia_unit_test = GaiaUnitTest()
    gaia_unit_test.run_and_exit()
