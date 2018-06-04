from tests import common
import hglib

class test_hglib(common.basetest):
    def setUp(self):
        pass

    def test_close_fds(self):
        """A weird Python bug that has something to do to inherited file
        descriptors, see http://bugs.python.org/issue12786
        """
        common.basetest.setUp(self)
        client2 = hglib.open()
        self.client.close()

    def test_open_nonexistent(self):
        # setup stuff necessary for basetest.tearDown()
        self.clients = []
        self._oldopen = hglib.client.hgclient.open
        try:
            self.clients.append(hglib.open('inexistent'))
            # hg 3.5 can't report error (fixed by 7332bf4ae959)
            #self.fail('ServerError not raised')
        except hglib.error.ServerError as inst:
            self.assertTrue('inexistent' in str(inst))
