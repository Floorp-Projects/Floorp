from tests import common
import hglib
from hglib.util import b

class test_copy(common.basetest):
    def test_basic(self):
        self.append('a', 'a')
        self.client.commit(b('first'), addremove=True)

        self.assertTrue(self.client.copy(b('a'), b('b')))
        self.assertEquals(self.client.status(), [(b('A'), b('b'))])
        self.append('c', 'a')
        self.assertTrue(self.client.copy(b('a'), b('c'), after=True))
        self.assertEquals(self.client.status(),
                          [(b('A'), b('b')), (b('A'), b('c'))])

    # hg returns 0 even if there were warnings
    #def test_warnings(self):
    #    self.append('a', 'a')
    #    self.client.commit('first', addremove=True)

    #    self.assertTrue(self.client.copy('a', 'b'))
    #    self.assertFalse(self.client.copy('a', 'b'))
