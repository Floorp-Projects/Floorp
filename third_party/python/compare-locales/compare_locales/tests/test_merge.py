# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import unittest
import filecmp
import os
from tempfile import mkdtemp
import shutil

from compare_locales.parser import getParser
from compare_locales.paths import File
from compare_locales.compare.content import ContentComparer
from compare_locales.compare.observer import Observer
from compare_locales import mozpath


class ContentMixin(object):
    extension = None  # OVERLOAD

    @property
    def ref(self):
        return mozpath.join(self.tmp, "en-reference" + self.extension)

    @property
    def l10n(self):
        return mozpath.join(self.tmp, "l10n" + self.extension)

    def reference(self, content):
        with open(self.ref, "w") as f:
            f.write(content)

    def localized(self, content):
        with open(self.l10n, "w") as f:
            f.write(content)


class TestNonSupported(unittest.TestCase, ContentMixin):
    extension = '.js'

    def setUp(self):
        self.maxDiff = None
        self.tmp = mkdtemp()
        os.mkdir(mozpath.join(self.tmp, "merge"))

    def tearDown(self):
        shutil.rmtree(self.tmp)
        del self.tmp

    def test_good(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""foo = 'fooVal';""")
        self.localized("""foo = 'lfoo';""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.js", ""),
                   File(self.l10n, "l10n.js", ""),
                   mozpath.join(self.tmp, "merge", "l10n.js"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary': {},
             'details': {}
             }
        )
        self.assertTrue(filecmp.cmp(
            self.l10n,
            mozpath.join(self.tmp, "merge", 'l10n.js'))
        )

    def test_missing(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""foo = 'fooVal';""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.add(File(self.ref, "en-reference.js", ""),
               File(self.l10n, "l10n.js", ""),
               mozpath.join(self.tmp, "merge", "l10n.js"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary': {},
             'details': {'l10n.js': [{'missingFile': 'error'}]}
             }
        )
        self.assertTrue(filecmp.cmp(
            self.ref,
            mozpath.join(self.tmp, "merge", 'l10n.js'))
        )

    def test_missing_ignored(self):

        def ignore(*args):
            return 'ignore'
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""foo = 'fooVal';""")
        cc = ContentComparer()
        cc.observers.append(Observer(filter=ignore))
        cc.add(File(self.ref, "en-reference.js", ""),
               File(self.l10n, "l10n.js", ""),
               mozpath.join(self.tmp, "merge", "l10n.js"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary': {},
             'details': {}
             }
        )
        self.assertTrue(filecmp.cmp(
            self.ref,
            mozpath.join(self.tmp, "merge", 'l10n.js'))
        )


class TestDefines(unittest.TestCase, ContentMixin):
    '''Test case for parsers with just CAN_COPY'''
    extension = '.inc'

    def setUp(self):
        self.maxDiff = None
        self.tmp = mkdtemp()
        os.mkdir(mozpath.join(self.tmp, "merge"))

    def tearDown(self):
        shutil.rmtree(self.tmp)
        del self.tmp

    def testGood(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""#filter emptyLines

#define MOZ_LANGPACK_CREATOR mozilla.org

#define MOZ_LANGPACK_CONTRIBUTORS <em:contributor>Suzy Solon</em:contributor>

#unfilter emptyLines
""")
        self.localized("""#filter emptyLines

#define MOZ_LANGPACK_CREATOR mozilla.org

#define MOZ_LANGPACK_CONTRIBUTORS <em:contributor>Jane Doe</em:contributor>

#unfilter emptyLines
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.inc", ""),
                   File(self.l10n, "l10n.inc", ""),
                   mozpath.join(self.tmp, "merge", "l10n.inc"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary':
                {None: {
                    'changed': 1,
                    'changed_w': 2,
                    'unchanged': 1,
                    'unchanged_w': 1
                }},
             'details': {}
             }
        )
        self.assertTrue(filecmp.cmp(
            self.l10n,
            mozpath.join(self.tmp, "merge", 'l10n.inc'))
        )

    def testMissing(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""#filter emptyLines

#define MOZ_LANGPACK_CREATOR mozilla.org

#define MOZ_LANGPACK_CONTRIBUTORS <em:contributor>Suzy Solon</em:contributor>

#unfilter emptyLines
""")
        self.localized("""#filter emptyLines

#define MOZ_LANGPACK_CREATOR mozilla.org

#unfilter emptyLines
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.inc", ""),
                   File(self.l10n, "l10n.inc", ""),
                   mozpath.join(self.tmp, "merge", "l10n.inc"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {
                'summary':
                    {None: {
                        'missing': 1,
                        'missing_w': 2,
                        'unchanged': 1,
                        'unchanged_w': 1
                    }},
                'details':
                    {
                        'l10n.inc': [
                            {'missingEntity': 'MOZ_LANGPACK_CONTRIBUTORS'}
                        ]
                    }
            }
        )
        self.assertTrue(filecmp.cmp(
            self.ref,
            mozpath.join(self.tmp, "merge", 'l10n.inc'))
        )


class TestProperties(unittest.TestCase, ContentMixin):
    extension = '.properties'

    def setUp(self):
        self.maxDiff = None
        self.tmp = mkdtemp()
        os.mkdir(mozpath.join(self.tmp, "merge"))

    def tearDown(self):
        shutil.rmtree(self.tmp)
        del self.tmp

    def testGood(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""foo = fooVal word
bar = barVal word
eff = effVal""")
        self.localized("""foo = lFoo
bar = lBar
eff = lEff word
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.properties", ""),
                   File(self.l10n, "l10n.properties", ""),
                   mozpath.join(self.tmp, "merge", "l10n.properties"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary':
                {None: {
                    'changed': 3,
                    'changed_w': 5
                }},
             'details': {}
             }
        )
        self.assertTrue(filecmp.cmp(
            self.l10n,
            mozpath.join(self.tmp, "merge", 'l10n.properties'))
        )

    def testMissing(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""foo = fooVal
bar = barVal
eff = effVal""")
        self.localized("""bar = lBar
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.properties", ""),
                   File(self.l10n, "l10n.properties", ""),
                   mozpath.join(self.tmp, "merge", "l10n.properties"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary':
                {None: {
                    'changed': 1,
                    'changed_w': 1,
                    'missing': 2,
                    'missing_w': 2
                }},
             'details': {
                 'l10n.properties': [
                     {'missingEntity': u'foo'},
                     {'missingEntity': u'eff'}]
                }
             })
        mergefile = mozpath.join(self.tmp, "merge", "l10n.properties")
        self.assertTrue(os.path.isfile(mergefile))
        p = getParser(mergefile)
        p.readFile(mergefile)
        entities = p.parse()
        self.assertEqual(list(entities.keys()), ["bar", "foo", "eff"])

    def test_missing_file(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""foo = fooVal
bar = barVal
eff = effVal""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.add(File(self.ref, "en-reference.properties", ""),
               File(self.l10n, "l10n.properties", ""),
               mozpath.join(self.tmp, "merge", "l10n.properties"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary':
                {None: {
                    'missingInFiles': 3,
                    'missing_w': 3
                }},
             'details': {
                 'l10n.properties': [
                     {'missingFile': 'error'}]
                }
             })
        mergefile = mozpath.join(self.tmp, "merge", "l10n.properties")
        self.assertTrue(filecmp.cmp(self.ref, mergefile))

    def testError(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""foo = fooVal
bar = %d barVal
eff = effVal""")
        self.localized("""\
bar = %S lBar
eff = leffVal
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.properties", ""),
                   File(self.l10n, "l10n.properties", ""),
                   mozpath.join(self.tmp, "merge", "l10n.properties"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary':
                {None: {
                    'changed': 2,
                    'changed_w': 3,
                    'errors': 1,
                    'missing': 1,
                    'missing_w': 1
                }},
             'details': {
                 'l10n.properties': [
                     {'missingEntity': u'foo'},
                     {'error': u'argument 1 `S` should be `d` '
                               u'at line 1, column 7 for bar'}]
                }
             })
        mergefile = mozpath.join(self.tmp, "merge", "l10n.properties")
        self.assertTrue(os.path.isfile(mergefile))
        p = getParser(mergefile)
        p.readFile(mergefile)
        entities = p.parse()
        self.assertEqual(list(entities.keys()), ["eff", "foo", "bar"])
        self.assertEqual(entities['bar'].val, '%d barVal')

    def testObsolete(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""foo = fooVal
eff = effVal""")
        self.localized("""foo = fooVal
other = obsolete
eff = leffVal
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.properties", ""),
                   File(self.l10n, "l10n.properties", ""),
                   mozpath.join(self.tmp, "merge", "l10n.properties"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary':
                {None: {
                    'changed': 1,
                    'changed_w': 1,
                    'obsolete': 1,
                    'unchanged': 1,
                    'unchanged_w': 1
                }},
             'details': {
                 'l10n.properties': [
                     {'obsoleteEntity': u'other'}]
                }
             })
        mergefile = mozpath.join(self.tmp, "merge", "l10n.properties")
        self.assertTrue(filecmp.cmp(self.l10n, mergefile))

    def test_obsolete_file(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.localized("""foo = fooVal
eff = leffVal
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.remove(File(self.ref, "en-reference.properties", ""),
                  File(self.l10n, "l10n.properties", ""),
                  mozpath.join(self.tmp, "merge", "l10n.properties"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary':
                {},
             'details': {
                 'l10n.properties': [
                     {'obsoleteFile': u'error'}]
                }
             })
        mergefile = mozpath.join(self.tmp, "merge", "l10n.properties")
        self.assertTrue(os.path.isfile(mergefile))

    def test_duplicate(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""foo = fooVal
bar = barVal
eff = effVal
foo = other val for foo""")
        self.localized("""foo = localized
bar = lBar
eff = localized eff
bar = duplicated bar
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.properties", ""),
                   File(self.l10n, "l10n.properties", ""),
                   mozpath.join(self.tmp, "merge", "l10n.properties"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary':
                {None: {
                    'errors': 1,
                    'warnings': 1,
                    'changed': 3,
                    'changed_w': 6
                }},
             'details': {
                 'l10n.properties': [
                     {'warning': u'foo occurs 2 times'},
                     {'error': u'bar occurs 2 times'}]
                }
             })
        mergefile = mozpath.join(self.tmp, "merge", "l10n.properties")
        self.assertTrue(filecmp.cmp(self.l10n, mergefile))


class TestDTD(unittest.TestCase, ContentMixin):
    extension = '.dtd'

    def setUp(self):
        self.maxDiff = None
        self.tmp = mkdtemp()
        os.mkdir(mozpath.join(self.tmp, "merge"))

    def tearDown(self):
        shutil.rmtree(self.tmp)
        del self.tmp

    def testGood(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""<!ENTITY foo 'fooVal'>
<!ENTITY bar 'barVal'>
<!ENTITY eff 'effVal'>""")
        self.localized("""<!ENTITY foo 'lFoo'>
<!ENTITY bar 'lBar'>
<!ENTITY eff 'lEff'>
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.dtd", ""),
                   File(self.l10n, "l10n.dtd", ""),
                   mozpath.join(self.tmp, "merge", "l10n.dtd"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary':
                {None: {
                    'changed': 3,
                    'changed_w': 3
                }},
             'details': {}
             }
        )
        self.assertTrue(filecmp.cmp(
            self.l10n,
            mozpath.join(self.tmp, "merge", 'l10n.dtd'))
        )

    def testMissing(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""<!ENTITY foo 'fooVal'>
<!ENTITY bar 'barVal'>
<!ENTITY eff 'effVal'>""")
        self.localized("""<!ENTITY bar 'lBar'>
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.dtd", ""),
                   File(self.l10n, "l10n.dtd", ""),
                   mozpath.join(self.tmp, "merge", "l10n.dtd"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary':
                {None: {
                    'changed': 1,
                    'changed_w': 1,
                    'missing': 2,
                    'missing_w': 2
                }},
             'details': {
                 'l10n.dtd': [
                     {'missingEntity': u'foo'},
                     {'missingEntity': u'eff'}]
                }
             })
        mergefile = mozpath.join(self.tmp, "merge", "l10n.dtd")
        self.assertTrue(os.path.isfile(mergefile))
        p = getParser(mergefile)
        p.readFile(mergefile)
        entities = p.parse()
        self.assertEqual(list(entities.keys()), ["bar", "foo", "eff"])

    def testJunk(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""<!ENTITY foo 'fooVal'>
<!ENTITY bar 'barVal'>
<!ENTITY eff 'effVal'>""")
        self.localized("""<!ENTITY foo 'fooVal'>
<!ENTY bar 'gimmick'>
<!ENTITY eff 'effVal'>
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.dtd", ""),
                   File(self.l10n, "l10n.dtd", ""),
                   mozpath.join(self.tmp, "merge", "l10n.dtd"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary':
                {None: {
                    'errors': 1,
                    'missing': 1,
                    'missing_w': 1,
                    'unchanged': 2,
                    'unchanged_w': 2
                }},
             'details': {
                 'l10n.dtd': [
                     {'error': u'Unparsed content "<!ENTY bar '
                               u'\'gimmick\'>\n" '
                               u'from line 2 column 1 to '
                               u'line 3 column 1'},
                     {'missingEntity': u'bar'}]
                }
             })
        mergefile = mozpath.join(self.tmp, "merge", "l10n.dtd")
        self.assertTrue(os.path.isfile(mergefile))
        p = getParser(mergefile)
        p.readFile(mergefile)
        entities = p.parse()
        self.assertEqual(list(entities.keys()), ["foo", "eff", "bar"])

    def test_reference_junk(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""<!ENTITY foo 'fooVal'>
<!ENT bar 'bad val'>
<!ENTITY eff 'effVal'>""")
        self.localized("""<!ENTITY foo 'fooVal'>
<!ENTITY eff 'effVal'>
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.dtd", ""),
                   File(self.l10n, "l10n.dtd", ""),
                   mozpath.join(self.tmp, "merge", "l10n.dtd"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary':
                {None: {
                    'warnings': 1,
                    'unchanged': 2,
                    'unchanged_w': 2
                }},
             'details': {
                 'l10n.dtd': [
                     {'warning': 'Parser error in en-US'}]
                }
             })
        mergefile = mozpath.join(self.tmp, "merge", "l10n.dtd")
        self.assertTrue(filecmp.cmp(self.l10n, mergefile))

    def test_reference_xml_error(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""<!ENTITY foo 'fooVal'>
<!ENTITY bar 'bad &val'>
<!ENTITY eff 'effVal'>""")
        self.localized("""<!ENTITY foo 'fooVal'>
<!ENTITY bar 'good val'>
<!ENTITY eff 'effVal'>
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.dtd", ""),
                   File(self.l10n, "l10n.dtd", ""),
                   mozpath.join(self.tmp, "merge", "l10n.dtd"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary':
                {None: {
                    'warnings': 1,
                    'unchanged': 2,
                    'unchanged_w': 2,
                    'changed': 1,
                    'changed_w': 2
                }},
             'details': {
                 'l10n.dtd': [
                     {'warning': u"can't parse en-US value at line 1, "
                                 u"column 0 for bar"}]
                }
             })
        mergefile = mozpath.join(self.tmp, "merge", "l10n.dtd")
        self.assertTrue(filecmp.cmp(self.l10n, mergefile))


class TestFluent(unittest.TestCase):
    maxDiff = None  # we got big dictionaries to compare

    def reference(self, content):
        self.ref = os.path.join(self.tmp, "en-reference.ftl")
        with open(self.ref, "w") as f:
            f.write(content)

    def localized(self, content):
        self.l10n = os.path.join(self.tmp, "l10n.ftl")
        with open(self.l10n, "w") as f:
            f.write(content)

    def setUp(self):
        self.tmp = mkdtemp()
        os.mkdir(os.path.join(self.tmp, "merge"))
        self.ref = self.l10n = None

    def tearDown(self):
        shutil.rmtree(self.tmp)
        del self.tmp
        del self.ref
        del self.l10n

    def testGood(self):
        self.reference("""\
foo = fooVal
bar = barVal
-eff = effVal
""")
        self.localized("""\
foo = lFoo
bar = lBar
-eff = lEff
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.ftl", ""),
                   File(self.l10n, "l10n.ftl", ""),
                   mozpath.join(self.tmp, "merge", "l10n.ftl"))

        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary':
                {None: {
                    'changed': 3,
                    'changed_w': 3
                }},
             'details': {}
             }
        )

        # validate merge results
        mergepath = mozpath.join(self.tmp, "merge", "l10n.ftl")
        self.assertTrue(filecmp.cmp(self.l10n, mergepath))

    def testMissing(self):
        self.reference("""\
foo = fooVal
bar = barVal
-baz = bazVal
eff = effVal
""")
        self.localized("""\
foo = lFoo
eff = lEff
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.ftl", ""),
                   File(self.l10n, "l10n.ftl", ""),
                   mozpath.join(self.tmp, "merge", "l10n.ftl"))

        self.assertDictEqual(
            cc.observers.toJSON(),
            {
                'details': {
                    'l10n.ftl': [
                        {'missingEntity': u'bar'},
                        {'missingEntity': u'-baz'},
                    ],
                },
                'summary': {
                    None: {
                        'changed': 2,
                        'changed_w': 2,
                        'missing': 2,
                        'missing_w': 2,
                    }
                }
            }
        )

        # validate merge results
        mergepath = mozpath.join(self.tmp, "merge", "l10n.ftl")
        self.assertTrue(filecmp.cmp(self.l10n, mergepath))

    def testBroken(self):
        self.reference("""\
foo = fooVal
bar = barVal
eff = effVal
""")
        self.localized("""\
-- Invalid Comment
foo = lFoo
bar lBar
eff = lEff {
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.ftl", ""),
                   File(self.l10n, "l10n.ftl", ""),
                   mozpath.join(self.tmp, "merge", "l10n.ftl"))

        self.assertDictEqual(
            cc.observers.toJSON(),
            {
                'details': {
                    'l10n.ftl': [
                        {'error': u'Unparsed content "-- Invalid Comment" '
                                  u'from line 1 column 1 '
                                  u'to line 1 column 19'},
                        {'error': u'Unparsed content "bar lBar" '
                                  u'from line 3 column 1 '
                                  u'to line 3 column 9'},
                        {'error': u'Unparsed content "eff = lEff {" '
                                  u'from line 4 column 1 '
                                  u'to line 4 column 13'},
                        {'missingEntity': u'bar'},
                        {'missingEntity': u'eff'},
                    ],
                },
                'summary': {
                    None: {
                        'changed': 1,
                        'changed_w': 1,
                        'missing': 2,
                        'missing_w': 2,
                        'errors': 3
                    }
                }
            }
        )

        # validate merge results
        mergepath = mozpath.join(self.tmp, "merge", "l10n.ftl")
        self.assertTrue(os.path.exists(mergepath))

        p = getParser(mergepath)
        p.readFile(mergepath)
        merged_entities = p.parse()
        self.assertEqual(list(merged_entities.keys()), ["foo"])
        merged_foo = merged_entities['foo']

        # foo should be l10n
        p.readFile(self.l10n)
        l10n_entities = p.parse()
        l10n_foo = l10n_entities['foo']
        self.assertTrue(merged_foo.equals(l10n_foo))

    def testMatchingReferences(self):
        self.reference("""\
foo = Reference { bar }
""")
        self.localized("""\
foo = Localized { bar }
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.ftl", ""),
                   File(self.l10n, "l10n.ftl", ""),
                   mozpath.join(self.tmp, "merge", "l10n.ftl"))

        self.assertDictEqual(
            cc.observers.toJSON(),
            {
                'details': {},
                'summary': {
                    None: {
                        'changed': 1,
                        'changed_w': 1
                    }
                }
            }
        )

        # validate merge results
        mergepath = mozpath.join(self.tmp, "merge", "l10n.ftl")
        self.assertTrue(filecmp.cmp(self.l10n, mergepath))

    def testMismatchingReferences(self):
        self.reference("""\
foo = Reference { bar }
bar = Reference { baz }
baz = Reference
""")
        self.localized("""\
foo = Localized { qux }
bar = Localized
baz = Localized { qux }
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.ftl", ""),
                   File(self.l10n, "l10n.ftl", ""),
                   mozpath.join(self.tmp, "merge", "l10n.ftl"))

        self.assertDictEqual(
            cc.observers.toJSON(),
            {
                'details': {
                    'l10n.ftl': [
                            {
                                'warning':
                                    u'Missing message reference: bar '
                                    u'at line 1, column 1 for foo'
                            },
                            {
                                'warning':
                                    u'Obsolete message reference: qux '
                                    u'at line 1, column 19 for foo'
                            },
                            {
                                'warning':
                                    u'Missing message reference: baz '
                                    u'at line 2, column 1 for bar'
                            },
                            {
                                'warning':
                                    u'Obsolete message reference: qux '
                                    u'at line 3, column 19 for baz'
                            },
                    ],
                },
                'summary': {
                    None: {
                        'changed': 3,
                        'changed_w': 3,
                        'warnings': 4
                    }
                }
            }
        )

        # validate merge results
        mergepath = mozpath.join(self.tmp, "merge", "l10n.ftl")
        self.assertTrue(filecmp.cmp(self.l10n, mergepath))

    def testMismatchingAttributes(self):
        self.reference("""
foo = Foo
bar = Bar
  .tender = Attribute value
eff = Eff
""")
        self.localized("""\
foo = lFoo
  .obsolete = attr
bar = lBar
eff = lEff
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.ftl", ""),
                   File(self.l10n, "l10n.ftl", ""),
                   mozpath.join(self.tmp, "merge", "l10n.ftl"))

        self.assertDictEqual(
            cc.observers.toJSON(),
            {
                'details': {
                    'l10n.ftl': [
                            {
                                'error':
                                    u'Obsolete attribute: '
                                    'obsolete at line 2, column 3 for foo'
                            },
                            {
                                'error':
                                    u'Missing attribute: tender at line 3,'
                                    ' column 1 for bar',
                            },
                    ],
                },
                'summary': {
                    None: {'changed': 3, 'changed_w': 5, 'errors': 2}
                }
            }
        )

        # validate merge results
        mergepath = mozpath.join(self.tmp, "merge", "l10n.ftl")
        self.assertTrue(os.path.exists(mergepath))

        p = getParser(mergepath)
        p.readFile(mergepath)
        merged_entities = p.parse()
        self.assertEqual(list(merged_entities.keys()), ["eff"])
        merged_eff = merged_entities['eff']

        # eff should be l10n
        p.readFile(self.l10n)
        l10n_entities = p.parse()
        l10n_eff = l10n_entities['eff']
        self.assertTrue(merged_eff.equals(l10n_eff))

    def test_term_attributes(self):
        self.reference("""
-foo = Foo
-bar = Bar
-baz = Baz
    .attr = Baz Attribute
-qux = Qux
    .attr = Qux Attribute
-missing = Missing
    .attr = An Attribute
""")
        self.localized("""\
-foo = Localized Foo
-bar = Localized Bar
    .attr = Locale-specific Bar Attribute
-baz = Localized Baz
-qux = Localized Qux
    .other = Locale-specific Qux Attribute
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.ftl", ""),
                   File(self.l10n, "l10n.ftl", ""),
                   mozpath.join(self.tmp, "merge", "l10n.ftl"))

        self.assertDictEqual(
            cc.observers.toJSON(),
            {
                'details': {
                    'l10n.ftl': [
                        {'missingEntity': u'-missing'},
                    ],
                },
                'summary': {
                    None: {
                        'changed': 4,
                        'changed_w': 4,
                        'missing': 1,
                        'missing_w': 1,
                    }
                }
            }
        )

        # validate merge results
        mergepath = mozpath.join(self.tmp, "merge", "l10n.ftl")
        self.assertTrue(filecmp.cmp(self.l10n, mergepath))

    def testMismatchingValues(self):
        self.reference("""
foo = Foo
  .foottr = something
bar =
  .tender = Attribute value
""")
        self.localized("""\
foo =
  .foottr = attr
bar = lBar
  .tender = localized
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.ftl", ""),
                   File(self.l10n, "l10n.ftl", ""),
                   mozpath.join(self.tmp, "merge", "l10n.ftl"))

        self.assertDictEqual(
            cc.observers.toJSON(),
            {
                'details': {
                    'l10n.ftl': [
                        {
                            'error':
                                u'Missing value at line 1, column 1 for foo'
                        },
                        {
                            'error':
                                u'Obsolete value at line 3, column 7 for bar',
                        },
                    ]
                },
                'summary': {
                    None: {'changed': 2, 'changed_w': 4, 'errors': 2}
                }
            }
        )

        # validate merge results
        mergepath = mozpath.join(self.tmp, "merge", "l10n.ftl")
        self.assertTrue(os.path.exists(mergepath))

        p = getParser(mergepath)
        p.readFile(mergepath)
        merged_entities = p.parse()
        self.assertEqual(merged_entities, tuple())

    def testMissingGroupComment(self):
        self.reference("""\
foo = fooVal

## Group Comment
bar = barVal
""")
        self.localized("""\
foo = lFoo
bar = lBar
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.ftl", ""),
                   File(self.l10n, "l10n.ftl", ""),
                   mozpath.join(self.tmp, "merge", "l10n.ftl"))

        self.assertDictEqual(
            cc.observers.toJSON(),
            {
                'details': {},
                'summary': {
                    None: {
                        'changed': 2,
                        'changed_w': 2,
                    }
                }
            }
        )

        # validate merge results
        mergepath = mozpath.join(self.tmp, "merge", "l10n.ftl")
        self.assertTrue(filecmp.cmp(self.l10n, mergepath))

    def testMissingAttachedComment(self):
        self.reference("""\
foo = fooVal

# Attached Comment
bar = barVal
""")
        self.localized("""\
foo = lFoo
bar = barVal
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.ftl", ""),
                   File(self.l10n, "l10n.ftl", ""),
                   mozpath.join(self.tmp, "merge", "l10n.ftl"))

        self.assertDictEqual(
            cc.observers.toJSON(),
            {
                'details': {},
                'summary': {
                    None: {
                        'changed': 1,
                        'changed_w': 1,
                        'unchanged': 1,
                        'unchanged_w': 1,
                    }
                }
            }
        )

        # validate merge results
        mergepath = mozpath.join(self.tmp, "merge", "l10n.ftl")
        self.assertTrue(filecmp.cmp(self.l10n, mergepath))

    def testObsoleteStandaloneComment(self):
        self.reference("""\
foo = fooVal
bar = barVal
""")
        self.localized("""\
foo = lFoo

# Standalone Comment

bar = lBar
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.ftl", ""),
                   File(self.l10n, "l10n.ftl", ""),
                   mozpath.join(self.tmp, "merge", "l10n.ftl"))

        self.assertDictEqual(
            cc.observers.toJSON(),
            {
                'details': {},
                'summary': {
                    None: {
                        'changed': 2,
                        'changed_w': 2,
                    }
                }
            }
        )

        # validate merge results
        mergepath = mozpath.join(self.tmp, "merge", "l10n.ftl")
        self.assertTrue(filecmp.cmp(self.l10n, mergepath))

    def test_duplicate(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""foo = fooVal
bar = barVal
eff = effVal
foo = other val for foo""")
        self.localized("""foo = localized
bar = lBar
eff = localized eff
bar = duplicated bar
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.ftl", ""),
                   File(self.l10n, "l10n.ftl", ""),
                   mozpath.join(self.tmp, "merge", "l10n.ftl"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary':
                {None: {
                    'errors': 1,
                    'warnings': 1,
                    'changed': 3,
                    'changed_w': 6
                }},
             'details': {
                 'l10n.ftl': [
                     {'warning': u'foo occurs 2 times'},
                     {'error': u'bar occurs 2 times'}]
                }
             })
        mergefile = mozpath.join(self.tmp, "merge", "l10n.ftl")
        self.assertTrue(filecmp.cmp(self.l10n, mergefile))

    def test_duplicate_attributes(self):
        self.assertTrue(os.path.isdir(self.tmp))
        self.reference("""foo = fooVal
    .attr = good""")
        self.localized("""foo = localized
    .attr = not
    .attr = so
    .attr = good
""")
        cc = ContentComparer()
        cc.observers.append(Observer())
        cc.compare(File(self.ref, "en-reference.ftl", ""),
                   File(self.l10n, "l10n.ftl", ""),
                   mozpath.join(self.tmp, "merge", "l10n.ftl"))
        self.assertDictEqual(
            cc.observers.toJSON(),
            {'summary':
                {None: {
                    'warnings': 3,
                    'changed': 1,
                    'changed_w': 2
                }},
             'details': {
                 'l10n.ftl': [
                     {'warning':
                      u'Attribute "attr" is duplicated '
                      u'at line 2, column 5 for foo'
                      },
                     {'warning':
                      u'Attribute "attr" is duplicated '
                      u'at line 3, column 5 for foo'
                      },
                     {'warning':
                      u'Attribute "attr" is duplicated '
                      u'at line 4, column 5 for foo'
                      },
                  ]
                }
             })
        mergefile = mozpath.join(self.tmp, "merge", "l10n.ftl")
        self.assertTrue(filecmp.cmp(self.l10n, mergefile))


if __name__ == '__main__':
    unittest.main()
