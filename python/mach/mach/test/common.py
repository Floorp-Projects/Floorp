# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals

import os
import sys
import unittest

import six

try:
    from StringIO import StringIO
except ImportError:
    # TODO io.StringIO causes failures with Python 2 (needs to be sorted out)
    from io import StringIO

from mach.main import Mach

here = os.path.abspath(os.path.dirname(__file__))


class TestBase(unittest.TestCase):
    provider_dir = os.path.join(here, 'providers')

    @classmethod
    def get_mach(cls, provider_files=None, entry_point=None, context_handler=None):
        m = Mach(os.getcwd())
        m.define_category('testing', 'Mach unittest', 'Testing for mach core', 10)
        m.define_category('misc', 'Mach misc', 'Testing for mach core', 20)
        m.populate_context_handler = context_handler

        if provider_files:
            if isinstance(provider_files, six.string_types):
                provider_files = [provider_files]

            for path in provider_files:
                m.load_commands_from_file(os.path.join(cls.provider_dir, path))

        if entry_point:
            m.load_commands_from_entry_point(entry_point)

        return m

    def _run_mach(self, argv, *args, **kwargs):
        m = self.get_mach(*args, **kwargs)

        stdout = StringIO()
        stderr = StringIO()

        if sys.version_info < (3, 0):
            stdout.encoding = 'UTF-8'
            stderr.encoding = 'UTF-8'

        try:
            result = m.run(argv, stdout=stdout, stderr=stderr)
        except SystemExit:
            result = None

        return (result, stdout.getvalue(), stderr.getvalue())
