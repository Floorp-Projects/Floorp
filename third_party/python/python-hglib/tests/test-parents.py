from tests import common
from hglib.util import b

class test_parents(common.basetest):
    def test_noparents(self):
        self.assertEquals(self.client.parents(), None)

    def test_basic(self):
        self.append('a', 'a')
        rev, node = self.client.commit(b('first'), addremove=True)
        self.assertEquals(node, self.client.parents()[0].node)
        self.assertEquals(node, self.client.parents(file=b('a'))[0].node)

    def test_two_parents(self):
        pass
