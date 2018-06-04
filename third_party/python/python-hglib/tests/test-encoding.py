from tests import common
import hglib
from hglib.util import b

class test_encoding(common.basetest):
    def test_basic(self):
        self.client = hglib.open(encoding='utf-8')
        self.assertEquals(self.client.encoding, b('utf-8'))
