from tests import common
from hglib.util import b

class test_annotate(common.basetest):
    def test_basic(self):
        self.append('a', 'a\n')
        rev, node0 = self.client.commit(b('first'), addremove=True)
        self.append('a', 'a\n')
        rev, node1 = self.client.commit(b('second'))

        self.assertEquals(list(self.client.annotate(b('a'))),
                          [(b('0'), b('a')), (b('1'), b('a'))])
        self.assertEquals(list(
            self.client.annotate(
                b('a'), user=True, file=True,
                number=True, changeset=True, line=True, verbose=True)),
                          [(b('test 0 ') + node0[:12] + b(' a:1'), b('a')),
                           (b('test 1 ') + node1[:12] + b(' a:2'), b('a'))])

    def test_files(self):
        self.append('a', 'a\n')
        rev, node0 = self.client.commit(b('first'), addremove=True)
        self.append('b', 'b\n')
        rev, node1 = self.client.commit(b('second'), addremove=True)
        self.assertEquals(list(self.client.annotate([b('a'), b('b')])),
                          [(b('0'), b('a')), (b('1'), b('b'))])

    def test_two_colons(self):
        self.append('a', 'a: b\n')
        self.client.commit(b('first'), addremove=True)
        self.assertEquals(list(self.client.annotate(b('a'))),
                          [(b('0'), b('a: b'))])
