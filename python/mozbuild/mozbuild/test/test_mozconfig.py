# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import unittest

from tempfile import NamedTemporaryFile

from mozbuild.mozconfig import MozconfigLoader


curdir = os.path.dirname(__file__)
topsrcdir = os.path.normpath(os.path.join(curdir, '..', '..', '..', '..'))


class TestMozconfigLoader(unittest.TestCase):
    def get_loader(self):
        return MozconfigLoader(topsrcdir)

    def test_mozconfig_reading_moz_objdir(self):
        with NamedTemporaryFile(mode='wt') as mozconfig:
            mozconfig.write('mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/some-objdir')
            mozconfig.flush()

            loader = self.get_loader()
            result = loader.read_mozconfig(mozconfig.name)

            self.assertEqual(result['topobjdir'], '%s/some-objdir' % topsrcdir)

