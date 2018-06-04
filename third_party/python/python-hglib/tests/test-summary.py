from tests import common
import hglib
from hglib.util import b

class test_summary(common.basetest):
    def test_empty(self):
        d = {b('parent') : [(-1, b('000000000000'), b('tip'), None)],
             b('branch') : b('default'),
             b('commit') : True,
             b('update') : 0}

        self.assertEquals(self.client.summary(), d)

    def test_basic(self):
        self.append('a', 'a')
        rev, node = self.client.commit(b('first'), addremove=True)

        d = {b('parent') : [(0, node[:12], b('tip'), b('first'))],
             b('branch') : b('default'),
             b('commit') : True,
             b('update') : 0}
        if self.client.version >= (3, 5):
            d[b('phases')] = b('1 draft')

        self.assertEquals(self.client.summary(), d)

    def test_commit_dirty(self):
        self.append('a', 'a')
        rev, node = self.client.commit(b('first'), addremove=True)
        self.append('a', 'a')

        d = {b('parent') : [(0, node[:12], b('tip'), b('first'))],
             b('branch') : b('default'),
             b('commit') : False,
             b('update') : 0}
        if self.client.version >= (3, 5):
            d[b('phases')] = b('1 draft')

        self.assertEquals(self.client.summary(), d)

    def test_update(self):
        self.append('a', 'a')
        rev, node = self.client.commit(b('first'), addremove=True)
        self.append('a', 'a')
        self.client.commit(b('second'))
        self.client.update(0)

        d = {b('parent') : [(0, node[:12], None, b('first'))],
             b('branch') : b('default'),
             b('commit') : True,
             b('update') : 1}
        if self.client.version >= (3, 5):
            d[b('phases')] = b('2 draft')

        self.assertEquals(self.client.summary(), d)

    def test_remote(self):
        self.append('a', 'a')
        rev, node = self.client.commit(b('first'), addremove=True)

        self.client.clone(dest=b('other'))
        other = hglib.open('other')

        d = {b('parent') : [(0, node[:12], b('tip'), b('first'))],
             b('branch') : b('default'),
             b('commit') : True,
             b('update') : 0,
             b('remote') : (0, 0, 0, 0)}

        self.assertEquals(other.summary(remote=True), d)

        self.append('a', 'a')
        self.client.commit(b('second'))

        d[b('remote')] = (1, 0, 0, 0)
        self.assertEquals(other.summary(remote=True), d)

        self.client.bookmark(b('bm'))
        d[b('remote')] = (1, 1, 0, 0)
        self.assertEquals(other.summary(remote=True), d)

        other.bookmark(b('bmother'))
        d[b('remote')] = (1, 1, 0, 1)
        if self.client.version < (2, 0, 0):
            d[b('parent')] = [(0, node[:12], b('tip bmother'), b('first'))]
        else:
            d[b('bookmarks')] = b('*bmother')
        self.assertEquals(other.summary(remote=True), d)

        self.append('other/a', 'a')
        rev, node = other.commit(b('second in other'))

        d[b('remote')] = (1, 1, 1, 1)
        if self.client.version < (2, 0, 0):
            tags = b('tip bmother')
        else:
            tags = b('tip')
        d[b('parent')] = [(1, node[:12], tags, b('second in other'))]
        if self.client.version >= (3, 5):
            d[b('phases')] = b('1 draft')

        self.assertEquals(other.summary(remote=True), d)

    def test_two_parents(self):
        self.append('a', 'a')
        rev0, node = self.client.commit(b('first'), addremove=True)

        self.append('a', 'a')
        rev1, node1 = self.client.commit(b('second'))

        self.client.update(rev0)
        self.append('b', 'a')
        rev2, node2 = self.client.commit(b('third'), addremove=True)

        self.client.merge(rev1)

        d = {b('parent') : [(2, node2[:12], b('tip'), b('third')),
                         (1, node1[:12], None, b('second'))],
             b('branch') : b('default'),
             b('commit') : False,
             b('update') : 0}
        if self.client.version >= (3, 5):
            d[b('phases')] = b('3 draft')

        self.assertEquals(self.client.summary(), d)
