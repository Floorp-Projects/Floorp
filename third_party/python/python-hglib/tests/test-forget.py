from tests import common
from hglib.util import b

class test_forget(common.basetest):
    def test_basic(self):
        self.append('a', 'a')
        self.client.add([b('a')])
        self.assertTrue(self.client.forget(b('a')))

    def test_warnings(self):
        self.assertFalse(self.client.forget(b('a')))
        self.append('a', 'a')
        self.client.add([b('a')])
        self.assertFalse(self.client.forget([b('a'), b('b')]))
