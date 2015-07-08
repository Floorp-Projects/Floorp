#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import os
import sys

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
        binary = os.path.join(os.path.dirname(self.binary_path), 'b2g-bin')
        cmd.extend(self._build_arg('--binary', binary))
        cmd.extend(self._build_arg('--profile', os.path.join(dirs['abs_gaia_dir'],
                                                             'profile-debug')))
        cmd.extend(self._build_arg('--symbols-path', self.symbols_path))
        cmd.extend(self._build_arg('--browser-arg', self.config.get('browser_arg')))

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
