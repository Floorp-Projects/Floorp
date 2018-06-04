from tests import common
from hglib.util import b

class test_grep(common.basetest):
    def test_basic(self):
        self.append('a', 'a\n')
        self.append('b', 'ab\n')
        self.client.commit(b('first'), addremove=True)

        # no match
        self.assertEquals(list(self.client.grep(b('c'))), [])

        self.assertEquals(list(self.client.grep(b('a'))),
                          [(b('a'), b('0'), b('a')), (b('b'), b('0'), b('ab'))])
        self.assertEquals(list(self.client.grep(b('a'), b('a'))),
                          [(b('a'), b('0'), b('a'))])

        self.assertEquals(list(self.client.grep(b('b'))),
                          [(b('b'), b('0'), b('ab'))])

    def test_options(self):
        self.append('a', 'a\n')
        self.append('b', 'ab\n')
        rev, node = self.client.commit(b('first'), addremove=True)

        self.assertEquals([(b('a'), b('0'), b('+'), b('a')),
                           (b('b'), b('0'), b('+'), b('ab'))],
                          list(self.client.grep(b('a'), all=True)))

        self.assertEquals([(b('a'), b('0')), (b('b'), b('0'))],
                          list(self.client.grep(b('a'), fileswithmatches=True)))

        self.assertEquals([(b('a'), b('0'), b('1'), b('a')),
                           (b('b'), b('0'), b('1'), b('ab'))],
                          list(self.client.grep(b('a'), line=True)))

        self.assertEquals([(b('a'), b('0'), b('test'), b('a')),
                           (b('b'), b('0'), b('test'), b('ab'))],
                          list(self.client.grep(b('a'), user=True)))

        self.assertEquals([(b('a'), b('0'), b('1'), b('+'), b('test')),
                           (b('b'), b('0'), b('1'), b('+'), b('test'))],
                          list(self.client.grep(b('a'), all=True, user=True,
                                                line=True,
                                                fileswithmatches=True)))
