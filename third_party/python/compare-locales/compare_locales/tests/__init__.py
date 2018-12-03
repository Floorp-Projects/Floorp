# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''Mixins for parser tests.
'''

from __future__ import absolute_import

from pkg_resources import resource_string
import re

from compare_locales import parser
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
