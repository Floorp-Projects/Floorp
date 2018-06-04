from tests import common
import hglib
from hglib.util import b

class test_resolve(common.basetest):
    def setUp(self):
        common.basetest.setUp(self)

        self.append('a', 'a')
        self.append('b', 'b')
        rev, self.node0 = self.client.commit(b('first'), addremove=True)

        self.append('a', 'a')
        self.append('b', 'b')
        rev, self.node1 = self.client.commit(b('second'))

    def test_basic(self):
        self.client.update(self.node0)
        self.append('a', 'b')
        self.append('b', 'a')
        rev, self.node3 = self.client.commit(b('third'))

        self.assertRaises(hglib.error.CommandError, self.client.merge,
                          self.node1)
        self.assertRaises(hglib.error.CommandError,
                          self.client.resolve, all=True)

        self.assertEquals([(b('U'), b('a')), (b('U'), b('b'))],
                          self.client.resolve(listfiles=True))

        self.client.resolve(b('a'), mark=True)
        self.assertEquals([(b('R'), b('a')), (b('U'), b('b'))],
                          self.client.resolve(listfiles=True))
