# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import unittest

from mozfile.mozfile import NamedTemporaryFile

from mozunit import main

from mach.logging import LoggingManager

from mozbuild.base import (
    BuildConfig,
    MozbuildObject,
)



curdir = os.path.dirname(__file__)
topsrcdir = os.path.normpath(os.path.join(curdir, '..', '..', '..', '..'))
log_manager = LoggingManager()


class TestMozbuildObject(unittest.TestCase):
    def get_base(self):
        return MozbuildObject(topsrcdir, None, log_manager)

    def test_objdir_config_guess(self):
        base = self.get_base()

        with NamedTemporaryFile() as mozconfig:
            os.environ[b'MOZCONFIG'] = mozconfig.name

            self.assertIsNotNone(base.topobjdir)
            self.assertEqual(len(base.topobjdir.split()), 1)
            self.assertTrue(base.topobjdir.endswith(base._config_guess))

        del os.environ[b'MOZCONFIG']

    def test_config_guess(self):
        # It's difficult to test for exact values from the output of
        # config.guess because they vary depending on platform.
        base = self.get_base()
        result = base._config_guess

        self.assertIsNotNone(result)
        self.assertGreater(len(result), 0)


if __name__ == '__main__':
    main()
