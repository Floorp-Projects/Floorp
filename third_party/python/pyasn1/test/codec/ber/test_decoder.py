from pyasn1.type import tag, namedtype, univ
from pyasn1.codec.ber import decoder
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

class LargeTagDecoderTestCase(unittest.TestCase):
    def testLargeTag(self):
        assert decoder.decode(ints2octs((127, 141, 245, 182, 253, 47, 3, 2, 1, 1))) == (1, null)

class IntegerDecoderTestCase(unittest.TestCase):
    def testPosInt(self):
        assert decoder.decode(ints2octs((2, 1, 12))) == (12, null)
    def testNegInt(self):
        assert decoder.decode(ints2octs((2, 1, 244))) == (-12, null)
    def testZero(self):
        assert decoder.decode(ints2octs((2, 0))) == (0, null)
    def testZeroLong(self):
        assert decoder.decode(ints2octs((2, 1, 0))) == (0, null)
    def testMinusOne(self):
        assert decoder.decode(ints2octs((2, 1, 255))) == (-1, null)
    def testPosLong(self):
        assert decoder.decode(
            ints2octs((2, 9, 0, 255, 255, 255, 255, 255, 255, 255, 255))
            ) == (0xffffffffffffffff, null)
    def testNegLong(self):
        assert decoder.decode(
            ints2octs((2, 9, 255, 0, 0, 0, 0, 0, 0, 0, 1))
            ) == (-0xffffffffffffffff, null)
    def testSpec(self):
        try:
            decoder.decode(
                ints2octs((2, 1, 12)), asn1Spec=univ.Null()
                ) == (12, null)
        except PyAsn1Error:
            pass
        else:
            assert 0, 'wrong asn1Spec worked out'
        assert decoder.decode(
            ints2octs((2, 1, 12)), asn1Spec=univ.Integer()
            ) == (12, null)
    def testTagFormat(self):
        try:
            decoder.decode(ints2octs((34, 1, 12)))
        except PyAsn1Error:
            pass
        else:
            assert 0, 'wrong tagFormat worked out'

class BooleanDecoderTestCase(unittest.TestCase):
    def testTrue(self):
        assert decoder.decode(ints2octs((1, 1, 1))) == (1, null)
    def testTrueNeg(self):
        assert decoder.decode(ints2octs((1, 1, 255))) == (1, null)
    def testExtraTrue(self):
        assert decoder.decode(ints2octs((1, 1, 1, 0, 120, 50, 50))) == (1, ints2octs((0, 120, 50, 50)))
    def testFalse(self):
        assert decoder.decode(ints2octs((1, 1, 0))) == (0, null)
    def testTagFormat(self):
        try:
            decoder.decode(ints2octs((33, 1, 1)))
        except PyAsn1Error:
            pass
        else:
            assert 0, 'wrong tagFormat worked out'

class BitStringDecoderTestCase(unittest.TestCase):
    def testDefMode(self):
        assert decoder.decode(
            ints2octs((3, 3, 1, 169, 138))
            ) == ((1,0,1,0,1,0,0,1,1,0,0,0,1,0,1), null)
    def testIndefMode(self):
        assert decoder.decode(
            ints2octs((3, 3, 1, 169, 138))
            ) == ((1,0,1,0,1,0,0,1,1,0,0,0,1,0,1), null)
    def testDefModeChunked(self):
        assert decoder.decode(
            ints2octs((35, 8, 3, 2, 0, 169, 3, 2, 1, 138))
            ) == ((1,0,1,0,1,0,0,1,1,0,0,0,1,0,1), null)
    def testIndefModeChunked(self):
        assert decoder.decode(
            ints2octs((35, 128, 3, 2, 0, 169, 3, 2, 1, 138, 0, 0))
            ) == ((1,0,1,0,1,0,0,1,1,0,0,0,1,0,1), null)
    def testDefModeChunkedSubst(self):
        assert decoder.decode(
            ints2octs((35, 8, 3, 2, 0, 169, 3, 2, 1, 138)),
            substrateFun=lambda a,b,c: (b,c)
            ) == (ints2octs((3, 2, 0, 169, 3, 2, 1, 138)), 8)
    def testIndefModeChunkedSubst(self):
        assert decoder.decode(
            ints2octs((35, 128, 3, 2, 0, 169, 3, 2, 1, 138, 0, 0)),
            substrateFun=lambda a,b,c: (b,c)
            ) == (ints2octs((3, 2, 0, 169, 3, 2, 1, 138, 0, 0)), -1)
        
