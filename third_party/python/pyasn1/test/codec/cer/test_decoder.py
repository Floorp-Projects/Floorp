from pyasn1.type import univ
from pyasn1.codec.cer import decoder
from pyasn1.compat.octets import ints2octs, str2octs, null
from pyasn1.error import PyAsn1Error
from sys import version_info
if version_info[0:2] < (2, 7) or \
   version_info[0:2] in ( (3, 0), (3, 1) ):
    try:
        import unittest2 as unittest
    except ImportError:
        import unittest
else:
    import unittest

class BooleanDecoderTestCase(unittest.TestCase):
    def testTrue(self):
        assert decoder.decode(ints2octs((1, 1, 255))) == (1, null)
    def testFalse(self):
        assert decoder.decode(ints2octs((1, 1, 0))) == (0, null)
        
class OctetStringDecoderTestCase(unittest.TestCase):
    def testShortMode(self):
        assert decoder.decode(
            ints2octs((4, 15, 81, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 32, 102, 111, 120)),
            ) == (str2octs('Quick brown fox'), null)
    def testLongMode(self):
        assert decoder.decode(
            ints2octs((36, 128, 4, 130, 3, 232) + (81,)*1000 + (4, 1, 81, 0, 0))
            ) == (str2octs('Q'*1001), null)

if __name__ == '__main__': unittest.main()
