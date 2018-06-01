from tests import common
import hglib
from hglib.util import b

class test_push(common.basetest):
    def test_basic(self):
        self.append('a', 'a')
        self.client.commit(b('first'), addremove=True)

        self.client.clone(dest=b('other'))
        other = hglib.open(b('other'))

        # broken in hg, doesn't return 1 if nothing to push
        #self.assertFalse(self.client.push('other'))

        self.append('a', 'a')
        self.client.commit(b('second'))

        self.assertTrue(self.client.push(b('other')))
        self.assertEquals(self.client.log(), other.log())