class OctetStringDecoderTestCase(unittest.TestCase):
    def testDefMode(self):
        assert decoder.decode(
            ints2octs((4, 15, 81, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 32, 102, 111, 120))
            ) == (str2octs('Quick brown fox'), null)
    def testIndefMode(self):
        assert decoder.decode(
            ints2octs((36, 128, 4, 15, 81, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 32, 102, 111, 120, 0, 0))
            ) == (str2octs('Quick brown fox'), null)
    def testDefModeChunked(self):
        assert decoder.decode(
            ints2octs((36, 23, 4, 4, 81, 117, 105, 99, 4, 4, 107, 32, 98, 114, 4, 4, 111, 119, 110, 32, 4, 3, 102, 111, 120))
            ) == (str2octs('Quick brown fox'), null)
    def testIndefModeChunked(self):
        assert decoder.decode(
            ints2octs((36, 128, 4, 4, 81, 117, 105, 99, 4, 4, 107, 32, 98, 114, 4, 4, 111, 119, 110, 32, 4, 3, 102, 111, 120, 0, 0))
            ) == (str2octs('Quick brown fox'), null)
    def testDefModeChunkedSubst(self):
        assert decoder.decode(
            ints2octs((36, 23, 4, 4, 81, 117, 105, 99, 4, 4, 107, 32, 98, 114, 4, 4, 111, 119, 110, 32, 4, 3, 102, 111, 120)),
            substrateFun=lambda a,b,c: (b,c)
            ) == (ints2octs((4, 4, 81, 117, 105, 99, 4, 4, 107, 32, 98, 114, 4, 4, 111, 119, 110, 32, 4, 3, 102, 111, 120)), 23)
    def testIndefModeChunkedSubst(self):
        assert decoder.decode(
            ints2octs((36, 128, 4, 4, 81, 117, 105, 99, 4, 4, 107, 32, 98, 114, 4, 4, 111, 119, 110, 32, 4, 3, 102, 111, 120, 0, 0)),
            substrateFun=lambda a,b,c: (b,c)
            ) == (ints2octs((4, 4, 81, 117, 105, 99, 4, 4, 107, 32, 98, 114, 4, 4, 111, 119, 110, 32, 4, 3, 102, 111, 120, 0, 0)), -1)
        
class ExpTaggedOctetStringDecoderTestCase(unittest.TestCase):
    def setUp(self):
        self.o = univ.OctetString(
            'Quick brown fox',
            tagSet=univ.OctetString.tagSet.tagExplicitly(
            tag.Tag(tag.tagClassApplication, tag.tagFormatSimple, 5)
            ))

    def testDefMode(self):
        assert self.o.isSameTypeWith(decoder.decode(
            ints2octs((101, 17, 4, 15, 81, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 32, 102, 111, 120))
            )[0])

    def testIndefMode(self):
        v, s = decoder.decode(ints2octs((101, 128, 36, 128, 4, 15, 81, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 32, 102, 111, 120, 0, 0, 0, 0)))
        assert self.o.isSameTypeWith(v)
        assert not s

    def testDefModeChunked(self):
        v, s = decoder.decode(ints2octs((101, 25, 36, 23, 4, 4, 81, 117, 105, 99, 4, 4, 107, 32, 98, 114, 4, 4, 111, 119, 110, 32, 4, 3, 102, 111, 120)))
        assert self.o.isSameTypeWith(v)
        assert not s

    def testIndefModeChunked(self):
        v, s = decoder.decode(ints2octs((101, 128, 36, 128, 4, 4, 81, 117, 105, 99, 4, 4, 107, 32, 98, 114, 4, 4, 111, 119, 110, 32, 4, 3, 102, 111, 120, 0, 0, 0, 0)))
        assert self.o.isSameTypeWith(v)
        assert not s

    def testDefModeSubst(self):
        assert decoder.decode(
            ints2octs((101, 17, 4, 15, 81, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 32, 102, 111, 120)),
            substrateFun=lambda a,b,c: (b,c)
            ) == (ints2octs((4, 15, 81, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 32, 102, 111, 120)), 17)

    def testIndefModeSubst(self):
        assert decoder.decode(
            ints2octs((101, 128, 36, 128, 4, 15, 81, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 32, 102, 111, 120, 0, 0, 0, 0)),
            substrateFun=lambda a,b,c: (b,c)
            ) == (ints2octs((36, 128, 4, 15, 81, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 32, 102, 111, 120, 0, 0, 0, 0)), -1)
  
