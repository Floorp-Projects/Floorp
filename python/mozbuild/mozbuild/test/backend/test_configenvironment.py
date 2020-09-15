# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import os
import unittest
from mozunit import main

import mozbuild.backend.configenvironment as ConfigStatus

from mozbuild.util import ReadOnlyDict

import mozpack.path as mozpath


class ConfigEnvironment(ConfigStatus.ConfigEnvironment):
    def __init__(self, *args, **kwargs):
        ConfigStatus.ConfigEnvironment.__init__(self, *args, **kwargs)
        # Be helpful to unit tests
        if 'top_srcdir' not in self.substs:
            if os.path.isabs(self.topsrcdir):
                top_srcdir = self.topsrcdir.replace(os.sep, '/')
            else:
                top_srcdir = mozpath.relpath(self.topsrcdir, self.topobjdir).replace(os.sep, '/')

            d = dict(self.substs)
            d['top_srcdir'] = top_srcdir
            self.substs = ReadOnlyDict(d)


class TestEnvironment(unittest.TestCase):
    def test_auto_substs(self):
        '''Test the automatically set values of ACDEFINES, ALLSUBSTS
        and ALLEMPTYSUBSTS.
        '''
        env = ConfigEnvironment('.', '.',
                                defines={'foo': 'bar', 'baz': 'qux 42',
                                         'abc': "d'e'f", 'extra': 'foobar'},
                                non_global_defines=['extra', 'ignore'],
                                substs={'FOO': 'bar', 'FOOBAR': '', 'ABC': 'def',
                                        'bar': 'baz qux', 'zzz': '"abc def"',
                                        'qux': ''})
        # non_global_defines should be filtered out in ACDEFINES.
        # Original order of the defines need to be respected in ACDEFINES
        self.assertEqual(env.substs['ACDEFINES'],
                         """-Dabc='d'\\''e'\\''f' -Dbaz='qux 42' -Dfoo=bar""")
        # Likewise for ALLSUBSTS, which also must contain ACDEFINES
        self.assertEqual(env.substs['ALLSUBSTS'], '''ABC = def
ACDEFINES = -Dabc='d'\\''e'\\''f' -Dbaz='qux 42' -Dfoo=bar
FOO = bar
bar = baz qux
zzz = "abc def"''')
        # ALLEMPTYSUBSTS contains all substs with no value.
        self.assertEqual(env.substs['ALLEMPTYSUBSTS'], '''FOOBAR =
qux =''')


if __name__ == "__main__":
    main()
