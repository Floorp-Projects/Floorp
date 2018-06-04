from tests import common
import hglib
from hglib.util import b

class test_log(common.basetest):
    def test_basic(self):
        self.append('a', 'a')
        rev0, node0 = self.client.commit(b('first'), addremove=True)
        self.append('a', 'a')
        rev1, node1 = self.client.commit(b('second'))

        revs = self.client.log()
        revs.reverse()

        self.assertTrue(len(revs) == 2)
        self.assertEquals(revs[1].node, node1)

        self.assertEquals(revs[0], self.client.log(b('0'))[0])
        self.assertEquals(self.client.log(), self.client.log(files=[b('a')]))

        self.assertEquals(self.client.log(), self.client.log(hidden=True))

    # def test_errors(self):
    #     self.assertRaisesRegexp(CommandError, 'abort: unknown revision',
    #                             self.client.log, 'foo')
    #     self.append('a', 'a')
    #     self.client.commit('first', addremove=True)
    #     self.assertRaisesRegexp(CommandError,
    #                             'abort: unknown revision',
    #                             self.client.log, 'bar')