class NullDecoderTestCase(unittest.TestCase):
    def testNull(self):
        assert decoder.decode(ints2octs((5, 0))) == (null, null)
    def testTagFormat(self):
        try:
            decoder.decode(ints2octs((37, 0)))
        except PyAsn1Error:
            pass
        else:
            assert 0, 'wrong tagFormat worked out'

class ObjectIdentifierDecoderTestCase(unittest.TestCase):
    def testOID(self):
        assert decoder.decode(
            ints2octs((6, 6, 43, 6, 0, 191, 255, 126))
            ) == ((1,3,6,0,0xffffe), null)

    def testEdges1(self):
        assert decoder.decode(
            ints2octs((6, 1, 255))
            ) == ((6,15), null)

    def testEdges2(self):
        assert decoder.decode(
            ints2octs((6, 1, 239))
            ) == ((5,39), null)

    def testEdges3(self):
        assert decoder.decode(
            ints2octs((6, 7, 43, 6, 143, 255, 255, 255, 127))
            ) == ((1, 3, 6, 4294967295), null)

    def testNonLeading0x80(self):
        assert decoder.decode(
            ints2octs((6, 5, 85, 4, 129, 128, 0)),
            ) == ((2, 5, 4, 16384), null)

    def testLeading0x80(self):
        try:
            decoder.decode(
                ints2octs((6, 5, 85, 4, 128, 129, 0))
            )
        except PyAsn1Error:
            pass
        else:
            assert 1, 'Leading 0x80 tolarated'

    def testTagFormat(self):
        try:
            decoder.decode(ints2octs((38, 1, 239)))
        except PyAsn1Error:
            pass
        else:
            assert 0, 'wrong tagFormat worked out'

class RealDecoderTestCase(unittest.TestCase):
    def testChar(self):
        assert decoder.decode(
            ints2octs((9, 7, 3, 49, 50, 51, 69, 49, 49))
        ) == (univ.Real((123, 10, 11)), null)

    def testBin1(self):
        assert decoder.decode(
            ints2octs((9, 4, 128, 245, 4, 77))
        ) == (univ.Real((1101, 2, -11)), null)

    def testBin2(self):
        assert decoder.decode(
            ints2octs((9, 4, 128, 11, 4, 77))
        ) == (univ.Real((1101, 2, 11)), null)

    def testBin3(self):
        assert decoder.decode(
            ints2octs((9, 3, 192, 10, 123))
        ) == (univ.Real((-123, 2, 10)), null)


    def testPlusInf(self):
        assert decoder.decode(
            ints2octs((9, 1, 64))
        ) == (univ.Real('inf'), null)
 
    def testMinusInf(self):
        assert decoder.decode(
            ints2octs((9, 1, 65))
        ) == (univ.Real('-inf'), null)

    def testEmpty(self):
        assert decoder.decode(
            ints2octs((9, 0))
        ) == (univ.Real(0.0), null)
 
    def testTagFormat(self):
        try:
            decoder.decode(ints2octs((41, 0)))
        except PyAsn1Error:
            pass
        else:
            assert 0, 'wrong tagFormat worked out'

