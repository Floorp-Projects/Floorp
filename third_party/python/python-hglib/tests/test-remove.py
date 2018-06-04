from tests import common
from hglib.util import b

class test_remove(common.basetest):
    def test_basic(self):
        self.append('a', 'a')
        self.client.commit(b('first'), addremove=True)
        self.assertTrue(self.client.remove([b('a')]))

    def test_warnings(self):
        self.append('a', 'a')
        self.client.commit(b('first'), addremove=True)
        self.assertFalse(self.client.remove([b('a'), b('b')]))
