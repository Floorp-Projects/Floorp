from pyasn1.type import namedtype, univ
from pyasn1.codec.der import encoder
from pyasn1.compat.octets import ints2octs
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

class OctetStringEncoderTestCase(unittest.TestCase):
    def testShortMode(self):
        assert encoder.encode(
            univ.OctetString('Quick brown fox')
            ) == ints2octs((4, 15, 81, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 32, 102, 111, 120))

class BitStringEncoderTestCase(unittest.TestCase):
    def testShortMode(self):
        assert encoder.encode(
            univ.BitString((1,))
            ) == ints2octs((3, 2, 7, 128))
        
class SetWithChoiceEncoderTestCase(unittest.TestCase):
    def setUp(self):
        c = univ.Choice(componentType=namedtype.NamedTypes(
            namedtype.NamedType('name', univ.OctetString('')),
            namedtype.NamedType('amount', univ.Integer(0))
            ))
        self.s = univ.Set(componentType=namedtype.NamedTypes(
            namedtype.NamedType('place-holder', univ.Null('')),
            namedtype.NamedType('status', c)
            ))

    def testDefMode(self):
        self.s.setComponentByPosition(0)
        self.s.setComponentByName('status')
        self.s.getComponentByName('status').setComponentByPosition(0, 'ann')
        assert encoder.encode(self.s) == ints2octs((49, 7, 4, 3, 97, 110, 110, 5, 0))

if __name__ == '__main__': unittest.main()
