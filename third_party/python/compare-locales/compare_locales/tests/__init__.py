# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''Mixins for parser tests.
'''

from __future__ import absolute_import

from pkg_resources import resource_string
import re
import unittest

from compare_locales import parser
from compare_locales.checks import getChecker
import six
from six.moves import zip_longest


class ParserTestMixin():
    '''Utility methods used by the parser tests.
    '''
    filename = None

    def setUp(self):
        '''Create a parser for this test.
        '''
        self.parser = parser.getParser(self.filename)

    def tearDown(self):
        'tear down this test'
        del self.parser

    def resource(self, name):
        testcontent = resource_string(__name__, 'data/' + name)
        # fake universal line endings
        testcontent = re.sub(b'\r\n?', lambda m: b'\n', testcontent)
        return testcontent

    def _test(self, unicode_content, refs):
        '''Helper to test the parser.
        Compares the result of parsing content with the given list
        of reference keys and values.
        '''
        self.parser.readUnicode(unicode_content)
        entities = list(self.parser.walk())
        for entity, ref in zip_longest(entities, refs):
            self.assertTrue(entity,
                            'excess reference entity ' + six.text_type(ref))
            self.assertTrue(ref,
                            'excess parsed entity ' + six.text_type(entity))
            if isinstance(entity, parser.Entity):
                self.assertEqual(entity.key, ref[0])
                self.assertEqual(entity.val, ref[1])
                if len(ref) == 3:
                    self.assertIn(ref[2], entity.pre_comment.val)
            else:
                self.assertIsInstance(entity, ref[0])
                self.assertIn(ref[1], entity.all)


class BaseHelper(unittest.TestCase):
    file = None
    refContent = None

    def setUp(self):
        p = parser.getParser(self.file.file)
        p.readContents(self.refContent)
        self.refList = p.parse()

    def _test(self, content, refWarnOrErrors):
        p = parser.getParser(self.file.file)
        p.readContents(content)
        l10n = [e for e in p]
        assert len(l10n) == 1
        l10n = l10n[0]
        checker = getChecker(self.file)
        if checker.needs_reference:
            checker.set_reference(self.refList)
        ref = self.refList[l10n.key]
        found = tuple(checker.check(ref, l10n))
        self.assertEqual(found, refWarnOrErrors)
