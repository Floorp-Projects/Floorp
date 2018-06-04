from tests import common
from hglib.util import b

class test_bundle(common.basetest):
    def test_no_changes(self):
        self.append('a', 'a')
        rev, node0 = self.client.commit(b('first'), addremove=True)
        self.assertFalse(self.client.bundle(b('bundle'), destrepo=b('.')))

    def test_basic(self):
        self.append('a', 'a')
        rev, node0 = self.client.commit(b('first'), addremove=True)
        self.client.clone(dest=b('other'))

        self.append('a', 'a')
        rev, node1 = self.client.commit(b('second'))

        self.assertTrue(self.client.bundle(b('bundle'), destrepo=b('other')))
