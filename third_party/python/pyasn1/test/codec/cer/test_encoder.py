from pyasn1.type import namedtype, univ
from pyasn1.codec.cer import encoder
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

class BooleanEncoderTestCase(unittest.TestCase):
    def testTrue(self):
        assert encoder.encode(univ.Boolean(1)) == ints2octs((1, 1, 255))
    def testFalse(self):
        assert encoder.encode(univ.Boolean(0)) == ints2octs((1, 1, 0))

class BitStringEncoderTestCase(unittest.TestCase):
    def testShortMode(self):
        assert encoder.encode(
            univ.BitString((1,0)*501)
            ) == ints2octs((3, 127, 6) + (170,) * 125 + (128,))

    def testLongMode(self):
        assert encoder.encode(
            univ.BitString((1,0)*501)
            ) == ints2octs((3, 127, 6) + (170,) * 125 + (128,))
        
class OctetStringEncoderTestCase(unittest.TestCase):
    def testShortMode(self):
        assert encoder.encode(
            univ.OctetString('Quick brown fox')
            ) == ints2octs((4, 15, 81, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 32, 102, 111, 120))
    def testLongMode(self):
        assert encoder.encode(
            univ.OctetString('Q'*1001)
            ) == ints2octs((36, 128, 4, 130, 3, 232) + (81,)*1000 + (4, 1, 81, 0, 0))
        
class SetEncoderTestCase(unittest.TestCase):
    def setUp(self):
        self.s = univ.Set(componentType=namedtype.NamedTypes(
            namedtype.NamedType('place-holder', univ.Null('')),
            namedtype.OptionalNamedType('first-name', univ.OctetString('')),
            namedtype.DefaultedNamedType('age', univ.Integer(33))
            ))

    def __init(self):
        self.s.clear()
        self.s.setComponentByPosition(0)
    def __initWithOptional(self):
        self.s.clear()
        self.s.setComponentByPosition(0)
        self.s.setComponentByPosition(1, 'quick brown')
        
    def __initWithDefaulted(self):
        self.s.clear()
        self.s.setComponentByPosition(0)
        self.s.setComponentByPosition(2, 1)
        
    def __initWithOptionalAndDefaulted(self):
        self.s.clear()
        self.s.setComponentByPosition(0, univ.Null(''))
        self.s.setComponentByPosition(1, univ.OctetString('quick brown'))
        self.s.setComponentByPosition(2, univ.Integer(1))
        
    def testIndefMode(self):
        self.__init()
        assert encoder.encode(self.s) == ints2octs((49, 128, 5, 0, 0, 0))

    def testWithOptionalIndefMode(self):
        self.__initWithOptional()
        assert encoder.encode(
            self.s
            ) == ints2octs((49, 128, 4, 11, 113, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 5, 0, 0, 0))

    def testWithDefaultedIndefMode(self):
        self.__initWithDefaulted()
        assert encoder.encode(
            self.s
            ) == ints2octs((49, 128, 2, 1, 1, 5, 0, 0, 0))

    def testWithOptionalAndDefaultedIndefMode(self):
        self.__initWithOptionalAndDefaulted()
        assert encoder.encode(
            self.s
            ) == ints2octs((49, 128, 2, 1, 1, 4, 11, 113, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 5, 0, 0, 0))

class SetWithChoiceEncoderTestCase(unittest.TestCase):
    def setUp(self):
        c = univ.Choice(componentType=namedtype.NamedTypes(
            namedtype.NamedType('actual', univ.Boolean(0))
            ))
        self.s = univ.Set(componentType=namedtype.NamedTypes(
            namedtype.NamedType('place-holder', univ.Null('')),
            namedtype.NamedType('status', c)
            ))

    def testIndefMode(self):
        self.s.setComponentByPosition(0)
        self.s.setComponentByName('status')
        self.s.getComponentByName('status').setComponentByPosition(0, 1)
        assert encoder.encode(self.s) == ints2octs((49, 128, 1, 1, 255, 5, 0, 0, 0))

if __name__ == '__main__': unittest.main()
