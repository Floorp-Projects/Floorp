from tests import common
import hglib
from hglib.util import b

class test_branch(common.basetest):
    def test_empty(self):
        self.assertEquals(self.client.branch(), b('default'))

    def test_basic(self):
        self.assertEquals(self.client.branch(b('foo')), b('foo'))
        self.append('a', 'a')
        rev, node = self.client.commit(b('first'), addremove=True)

        rev = self.client.log(node)[0]

        self.assertEquals(rev.branch, b('foo'))
        self.assertEquals(self.client.branches(),
                          [(rev.branch, int(rev.rev), rev.node[:12])])

    def test_reset_with_name(self):
        self.assertRaises(ValueError, self.client.branch, b('foo'), clean=True)

    def test_reset(self):
        self.client.branch(b('foo'))
        self.assertEquals(self.client.branch(clean=True), b('default'))

    def test_exists(self):
        self.append('a', 'a')
        self.client.commit(b('first'), addremove=True)
        self.client.branch(b('foo'))
        self.append('a', 'a')
        self.client.commit(b('second'))
        self.assertRaises(hglib.error.CommandError,
                          self.client.branch, b('default'))

    def test_force(self):
        self.append('a', 'a')
        self.client.commit(b('first'), addremove=True)
        self.client.branch(b('foo'))
        self.append('a', 'a')
        self.client.commit(b('second'))

        self.assertRaises(hglib.error.CommandError,
                          self.client.branch, b('default'))
        self.assertEquals(self.client.branch(b('default'), force=True),
                          b('default'))
