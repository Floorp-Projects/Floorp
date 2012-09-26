# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import unittest

from tempfile import NamedTemporaryFile

from mozbuild.base import BuildConfig
from mozbuild.base import MozbuildObject
from mozbuild.config import ConfigSettings
from mozbuild.logger import LoggingManager


curdir = os.path.dirname(__file__)
topsrcdir = os.path.normpath(os.path.join(curdir, '..', '..', '..', '..'))
log_manager = LoggingManager()


class TestBuildConfig(unittest.TestCase):
    def test_basic(self):
        c = ConfigSettings()
        c.register_provider(BuildConfig)

        c.build.threads = 6


class TestMozbuildObject(unittest.TestCase):
    def get_base(self):
        settings = ConfigSettings()
        settings.register_provider(BuildConfig)

        return MozbuildObject(topsrcdir, settings, log_manager)

    def test_mozconfig_parsing(self):
        with NamedTemporaryFile(mode='wt') as mozconfig:
            mozconfig.write('mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/some-objdir')
            mozconfig.flush()

            os.environ['MOZCONFIG'] = mozconfig.name

            base = self.get_base()
            base._load_mozconfig()

            self.assertEqual(base.topobjdir, '%s/some-objdir' % topsrcdir)

        del os.environ['MOZCONFIG']

    def test_objdir_config_guess(self):
        base = self.get_base()

        with NamedTemporaryFile() as mozconfig:
            os.environ['MOZCONFIG'] = mozconfig.name

            self.assertIsNotNone(base.topobjdir)
            self.assertEqual(len(base.topobjdir.split()), 1)

        del os.environ['MOZCONFIG']
