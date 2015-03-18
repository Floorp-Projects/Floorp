# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
import os
from tempfile import mkdtemp
import shutil

from compare_locales.parser import getParser
from compare_locales.paths import File
from compare_locales.compare import ContentComparer


class TestProperties(unittest.TestCase):

    def setUp(self):
        self.tmp = mkdtemp()
        os.mkdir(os.path.join(self.tmp, "merge"))
        self.ref = os.path.join(self.tmp, "en-reference.properties")
        open(self.ref, "w").write("""foo = fooVal
bar = barVal
eff = effVal""")

    def tearDown(self):
        shutil.rmtree(self.tmp)
        del self.tmp

    def testGood(self):
        self.assertTrue(os.path.isdir(self.tmp))
        l10n = os.path.join(self.tmp, "l10n.properties")
        open(l10n, "w").write("""foo = lFoo
bar = lBar
eff = lEff
""")
        cc = ContentComparer()
        cc.set_merge_stage(os.path.join(self.tmp, "merge"))
        cc.compare(File(self.ref, "en-reference.properties", ""),
                   File(l10n, "l10n.properties", ""))
        print cc.observer.serialize()

    def testMissing(self):
        self.assertTrue(os.path.isdir(self.tmp))
        l10n = os.path.join(self.tmp, "l10n.properties")
        open(l10n, "w").write("""bar = lBar
""")
        cc = ContentComparer()
        cc.set_merge_stage(os.path.join(self.tmp, "merge"))
        cc.compare(File(self.ref, "en-reference.properties", ""),
                   File(l10n, "l10n.properties", ""))
        print cc.observer.serialize()
        mergefile = os.path.join(self.tmp, "merge", "l10n.properties")
        self.assertTrue(os.path.isfile(mergefile))
        p = getParser(mergefile)
        p.readFile(mergefile)
        [m, n] = p.parse()
        self.assertEqual(map(lambda e: e.key,  m), ["bar", "eff", "foo"])


class TestDTD(unittest.TestCase):

    def setUp(self):
        self.tmp = mkdtemp()
        os.mkdir(os.path.join(self.tmp, "merge"))
        self.ref = os.path.join(self.tmp, "en-reference.dtd")
        open(self.ref, "w").write("""<!ENTITY foo 'fooVal'>
<!ENTITY bar 'barVal'>
<!ENTITY eff 'effVal'>""")

    def tearDown(self):
        shutil.rmtree(self.tmp)
        del self.tmp

    def testGood(self):
        self.assertTrue(os.path.isdir(self.tmp))
        l10n = os.path.join(self.tmp, "l10n.dtd")
        open(l10n, "w").write("""<!ENTITY foo 'lFoo'>
<!ENTITY bar 'lBar'>
<!ENTITY eff 'lEff'>
""")
        cc = ContentComparer()
        cc.set_merge_stage(os.path.join(self.tmp, "merge"))
        cc.compare(File(self.ref, "en-reference.dtd", ""),
                   File(l10n, "l10n.dtd", ""))
        print cc.observer.serialize()

    def testMissing(self):
        self.assertTrue(os.path.isdir(self.tmp))
        l10n = os.path.join(self.tmp, "l10n.dtd")
        open(l10n, "w").write("""<!ENTITY bar 'lBar'>
""")
        cc = ContentComparer()
        cc.set_merge_stage(os.path.join(self.tmp, "merge"))
        cc.compare(File(self.ref, "en-reference.dtd", ""),
                   File(l10n, "l10n.dtd", ""))
        print cc.observer.serialize()
        mergefile = os.path.join(self.tmp, "merge", "l10n.dtd")
        self.assertTrue(os.path.isfile(mergefile))
        p = getParser(mergefile)
        p.readFile(mergefile)
        [m, n] = p.parse()
        self.assertEqual(map(lambda e: e.key,  m), ["bar", "eff", "foo"])

if __name__ == '__main__':
    unittest.main()
