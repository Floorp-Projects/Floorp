from tests import common
import hglib
from hglib.util import b

class test_pull(common.basetest):
    def test_basic(self):
        self.append('a', 'a')
        self.client.commit(b('first'), addremove=True)

        self.client.clone(dest=b('other'))
        other = hglib.open(b('other'))

        self.append('a', 'a')
        self.client.commit(b('second'))

        self.assertTrue(other.pull())
        self.assertEquals(self.client.log(), other.log())

    def test_unresolved(self):
        self.append('a', 'a')
        self.client.commit(b('first'), addremove=True)

        self.client.clone(dest=b('other'))
        other = hglib.open(b('other'))

        self.append('a', 'a')
        self.client.commit(b('second'))

        self.append('other/a', 'b')
        self.assertFalse(other.pull(update=True))
        self.assertTrue((b('M'), b('a')) in other.status())
