#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import os
import re
import sys

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.log import OutputParser, ERROR
from mozharness.mozilla.testing.gaia_test import GaiaTest


class GaiaLinterOutputParser(OutputParser):

    JSHINT_START = "Running jshint..."
    JSHINT_DONE = "xfailed)"
    JSHINT_ERROR = re.compile('(.+): (.*?) \(ERROR\)')

    LAST_FILE = re.compile('----- FILE  :  (.*?) -----')

    GJSLINT_START = "Running gjslint..."
    GJSLINT_ERROR = re.compile('Line (\d+), E:(\d+):')

    GENERAL_ERRORS = (re.compile('make(.*?)\*\*\*(.*?)Error'),)

    def __init__(self, **kwargs):
        self.base_dir = kwargs.pop('base_dir')
        super(GaiaLinterOutputParser, self).__init__(**kwargs)
        self.in_jshint = False
        self.in_gjslint = False
        self.last_file = 'unknown'

    def log_error(self, message, filename=None):
        if not filename:
            self.log('TEST-UNEXPECTED-FAIL | make lint | %s' % message)
        else:
            path = filename
            if self.base_dir in path:
                path = os.path.relpath(filename, self.base_dir)
            self.log('TEST-UNEXPECTED-FAIL | %s | %s' % (path, message),
                     level=ERROR)
        self.num_errors += 1
        self.worst_log_level = self.worst_level(ERROR,
                                                self.worst_log_level)

    def parse_single_line(self, line):
        if not self.in_jshint:
            if self.JSHINT_START in line:
                self.in_jshint = True
                self.in_gjslint = False
        else:
            if self.JSHINT_DONE in line:
                self.in_jshint = False

        if not self.in_gjslint:
            if self.GJSLINT_START in line:
                self.in_gjslint = True

        if self.in_jshint:
            m = self.JSHINT_ERROR.search(line)
            if m:
                self.log_error(m.groups()[1], m.groups()[0])

        if self.in_gjslint:
            m = self.LAST_FILE.search(line)
            if m:
                self.last_file = m.groups()[0]

            m = self.GJSLINT_ERROR.search(line)
            if m:
                self.log_error(line, self.last_file)

        for an_error in self.GENERAL_ERRORS:
            if an_error.search(line):
                self.log_error(line)

        if self.log_output:
            self.info(' %s' % line)

    def evaluate_parser(self):
        # generate the TinderboxPrint line for TBPL
        if self.num_errors:
            self.tsummary = '<em class="testfail">%d errors</em>' % self.num_errors
        else:
            self.tsummary =  "0 errors"

    def print_summary(self, suite_name):
        self.evaluate_parser()
        self.info("TinderboxPrint: %s: %s\n" % (suite_name, self.tsummary))


class GaiaIntegrationTest(GaiaTest):

    virtualenv_modules = ['closure_linter==2.3.13',
                          'python-gflags',
                          ]

    def __init__(self, require_config_file=False):
      GaiaTest.__init__(self, require_config_file)

    def run_tests(self):
        """
        Run the integration test suite.
        """
        dirs = self.query_abs_dirs()

        # Copy the b2g desktop we built to the gaia directory so that it
        # gets used by the marionette-js-runner.
        self.copytree(
            os.path.join(os.path.dirname(self.binary_path)),
            os.path.join(dirs['abs_gaia_dir'], 'b2g'),
            overwrite='clobber'
        )

        cmd = [
            'make',
            'lint',
            'NODE_MODULES_SRC=npm-cache',
            'VIRTUALENV_EXISTS=1'
        ]

        # for Mulet
        if 'firefox' in self.binary_path:
            cmd += ['RUNTIME=%s' % self.binary_path]

        self.make_node_modules()

        output_parser = GaiaLinterOutputParser(
            base_dir=dirs['abs_gaia_dir'],
            config=self.config,
            log_obj=self.log_obj)

        code = self.run_command(cmd, cwd=dirs['abs_gaia_dir'],
           output_parser=output_parser,
           output_timeout=600)

        output_parser.print_summary('gaia-lint')
        self.publish(code)

if __name__ == '__main__':
    gaia_integration_test = GaiaIntegrationTest()
    gaia_integration_test.run_and_exit()
