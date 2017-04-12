# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''Mixins for parser tests.
'''

from itertools import izip_longest
from pkg_resources import resource_string
import re

from compare_locales import parser


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
        testcontent = re.sub('\r\n?', lambda m: '\n', testcontent)
        return testcontent

    def _test(self, content, refs):
        '''Helper to test the parser.
        Compares the result of parsing content with the given list
        of reference keys and values.
        '''
        self.parser.readContents(content)
        entities = list(self.parser.walk())
        for entity, ref in izip_longest(entities, refs):
            self.assertTrue(entity, 'excess reference entity ' + unicode(ref))
            self.assertTrue(ref, 'excess parsed entity ' + unicode(entity))
            if isinstance(entity, parser.Entity):
                self.assertEqual(entity.key, ref[0])
                self.assertEqual(entity.val, ref[1])
            else:
                self.assertEqual(type(entity).__name__, ref[0])
                self.assertIn(ref[1], entity.all)