class SequenceDecoderTestCase(unittest.TestCase):
    def setUp(self):
        self.s = univ.Sequence(componentType=namedtype.NamedTypes(
            namedtype.NamedType('place-holder', univ.Null(null)),
            namedtype.NamedType('first-name', univ.OctetString(null)),
            namedtype.NamedType('age', univ.Integer(33)),
            ))
        self.s.setComponentByPosition(0, univ.Null(null))
        self.s.setComponentByPosition(1, univ.OctetString('quick brown'))
        self.s.setComponentByPosition(2, univ.Integer(1))
        self.s.setDefaultComponents()
        
    def testWithOptionalAndDefaultedDefMode(self):
        assert decoder.decode(
            ints2octs((48, 18, 5, 0, 4, 11, 113, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 2, 1, 1))
            ) == (self.s, null)
        
    def testWithOptionalAndDefaultedIndefMode(self):
        assert decoder.decode(
            ints2octs((48, 128, 5, 0, 36, 128, 4, 11, 113, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 0, 0, 2, 1, 1, 0, 0))
            ) == (self.s, null)

    def testWithOptionalAndDefaultedDefModeChunked(self):
        assert decoder.decode(
            ints2octs((48, 24, 5, 0, 36, 17, 4, 4, 113, 117, 105, 99, 4, 4, 107, 32, 98, 114, 4, 3, 111, 119, 110, 2, 1, 1))
            ) == (self.s, null)

    def testWithOptionalAndDefaultedIndefModeChunked(self):
        assert decoder.decode(
            ints2octs((48, 128, 5, 0, 36, 128, 4, 4, 113, 117, 105, 99, 4, 4, 107, 32, 98, 114, 4, 3, 111, 119, 110, 0, 0, 2, 1, 1, 0, 0))
            ) == (self.s, null)

    def testWithOptionalAndDefaultedDefModeSubst(self):
        assert decoder.decode(
            ints2octs((48, 18, 5, 0, 4, 11, 113, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 2, 1, 1)),
            substrateFun=lambda a,b,c: (b,c)
            ) == (ints2octs((5, 0, 4, 11, 113, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 2, 1, 1)), 18)
        
    def testWithOptionalAndDefaultedIndefModeSubst(self):
        assert decoder.decode(
            ints2octs((48, 128, 5, 0, 36, 128, 4, 11, 113, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 0, 0, 2, 1, 1, 0, 0)),
            substrateFun=lambda a,b,c: (b,c)
            ) == (ints2octs((5, 0, 36, 128, 4, 11, 113, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 0, 0, 2, 1, 1, 0, 0)), -1)

    def testTagFormat(self):
        try:
            decoder.decode(
                ints2octs((16, 18, 5, 0, 4, 11, 113, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 2, 1, 1))
            )
        except PyAsn1Error:
            pass
        else:
            assert 0, 'wrong tagFormat worked out'

