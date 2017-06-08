from pyasn1.type import univ
from pyasn1.codec.der import decoder
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

class OctetStringDecoderTestCase(unittest.TestCase):
    def testShortMode(self):
        assert decoder.decode(
            '\004\017Quick brown fox'.encode()
            ) == ('Quick brown fox'.encode(), ''.encode())

if __name__ == '__main__': unittest.main()
