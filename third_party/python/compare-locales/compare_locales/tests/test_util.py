# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from compare_locales import util


class ParseLocalesTest(unittest.TestCase):
    def test_empty(self):
        self.assertEquals(util.parseLocales(''), [])

    def test_all(self):
        self.assertEquals(util.parseLocales('''af
de'''), ['af', 'de'])

    def test_shipped(self):
        self.assertEquals(util.parseLocales('''af
ja win mac
de'''), ['af', 'de', 'ja'])

    def test_sparse(self):
        self.assertEquals(util.parseLocales('''
af

de

'''), ['af', 'de'])
