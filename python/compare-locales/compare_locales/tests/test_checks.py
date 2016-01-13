# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from compare_locales.checks import getChecker
from compare_locales.parser import getParser, Entity
from compare_locales.paths import File


class BaseHelper(unittest.TestCase):
    file = None
    refContent = None

    def setUp(self):
        p = getParser(self.file.file)
        p.readContents(self.refContent)
        self.refList, self.refMap = p.parse()

    def _test(self, content, refWarnOrErrors, with_ref_file=False):
        p = getParser(self.file.file)
        p.readContents(content)
        l10n = [e for e in p]
        assert len(l10n) == 1
        l10n = l10n[0]
        if with_ref_file:
            kwargs = {
                'reference': self.refList
            }
        else:
            kwargs = {}
        checker = getChecker(self.file, **kwargs)
        ref = self.refList[self.refMap[l10n.key]]
        found = tuple(checker.check(ref, l10n))
        self.assertEqual(found, refWarnOrErrors)


class TestProperties(BaseHelper):
    file = File('foo.properties', 'foo.properties')
    refContent = '''some = value
'''

    def testGood(self):
        self._test('''some = localized''',
                   tuple())

    def testMissedEscape(self):
        self._test(r'''some = \u67ood escape, bad \escape''',
                   (('warning', 20, r'unknown escape sequence, \e',
                     'escape'),))


class TestPlurals(BaseHelper):
    file = File('foo.properties', 'foo.properties')
    refContent = '''\
# LOCALIZATION NOTE (downloadsTitleFiles): Semi-colon list of plural forms.
# See: http://developer.mozilla.org/en/docs/Localization_and_Plurals
# #1 number of files
# example: 111 files - Downloads
downloadsTitleFiles=#1 file - Downloads;#1 files - #2
'''

    def testGood(self):
        self._test('''\
# LOCALIZATION NOTE (downloadsTitleFiles): Semi-colon list of plural forms.
# See: http://developer.mozilla.org/en/docs/Localization_and_Plurals
# #1 number of files
# example: 111 files - Downloads
downloadsTitleFiles=#1 file - Downloads;#1 files - #2;#1 filers
''',
                   tuple())

    def testNotUsed(self):
        self._test('''\
# LOCALIZATION NOTE (downloadsTitleFiles): Semi-colon list of plural forms.
# See: http://developer.mozilla.org/en/docs/Localization_and_Plurals
# #1 number of files
# example: 111 files - Downloads
downloadsTitleFiles=#1 file - Downloads;#1 files - Downloads;#1 filers
''',
                   (('warning', 0, 'not all variables used in l10n',
                     'plural'),))

    def testNotDefined(self):
        self._test('''\
# LOCALIZATION NOTE (downloadsTitleFiles): Semi-colon list of plural forms.
# See: http://developer.mozilla.org/en/docs/Localization_and_Plurals
# #1 number of files
# example: 111 files - Downloads
downloadsTitleFiles=#1 file - Downloads;#1 files - #2;#1 #3
''',
                   (('error', 0, 'unreplaced variables in l10n', 'plural'),))


