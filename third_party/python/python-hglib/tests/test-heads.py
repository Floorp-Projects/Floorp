from tests import common
from hglib.util import b

class test_heads(common.basetest):
    def test_empty(self):
        self.assertEquals(self.client.heads(), [])

    def test_basic(self):
        self.append('a', 'a')
        rev, node0 = self.client.commit(b('first'), addremove=True)
        self.assertEquals(self.client.heads(), [self.client.tip()])

        self.client.branch(b('foo'))
        self.append('a', 'a')
        rev, node1 = self.client.commit(b('second'))

        self.assertEquals(self.client.heads(node0, topological=True), [])