class GuidedSequenceDecoderTestCase(unittest.TestCase):
    def setUp(self):
        self.s = univ.Sequence(componentType=namedtype.NamedTypes(
            namedtype.NamedType('place-holder', univ.Null(null)),
            namedtype.OptionalNamedType('first-name', univ.OctetString(null)),
            namedtype.DefaultedNamedType('age', univ.Integer(33)),
            ))

    def __init(self):
        self.s.clear()
        self.s.setComponentByPosition(0, univ.Null(null))
        self.s.setDefaultComponents()
        
    def __initWithOptional(self):
        self.s.clear()
        self.s.setComponentByPosition(0, univ.Null(null))
        self.s.setComponentByPosition(1, univ.OctetString('quick brown'))
        self.s.setDefaultComponents()

    def __initWithDefaulted(self):
        self.s.clear()
        self.s.setComponentByPosition(0, univ.Null(null))
        self.s.setComponentByPosition(2, univ.Integer(1))
        self.s.setDefaultComponents()
        
    def __initWithOptionalAndDefaulted(self):
        self.s.clear()
        self.s.setComponentByPosition(0, univ.Null(null))
        self.s.setComponentByPosition(1, univ.OctetString('quick brown'))
        self.s.setComponentByPosition(2, univ.Integer(1))
        self.s.setDefaultComponents()
        
    def testDefMode(self):
        self.__init()
        assert decoder.decode(
            ints2octs((48, 128, 5, 0, 0, 0)), asn1Spec=self.s
            ) == (self.s, null)
        
    def testIndefMode(self):
        self.__init()
        assert decoder.decode(
            ints2octs((48, 128, 5, 0, 0, 0)), asn1Spec=self.s
            ) == (self.s, null)

    def testDefModeChunked(self):
        self.__init()
        assert decoder.decode(
            ints2octs((48, 2, 5, 0)), asn1Spec=self.s
            ) == (self.s, null)

    def testIndefModeChunked(self):
        self.__init()
        assert decoder.decode(
            ints2octs((48, 128, 5, 0, 0, 0)), asn1Spec=self.s
            ) == (self.s, null)

    def testWithOptionalDefMode(self):
        self.__initWithOptional()
        assert decoder.decode(
            ints2octs((48, 15, 5, 0, 4, 11, 113, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110)), asn1Spec=self.s
            ) == (self.s, null)
        
    def testWithOptionaIndefMode(self):
        self.__initWithOptional()
        assert decoder.decode(
            ints2octs((48, 128, 5, 0, 36, 128, 4, 11, 113, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 0, 0, 0, 0)),
            asn1Spec=self.s
            ) == (self.s, null)

    def testWithOptionalDefModeChunked(self):
        self.__initWithOptional()
        assert decoder.decode(
            ints2octs((48, 21, 5, 0, 36, 17, 4, 4, 113, 117, 105, 99, 4, 4, 107, 32, 98, 114, 4, 3, 111, 119, 110)),
            asn1Spec=self.s
            ) == (self.s, null)

    def testWithOptionalIndefModeChunked(self):
        self.__initWithOptional()
        assert decoder.decode(
            ints2octs((48, 128, 5, 0, 36, 128, 4, 4, 113, 117, 105, 99, 4, 4, 107, 32, 98, 114, 4, 3, 111, 119, 110, 0, 0, 0, 0)),
            asn1Spec=self.s
            ) == (self.s, null)

    def testWithDefaultedDefMode(self):
        self.__initWithDefaulted()
        assert decoder.decode(
            ints2octs((48, 5, 5, 0, 2, 1, 1)), asn1Spec=self.s
            ) == (self.s, null)
        
    def testWithDefaultedIndefMode(self):
        self.__initWithDefaulted()
        assert decoder.decode(
            ints2octs((48, 128, 5, 0, 2, 1, 1, 0, 0)), asn1Spec=self.s
            ) == (self.s, null)

    def testWithDefaultedDefModeChunked(self):
        self.__initWithDefaulted()
        assert decoder.decode(
            ints2octs((48, 5, 5, 0, 2, 1, 1)), asn1Spec=self.s
            ) == (self.s, null)

    def testWithDefaultedIndefModeChunked(self):
        self.__initWithDefaulted()
        assert decoder.decode(
            ints2octs((48, 128, 5, 0, 2, 1, 1, 0, 0)), asn1Spec=self.s
            ) == (self.s, null)

    def testWithOptionalAndDefaultedDefMode(self):
        self.__initWithOptionalAndDefaulted()
        assert decoder.decode(
            ints2octs((48, 18, 5, 0, 4, 11, 113, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 2, 1, 1)), asn1Spec=self.s
            ) == (self.s, null)
        
    def testWithOptionalAndDefaultedIndefMode(self):
        self.__initWithOptionalAndDefaulted()
        assert decoder.decode(
            ints2octs((48, 128, 5, 0, 36, 128, 4, 11, 113, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 0, 0, 2, 1, 1, 0, 0)), asn1Spec=self.s
            ) == (self.s, null)

    def testWithOptionalAndDefaultedDefModeChunked(self):
        self.__initWithOptionalAndDefaulted()
        assert decoder.decode(
            ints2octs((48, 24, 5, 0, 36, 17, 4, 4, 113, 117, 105, 99, 4, 4, 107, 32, 98, 114, 4, 3, 111, 119, 110, 2, 1, 1)), asn1Spec=self.s
            ) == (self.s, null)

    def testWithOptionalAndDefaultedIndefModeChunked(self):
        self.__initWithOptionalAndDefaulted()
        assert decoder.decode(
            ints2octs((48, 128, 5, 0, 36, 128, 4, 4, 113, 117, 105, 99, 4, 4, 107, 32, 98, 114, 4, 3, 111, 119, 110, 0, 0, 2, 1, 1, 0, 0)), asn1Spec=self.s
            ) == (self.s, null)