class TestDTDs(BaseHelper):
    file = File('foo.dtd', 'foo.dtd')
    refContent = '''<!ENTITY foo "This is &apos;good&apos;">
<!ENTITY width "10ch">
<!ENTITY style "width: 20ch; height: 280px;">
<!ENTITY minStyle "min-height: 50em;">
<!ENTITY ftd "0">
<!ENTITY formatPercent "This is 100&#037; correct">
<!ENTITY some.key "K">
'''

    def testWarning(self):
        self._test('''<!ENTITY foo "This is &not; good">
''',
                   (('warning', (0, 0), 'Referencing unknown entity `not`',
                     'xmlparse'),))
        # make sure we only handle translated entity references
        self._test(u'''<!ENTITY foo "This is &ƞǿŧ; good">
'''.encode('utf-8'),
            (('warning', (0, 0), u'Referencing unknown entity `ƞǿŧ`',
              'xmlparse'),))

    def testErrorFirstLine(self):
        self._test('''<!ENTITY foo "This is </bad> stuff">
''',
                   (('error', (1, 10), 'mismatched tag', 'xmlparse'),))

    def testErrorSecondLine(self):
        self._test('''<!ENTITY foo "This is
  </bad>
stuff">
''',
                   (('error', (2, 4), 'mismatched tag', 'xmlparse'),))

    def testKeyErrorSingleAmpersand(self):
        self._test('''<!ENTITY some.key "&">
''',
                   (('error', (1, 1), 'not well-formed (invalid token)',
                     'xmlparse'),))

    def testXMLEntity(self):
        self._test('''<!ENTITY foo "This is &quot;good&quot;">
''',
                   tuple())

    def testPercentEntity(self):
        self._test('''<!ENTITY formatPercent "Another 100&#037;">
''',
                   tuple())
        self._test('''<!ENTITY formatPercent "Bad 100% should fail">
''',
                   (('error', (0, 32), 'not well-formed (invalid token)',
                     'xmlparse'),))

    def testNoNumber(self):
        self._test('''<!ENTITY ftd "foo">''',
                   (('warning', 0, 'reference is a number', 'number'),))

    def testNoLength(self):
        self._test('''<!ENTITY width "15miles">''',
                   (('error', 0, 'reference is a CSS length', 'css'),))

    def testNoStyle(self):
        self._test('''<!ENTITY style "15ch">''',
                   (('error', 0, 'reference is a CSS spec', 'css'),))
        self._test('''<!ENTITY style "junk">''',
                   (('error', 0, 'reference is a CSS spec', 'css'),))

    def testStyleWarnings(self):
        self._test('''<!ENTITY style "width:15ch">''',
                   (('warning', 0, 'height only in reference', 'css'),))
        self._test('''<!ENTITY style "width:15em;height:200px;">''',
                   (('warning', 0, "units for width don't match (em != ch)",
                     'css'),))

    def testNoWarning(self):
        self._test('''<!ENTITY width "12em">''', tuple())
        self._test('''<!ENTITY style "width:12ch;height:200px;">''', tuple())
        self._test('''<!ENTITY ftd "0">''', tuple())


class TestEntitiesInDTDs(BaseHelper):
    file = File('foo.dtd', 'foo.dtd')
    refContent = '''<!ENTITY short "This is &brandShortName;">
<!ENTITY shorter "This is &brandShorterName;">
<!ENTITY ent.start "Using &brandShorterName; start to">
<!ENTITY ent.end " end">
'''

    def testOK(self):
        self._test('''<!ENTITY ent.start "Mit &brandShorterName;">''', tuple(),
                   with_ref_file=True)

    def testMismatch(self):
        self._test('''<!ENTITY ent.start "Mit &brandShortName;">''',
                   (('warning', (0, 0),
                     'Entity brandShortName referenced, '
                     'but brandShorterName used in context',
                     'xmlparse'),),
                   with_ref_file=True)

    def testAcross(self):
        self._test('''<!ENTITY ent.end "Mit &brandShorterName;">''',
                   tuple(),
                   with_ref_file=True)

    def testAcrossWithMismatch(self):
        '''If we could tell that ent.start and ent.end are one string,
        we should warn. Sadly, we can't, so this goes without warning.'''
        self._test('''<!ENTITY ent.end "Mit &brandShortName;">''',
                   tuple(),
                   with_ref_file=True)

    def testUnknownWithRef(self):
        self._test('''<!ENTITY ent.start "Mit &foopy;">''',
                   (('warning',
                     (0, 0),
                     'Referencing unknown entity `foopy` '
                     '(brandShorterName used in context, '
                     'brandShortName known)',
                     'xmlparse'),),
                   with_ref_file=True)

    def testUnknown(self):
        self._test('''<!ENTITY ent.end "Mit &foopy;">''',
                   (('warning',
                     (0, 0),
                     'Referencing unknown entity `foopy`'
                     ' (brandShortName, brandShorterName known)',
                     'xmlparse'),),
                   with_ref_file=True)


