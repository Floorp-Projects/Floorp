from tests import common
import hglib, datetime
from hglib.util import b

class test_commit(common.basetest):
    def test_user(self):
        self.append('a', 'a')
        rev, node = self.client.commit(b('first'), addremove=True,
                                       user=b('foo'))
        rev = self.client.log(node)[0]
        self.assertEquals(rev.author, b('foo'))

    def test_no_user(self):
        self.append('a', 'a')
        self.assertRaises(hglib.error.CommandError,
                          self.client.commit, b('first'), user=b(''))

    def test_close_branch(self):
        self.append('a', 'a')
        rev0, node0 = self.client.commit(b('first'), addremove=True)
        self.client.branch(b('foo'))
        self.append('a', 'a')
        rev1, node1 = self.client.commit(b('second'))
        revclose = self.client.commit(b('closing foo'), closebranch=True)
        rev0, rev1, revclose = self.client.log([node0, node1, revclose[1]])

        self.assertEquals(self.client.branches(),
                          [(rev0.branch, int(rev0.rev), rev0.node[:12])])

        self.assertEquals(self.client.branches(closed=True),
                          [(revclose.branch, int(revclose.rev),
                            revclose.node[:12]),
                           (rev0.branch, int(rev0.rev), rev0.node[:12])])

    def test_message_logfile(self):
        self.assertRaises(ValueError, self.client.commit, b('foo'),
                          logfile=b('bar'))
        self.assertRaises(ValueError, self.client.commit)

    def test_date(self):
        self.append('a', 'a')
        now = datetime.datetime.now().replace(microsecond=0)
        rev0, node0 = self.client.commit(
            b('first'), addremove=True,
            date=now.isoformat(' ').encode('latin-1'))

        self.assertEquals(now, self.client.tip().date)

    def test_amend(self):
        self.append('a', 'a')
        now = datetime.datetime.now().replace(microsecond=0)
        rev0, node0 = self.client.commit(
            b('first'), addremove=True,
            date=now.isoformat(' ').encode('latin-1'))

        self.assertEquals(now, self.client.tip().date)

        self.append('a', 'a')
        rev1, node1 = self.client.commit(amend=True)
        self.assertEquals(now, self.client.tip().date)
        self.assertNotEquals(node0, node1)
        self.assertEqual(1, len(self.client.log()))