class ChoiceDecoderTestCase(unittest.TestCase):
    def setUp(self):
        self.s = univ.Choice(componentType=namedtype.NamedTypes(
            namedtype.NamedType('place-holder', univ.Null(null)),
            namedtype.NamedType('number', univ.Integer(0)),
            namedtype.NamedType('string', univ.OctetString())
            ))

    def testBySpec(self):
        self.s.setComponentByPosition(0, univ.Null(null))
        assert decoder.decode(
            ints2octs((5, 0)), asn1Spec=self.s
            ) == (self.s, null)

    def testWithoutSpec(self):
        self.s.setComponentByPosition(0, univ.Null(null))
        assert decoder.decode(ints2octs((5, 0))) == (self.s, null)
        assert decoder.decode(ints2octs((5, 0))) == (univ.Null(null), null)

    def testUndefLength(self):
        self.s.setComponentByPosition(2, univ.OctetString('abcdefgh'))
        assert decoder.decode(ints2octs((36, 128, 4, 3, 97, 98, 99, 4, 3, 100, 101, 102, 4, 2, 103, 104, 0, 0)), asn1Spec=self.s) == (self.s, null)

    def testExplicitTag(self):
        s = self.s.subtype(explicitTag=tag.Tag(tag.tagClassContext,
                           tag.tagFormatConstructed, 4))
        s.setComponentByPosition(0, univ.Null(null))
        assert decoder.decode(ints2octs((164, 2, 5, 0)), asn1Spec=s) == (s, null)

    def testExplicitTagUndefLength(self):
        s = self.s.subtype(explicitTag=tag.Tag(tag.tagClassContext,
                           tag.tagFormatConstructed, 4))
        s.setComponentByPosition(0, univ.Null(null))
        assert decoder.decode(ints2octs((164, 128, 5, 0, 0, 0)), asn1Spec=s) == (s, null)

class AnyDecoderTestCase(unittest.TestCase):
    def setUp(self):
        self.s = univ.Any()

    def testByUntagged(self):
        assert decoder.decode(
            ints2octs((4, 3, 102, 111, 120)), asn1Spec=self.s
            ) == (univ.Any('\004\003fox'), null)

    def testTaggedEx(self):
        s = univ.Any('\004\003fox').subtype(explicitTag=tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 4))
        assert decoder.decode(ints2octs((164, 5, 4, 3, 102, 111, 120)), asn1Spec=s) == (s, null)
                
    def testTaggedIm(self):
        s = univ.Any('\004\003fox').subtype(implicitTag=tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 4))
        assert decoder.decode(ints2octs((132, 5, 4, 3, 102, 111, 120)), asn1Spec=s) == (s, null)

    def testByUntaggedIndefMode(self):
        assert decoder.decode(
            ints2octs((4, 3, 102, 111, 120)), asn1Spec=self.s
            ) == (univ.Any('\004\003fox'), null)

    def testTaggedExIndefMode(self):
        s = univ.Any('\004\003fox').subtype(explicitTag=tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 4))
        assert decoder.decode(ints2octs((164, 128, 4, 3, 102, 111, 120, 0, 0)), asn1Spec=s) == (s, null)
                
    def testTaggedImIndefMode(self):
        s = univ.Any('\004\003fox').subtype(implicitTag=tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 4))
        assert decoder.decode(ints2octs((164, 128, 4, 3, 102, 111, 120, 0, 0)), asn1Spec=s) == (s, null)

    def testByUntaggedSubst(self):
        assert decoder.decode(
            ints2octs((4, 3, 102, 111, 120)),
            asn1Spec=self.s,
            substrateFun=lambda a,b,c: (b,c)
            ) == (ints2octs((4, 3, 102, 111, 120)), 5)

    def testTaggedExSubst(self):
        assert decoder.decode(
            ints2octs((164, 5, 4, 3, 102, 111, 120)),
            asn1Spec=self.s,
            substrateFun=lambda a,b,c: (b,c)
        ) == (ints2octs((164, 5, 4, 3, 102, 111, 120)), 7)

if __name__ == '__main__': unittest.main()
