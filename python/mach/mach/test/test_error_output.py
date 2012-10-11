# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import imp
import os
import sys
import unittest

from StringIO import StringIO

import mach.main


class TestErrorOutput(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        common_path = os.path.join(os.path.dirname(__file__), 'common.py')
        imp.load_source('mach.commands.error_output_test', common_path)

    def _run_mach(self, args):
        m = mach.main.Mach(os.getcwd())

        stdout = StringIO()
        stderr = StringIO()
        stdout.encoding = 'UTF-8'
        stderr.encoding = 'UTF-8'

        result = m.run(args, stdout=stdout, stderr=stderr)

        return (result, stdout.getvalue(), stderr.getvalue())

    def test_command_error(self):
        result, stdout, stderr = self._run_mach(['throw', '--message',
            'Command Error'])

        self.assertEqual(result, 1)

        self.assertIn(mach.main.COMMAND_ERROR, stdout)

    def test_invoked_error(self):
        result, stdout, stderr = self._run_mach(['throw_deep', '--message',
            'Deep stack'])

        self.assertEqual(result, 1)

        self.assertIn(mach.main.MODULE_ERROR, stdout)
