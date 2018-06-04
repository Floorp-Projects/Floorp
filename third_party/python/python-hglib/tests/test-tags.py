from tests import common
import hglib
from hglib.util import b

class test_tags(common.basetest):
    def test_basic(self):
        self.append('a', 'a')
        rev, node = self.client.commit(b('first'), addremove=True)
        self.client.tag(b('my tag'))
        self.client.tag(b('local tag'), rev=rev, local=True)

        # filecache that was introduced in 2.0 makes us see the local tag, for
        # now we have to reconnect
        if self.client.version < (2, 0, 0):
            self.client = hglib.open()

        tags = self.client.tags()
        self.assertEquals(tags,
                          [(b('tip'), 1, self.client.tip().node[:12], False),
                           (b('my tag'), 0, node[:12], False),
                           (b('local tag'), 0, node[:12], True)])