class TestAndroid(unittest.TestCase):
    """Test Android checker

    Make sure we're hitting our extra rules only if
    we're passing in a DTD file in the embedding/android module.
    """
    apos_msg = u"Apostrophes in Android DTDs need escaping with \\' or " + \
               u"\\u0027, or use \u2019, or put string in quotes."
    quot_msg = u"Quotes in Android DTDs need escaping with \\\" or " + \
               u"\\u0022, or put string in apostrophes."

    def getEntity(self, v):
        return Entity(v, lambda s: s, (0, len(v)), (), (0, 0), (), (),
                      (0, len(v)), ())

    def getDTDEntity(self, v):
        v = v.replace('"', '&quot;')
        return Entity('<!ENTITY foo "%s">' % v,
                      lambda s: s,
                      (0, len(v) + 16), (), (0, 0), (), (9, 12),
                      (14, len(v) + 14), ())

    def test_android_dtd(self):
        """Testing the actual android checks. The logic is involved,
        so this is a lot of nitty gritty detail tests.
        """
        f = File("embedding/android/strings.dtd", "strings.dtd",
                 "embedding/android")
        checker = getChecker(f)
        # good string
        ref = self.getDTDEntity("plain string")
        l10n = self.getDTDEntity("plain localized string")
        self.assertEqual(tuple(checker.check(ref, l10n)),
                         ())
        # dtd warning
        l10n = self.getDTDEntity("plain localized string &ref;")
        self.assertEqual(tuple(checker.check(ref, l10n)),
                         (('warning', (0, 0),
                           'Referencing unknown entity `ref`', 'xmlparse'),))
        # no report on stray ampersand or quote, if not completely quoted
        for i in xrange(3):
            # make sure we're catching unescaped apostrophes,
            # try 0..5 backticks
            l10n = self.getDTDEntity("\\"*(2*i) + "'")
            self.assertEqual(tuple(checker.check(ref, l10n)),
                             (('error', 2*i, self.apos_msg, 'android'),))
            l10n = self.getDTDEntity("\\"*(2*i + 1) + "'")
            self.assertEqual(tuple(checker.check(ref, l10n)),
                             ())
            # make sure we don't report if apos string is quoted
            l10n = self.getDTDEntity('"' + "\\"*(2*i) + "'\"")
            tpl = tuple(checker.check(ref, l10n))
            self.assertEqual(tpl, (),
                             "`%s` shouldn't fail but got %s"
                             % (l10n.val, str(tpl)))
            l10n = self.getDTDEntity('"' + "\\"*(2*i+1) + "'\"")
            tpl = tuple(checker.check(ref, l10n))
            self.assertEqual(tpl, (),
                             "`%s` shouldn't fail but got %s"
                             % (l10n.val, str(tpl)))
            # make sure we're catching unescaped quotes, try 0..5 backticks
            l10n = self.getDTDEntity("\\"*(2*i) + "\"")
            self.assertEqual(tuple(checker.check(ref, l10n)),
                             (('error', 2*i, self.quot_msg, 'android'),))
            l10n = self.getDTDEntity("\\"*(2*i + 1) + "'")
            self.assertEqual(tuple(checker.check(ref, l10n)),
                             ())
            # make sure we don't report if quote string is single quoted
            l10n = self.getDTDEntity("'" + "\\"*(2*i) + "\"'")
            tpl = tuple(checker.check(ref, l10n))
            self.assertEqual(tpl, (),
                             "`%s` shouldn't fail but got %s" %
                             (l10n.val, str(tpl)))
            l10n = self.getDTDEntity('"' + "\\"*(2*i+1) + "'\"")
            tpl = tuple(checker.check(ref, l10n))
            self.assertEqual(tpl, (),
                             "`%s` shouldn't fail but got %s" %
                             (l10n.val, str(tpl)))
        # check for mixed quotes and ampersands
        l10n = self.getDTDEntity("'\"")
        self.assertEqual(tuple(checker.check(ref, l10n)),
                         (('error', 0, self.apos_msg, 'android'),
                          ('error', 1, self.quot_msg, 'android')))
        l10n = self.getDTDEntity("''\"'")
        self.assertEqual(tuple(checker.check(ref, l10n)),
                         (('error', 1, self.apos_msg, 'android'),))
        l10n = self.getDTDEntity('"\'""')
        self.assertEqual(tuple(checker.check(ref, l10n)),
                         (('error', 2, self.quot_msg, 'android'),))

        # broken unicode escape
        l10n = self.getDTDEntity("Some broken \u098 unicode")
        self.assertEqual(tuple(checker.check(ref, l10n)),
                         (('error', 12, 'truncated \\uXXXX escape',
                           'android'),))
        # broken unicode escape, try to set the error off
        l10n = self.getDTDEntity(u"\u9690"*14+"\u006"+"  "+"\u0064")
        self.assertEqual(tuple(checker.check(ref, l10n)),
                         (('error', 14, 'truncated \\uXXXX escape',
                           'android'),))

    def test_android_prop(self):
        f = File("embedding/android/strings.properties", "strings.properties",
                 "embedding/android")
        checker = getChecker(f)
        # good plain string
        ref = self.getEntity("plain string")
        l10n = self.getEntity("plain localized string")
        self.assertEqual(tuple(checker.check(ref, l10n)),
                         ())
        # no dtd warning
        ref = self.getEntity("plain string")
        l10n = self.getEntity("plain localized string &ref;")
        self.assertEqual(tuple(checker.check(ref, l10n)),
                         ())
        # no report on stray ampersand
        ref = self.getEntity("plain string")
        l10n = self.getEntity("plain localized string with apos: '")
        self.assertEqual(tuple(checker.check(ref, l10n)),
                         ())
        # report on bad printf
        ref = self.getEntity("string with %s")
        l10n = self.getEntity("string with %S")
        self.assertEqual(tuple(checker.check(ref, l10n)),
                         (('error', 0, 'argument 1 `S` should be `s`',
                           'printf'),))

    def test_non_android_dtd(self):
        f = File("browser/strings.dtd", "strings.dtd", "browser")
        checker = getChecker(f)
        # good string
        ref = self.getDTDEntity("plain string")
        l10n = self.getDTDEntity("plain localized string")
        self.assertEqual(tuple(checker.check(ref, l10n)),
                         ())
        # dtd warning
        ref = self.getDTDEntity("plain string")
        l10n = self.getDTDEntity("plain localized string &ref;")
        self.assertEqual(tuple(checker.check(ref, l10n)),
                         (('warning', (0, 0),
                          'Referencing unknown entity `ref`', 'xmlparse'),))
        # no report on stray ampersand
        ref = self.getDTDEntity("plain string")
        l10n = self.getDTDEntity("plain localized string with apos: '")
        self.assertEqual(tuple(checker.check(ref, l10n)),
                         ())

    def test_entities_across_dtd(self):
        f = File("browser/strings.dtd", "strings.dtd", "browser")
        p = getParser(f.file)
        p.readContents('<!ENTITY other "some &good.ref;">')
        ref = p.parse()
        checker = getChecker(f, reference=ref[0])
        # good string
        ref = self.getDTDEntity("plain string")
        l10n = self.getDTDEntity("plain localized string")
        self.assertEqual(tuple(checker.check(ref, l10n)),
                         ())
        # dtd warning
        ref = self.getDTDEntity("plain string")
        l10n = self.getDTDEntity("plain localized string &ref;")
        self.assertEqual(tuple(checker.check(ref, l10n)),
                         (('warning', (0, 0),
                           'Referencing unknown entity `ref` (good.ref known)',
                           'xmlparse'),))
        # no report on stray ampersand
        ref = self.getDTDEntity("plain string")
        l10n = self.getDTDEntity("plain localized string with &good.ref;")
        self.assertEqual(tuple(checker.check(ref, l10n)),
                         ())


if __name__ == '__main__':
    unittest.main()
