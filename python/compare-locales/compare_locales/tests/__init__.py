# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''Mixins for parser tests.
'''

from itertools import izip_longest
from pkg_resources import resource_string
import re

from compare_locales.parser import getParser


class ParserTestMixin():
    '''Utility methods used by the parser tests.
    '''
    filename = None

    def setUp(self):
        '''Create a parser for this test.
        '''
        self.parser = getParser(self.filename)

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
        entities = [entity for entity in self.parser]
        for entity, ref in izip_longest(entities, refs):
            self.assertTrue(entity, 'excess reference entity')
            self.assertTrue(ref, 'excess parsed entity')
            self.assertEqual(entity.val, ref[1])
            if ref[0].startswith('_junk'):
                self.assertTrue(re.match(ref[0], entity.key))
            else:
                self.assertEqual(entity.key, ref[0])
