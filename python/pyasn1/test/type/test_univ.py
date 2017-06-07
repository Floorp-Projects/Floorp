from pyasn1.type import univ, tag, constraint, namedtype, namedval, error
from pyasn1.compat.octets import str2octs, ints2octs
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

class IntegerTestCase(unittest.TestCase):
    def testStr(self): assert str(univ.Integer(1)) in ('1','1L'),'str() fails'
    def testAnd(self): assert univ.Integer(1) & 0 == 0, '__and__() fails'
    def testOr(self): assert univ.Integer(1) | 0 == 1, '__or__() fails'
    def testXor(self): assert univ.Integer(1) ^ 0 == 1, '__xor__() fails'
    def testRand(self): assert 0 & univ.Integer(1) == 0, '__rand__() fails'
    def testRor(self): assert 0 | univ.Integer(1) == 1, '__ror__() fails'
    def testRxor(self): assert 0 ^ univ.Integer(1) == 1, '__rxor__() fails'    
    def testAdd(self): assert univ.Integer(-4) + 6 == 2, '__add__() fails'
    def testRadd(self): assert 4 + univ.Integer(5) == 9, '__radd__() fails'
    def testSub(self): assert univ.Integer(3) - 6 == -3, '__sub__() fails'
    def testRsub(self): assert 6 - univ.Integer(3) == 3, '__rsub__() fails'
    def testMul(self): assert univ.Integer(3) * -3 == -9, '__mul__() fails'
    def testRmul(self): assert 2 * univ.Integer(3) == 6, '__rmul__() fails'
    def testDiv(self): assert univ.Integer(3) / 2 == 1, '__div__() fails'
    def testRdiv(self): assert 6 / univ.Integer(3) == 2, '__rdiv__() fails'
    def testMod(self): assert univ.Integer(3) % 2 == 1, '__mod__() fails'
    def testRmod(self): assert 4 % univ.Integer(3) == 1, '__rmod__() fails'
    def testPow(self): assert univ.Integer(3) ** 2 == 9, '__pow__() fails'
    def testRpow(self): assert 2 ** univ.Integer(2) == 4, '__rpow__() fails'
    def testLshift(self): assert univ.Integer(1) << 1 == 2, '<< fails'
    def testRshift(self): assert univ.Integer(2) >> 1 == 1, '>> fails'
    def testInt(self): assert int(univ.Integer(3)) == 3, '__int__() fails'
    def testLong(self): assert int(univ.Integer(8)) == 8, '__long__() fails'
    def testFloat(self): assert float(univ.Integer(4))==4.0,'__float__() fails'
    def testPrettyIn(self): assert univ.Integer('3') == 3, 'prettyIn() fails'
    def testTag(self):
        assert univ.Integer().getTagSet() == tag.TagSet(
            (),
            tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 0x02)
            )
    def testNamedVals(self):
        i = univ.Integer(
            'asn1', namedValues=univ.Integer.namedValues.clone(('asn1', 1))
            )
        assert i == 1, 'named val fails'
        assert str(i) != 'asn1', 'named val __str__() fails'

class BooleanTestCase(unittest.TestCase):
    def testTruth(self):
        assert univ.Boolean(True) and univ.Boolean(1), 'Truth initializer fails'
    def testFalse(self):
        assert not univ.Boolean(False) and not univ.Boolean(0), 'False initializer fails'
    def testStr(self):
        assert str(univ.Boolean(1)) in ('1', '1L'), 'str() fails'
    def testTag(self):
        assert univ.Boolean().getTagSet() == tag.TagSet(
            (),
            tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 0x01)
            )
    def testConstraints(self):
        try:
            univ.Boolean(2)
        except error.ValueConstraintError:
            pass
        else:
            assert 0, 'constraint fail'
    def testSubtype(self):
        assert univ.Integer().subtype(
            value=1,
            implicitTag=tag.Tag(tag.tagClassPrivate,tag.tagFormatSimple,2),
            subtypeSpec=constraint.SingleValueConstraint(1,3)
            ) == univ.Integer(
            value=1,
            tagSet=tag.TagSet(tag.Tag(tag.tagClassPrivate,
                                        tag.tagFormatSimple,2)),
            subtypeSpec=constraint.ConstraintsIntersection(constraint.SingleValueConstraint(1,3))
            )

class BitStringTestCase(unittest.TestCase):
    def setUp(self):
        self.b = univ.BitString(
            namedValues=namedval.NamedValues(('Active', 0), ('Urgent', 1))
            )
    def testSet(self):
        assert self.b.clone('Active') == (1,)
        assert self.b.clone("'1010100110001010'B") == (1,0,1,0,1,0,0,1,1,0,0,0,1,0,1,0)
        assert self.b.clone("'A98A'H") == (1,0,1,0,1,0,0,1,1,0,0,0,1,0,1,0)
        assert self.b.clone((1,0,1)) == (1,0,1)
    def testStr(self):
        assert str(self.b.clone('Urgent,Active')) == '(1, 1)'
    def testRepr(self):
        assert repr(self.b.clone('Urgent,Active')) == 'BitString("\'11\'B")'
    def testTag(self):
        assert univ.BitString().getTagSet() == tag.TagSet(
            (),
            tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 0x03)
            )
    def testLen(self): assert len(self.b.clone("'A98A'H")) == 16
    def testIter(self):
        assert self.b.clone("'A98A'H")[0] == 1
        assert self.b.clone("'A98A'H")[1] == 0
        assert self.b.clone("'A98A'H")[2] == 1
        
class OctetStringTestCase(unittest.TestCase):
    def testInit(self):
        assert univ.OctetString(str2octs('abcd')) == str2octs('abcd'), '__init__() fails'
    def testBinStr(self):
        assert univ.OctetString(binValue="1000010111101110101111000000111011") == ints2octs((133, 238, 188, 14, 192)), 'bin init fails'
    def testHexStr(self):
        assert univ.OctetString(hexValue="FA9823C43E43510DE3422") == ints2octs((250, 152, 35, 196, 62, 67, 81, 13, 227, 66, 32)), 'hex init fails'
    def testTuple(self):
        assert univ.OctetString((1,2,3,4,5)) == ints2octs((1,2,3,4,5)), 'tuple init failed'
    def testStr(self):
        assert str(univ.OctetString('q')) == 'q', '__str__() fails'
    def testSeq(self):
        assert univ.OctetString('q')[0] == str2octs('q')[0],'__getitem__() fails'
    def testAsOctets(self):
        assert univ.OctetString('abcd').asOctets() == str2octs('abcd'), 'testAsOctets() fails'
    def testAsInts(self):
        assert univ.OctetString('abcd').asNumbers() == (97, 98, 99, 100), 'testAsNumbers() fails'

    def testEmpty(self):
        try:
            str(univ.OctetString())
        except PyAsn1Error:
            pass
        else:
            assert 0, 'empty OctetString() not reported'
            
    def testAdd(self):
        assert univ.OctetString('') + 'q' == str2octs('q'), '__add__() fails'
    def testRadd(self):
        assert 'b' + univ.OctetString('q') == str2octs('bq'), '__radd__() fails'
    def testMul(self):
        assert univ.OctetString('a') * 2 == str2octs('aa'), '__mul__() fails'
    def testRmul(self):
        assert 2 * univ.OctetString('b') == str2octs('bb'), '__rmul__() fails'
    def testTag(self):
        assert univ.OctetString().getTagSet() == tag.TagSet(
            (),
            tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 0x04)
            )

class Null(unittest.TestCase):
    def testStr(self): assert str(univ.Null('')) == '', 'str() fails'
    def testTag(self):
        assert univ.Null().getTagSet() == tag.TagSet(
            (),
            tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 0x05)
            )
    def testConstraints(self):
        try:
            univ.Null(2)
        except error.ValueConstraintError:
            pass
        else:
            assert 0, 'constraint fail'

class RealTestCase(unittest.TestCase):
    def testStr(self): assert str(univ.Real(1.0)) == '1.0','str() fails'
    def testRepr(self): assert repr(univ.Real(-4.1)) == 'Real((-41, 10, -1))','repr() fails'
    def testAdd(self): assert univ.Real(-4.1) + 1.4 == -2.7, '__add__() fails'
    def testRadd(self): assert 4 + univ.Real(0.5) == 4.5, '__radd__() fails'
    def testSub(self): assert univ.Real(3.9) - 1.7 == 2.2, '__sub__() fails'
    def testRsub(self): assert 6.1 - univ.Real(0.1) == 6, '__rsub__() fails'
    def testMul(self): assert univ.Real(3.0) * -3 == -9, '__mul__() fails'
    def testRmul(self): assert 2 * univ.Real(3.0) == 6, '__rmul__() fails'
    def testDiv(self): assert univ.Real(3.0) / 2 == 1.5, '__div__() fails'
    def testRdiv(self): assert 6 / univ.Real(3.0) == 2, '__rdiv__() fails'
    def testMod(self): assert univ.Real(3.0) % 2 == 1, '__mod__() fails'
    def testRmod(self): assert 4 % univ.Real(3.0) == 1, '__rmod__() fails'
    def testPow(self): assert univ.Real(3.0) ** 2 == 9, '__pow__() fails'
    def testRpow(self): assert 2 ** univ.Real(2.0) == 4, '__rpow__() fails'
    def testInt(self): assert int(univ.Real(3.0)) == 3, '__int__() fails'
    def testLong(self): assert int(univ.Real(8.0)) == 8, '__long__() fails'
    def testFloat(self): assert float(univ.Real(4.0))==4.0,'__float__() fails'
    def testPrettyIn(self): assert univ.Real((3,10,0)) == 3, 'prettyIn() fails'
    # infinite float values
    def testStrInf(self):
        assert str(univ.Real('inf')) == 'inf','str() fails'
    def testReprInf(self):
        assert repr(univ.Real('inf')) == 'Real(\'inf\')','repr() fails'
    def testAddInf(self):
        assert univ.Real('inf') + 1 == float('inf'), '__add__() fails'
    def testRaddInf(self):
        assert 1 + univ.Real('inf') == float('inf'), '__radd__() fails'
    def testIntInf(self):
        try:
            assert int(univ.Real('inf'))
        except OverflowError:
            pass
        else:
            assert 0, '__int__() fails'
    def testLongInf(self):
        try:
            assert int(univ.Real('inf'))
        except OverflowError:
            pass
        else:
            assert 0, '__long__() fails'        
        assert int(univ.Real(8.0)) == 8, '__long__() fails'
    def testFloatInf(self):
        assert float(univ.Real('-inf')) == float('-inf'),'__float__() fails'
    def testPrettyInInf(self):
        assert univ.Real(float('inf')) == float('inf'), 'prettyIn() fails'
    def testPlusInf(self):
        assert univ.Real('inf').isPlusInfinity(), 'isPlusInfinity failed'
    def testMinusInf(self):
        assert univ.Real('-inf').isMinusInfinity(), 'isMinusInfinity failed'

    def testTag(self):
        assert univ.Real().getTagSet() == tag.TagSet(
            (),
            tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 0x09)
            )

class ObjectIdentifier(unittest.TestCase):
    def testStr(self):
        assert str(univ.ObjectIdentifier((1,3,6))) == '1.3.6'
    def testEq(self):
        assert univ.ObjectIdentifier((1,3,6)) == (1,3,6), '__cmp__() fails'
    def testAdd(self):
        assert univ.ObjectIdentifier((1,3)) + (6,)==(1,3,6),'__add__() fails'
    def testRadd(self):
        assert (1,) + univ.ObjectIdentifier((3,6))==(1,3,6),'__radd__() fails'
    def testLen(self):
        assert len(univ.ObjectIdentifier((1,3))) == 2,'__len__() fails'
    def testPrefix(self):
        o = univ.ObjectIdentifier('1.3.6')
        assert o.isPrefixOf((1,3,6)), 'isPrefixOf() fails'
        assert o.isPrefixOf((1,3,6,1)), 'isPrefixOf() fails'
        assert not o.isPrefixOf((1,3)), 'isPrefixOf() fails'        
    def testInput(self):
        assert univ.ObjectIdentifier('1.3.6')==(1,3,6),'prettyIn() fails'
    def testTag(self):
        assert univ.ObjectIdentifier().getTagSet() == tag.TagSet(
            (),
            tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 0x06)
            )

class SequenceOf(unittest.TestCase):
    def setUp(self):
        self.s1 = univ.SequenceOf(
            componentType=univ.OctetString('')
            )
        self.s2 = self.s1.clone()
    def testTag(self):
        assert self.s1.getTagSet() == tag.TagSet(
            (),
            tag.Tag(tag.tagClassUniversal, tag.tagFormatConstructed, 0x10)
            ), 'wrong tagSet'
    def testSeq(self):
        self.s1.setComponentByPosition(0, univ.OctetString('abc'))
        assert self.s1[0] == str2octs('abc'), 'set by idx fails'
        self.s1[0] = 'cba'
        assert self.s1[0] == str2octs('cba'), 'set by idx fails'
    def testCmp(self):
        self.s1.clear()
        self.s1.setComponentByPosition(0, 'abc')
        self.s2.clear()
        self.s2.setComponentByPosition(0, univ.OctetString('abc'))
        assert self.s1 == self.s2, '__cmp__() fails'
    def testSubtypeSpec(self):
        s = self.s1.clone(subtypeSpec=constraint.ConstraintsUnion(
            constraint.SingleValueConstraint(str2octs('abc'))
            ))
        try:
            s.setComponentByPosition(0, univ.OctetString('abc'))
        except:
            assert 0, 'constraint fails'
        try:
            s.setComponentByPosition(1, univ.OctetString('Abc'))
        except:
            pass
        else:
            assert 0, 'constraint fails'
    def testSizeSpec(self):
        s = self.s1.clone(sizeSpec=constraint.ConstraintsUnion(
            constraint.ValueSizeConstraint(1,1)
            ))
        s.setComponentByPosition(0, univ.OctetString('abc'))
        try:
            s.verifySizeSpec()
        except:
            assert 0, 'size spec fails'
        s.setComponentByPosition(1, univ.OctetString('abc'))
        try:
            s.verifySizeSpec()
        except:
            pass
        else:
            assert 0, 'size spec fails'
    def testGetComponentTagMap(self):
        assert self.s1.getComponentTagMap().getPosMap() == {
            univ.OctetString.tagSet: univ.OctetString('')
            }
    def testSubtype(self):
        self.s1.clear()
        assert self.s1.subtype(
            implicitTag=tag.Tag(tag.tagClassPrivate,tag.tagFormatSimple,2),
            subtypeSpec=constraint.SingleValueConstraint(1,3),
            sizeSpec=constraint.ValueSizeConstraint(0,1)
            ) == self.s1.clone(
            tagSet=tag.TagSet(tag.Tag(tag.tagClassPrivate,
                                        tag.tagFormatSimple,2)),
            subtypeSpec=constraint.ConstraintsIntersection(constraint.SingleValueConstraint(1,3)),
            sizeSpec=constraint.ValueSizeConstraint(0,1)
            )
    def testClone(self):
        self.s1.setComponentByPosition(0, univ.OctetString('abc'))
        s = self.s1.clone()
        assert len(s) == 0
        s = self.s1.clone(cloneValueFlag=1)
        assert len(s) == 1
        assert s.getComponentByPosition(0) == self.s1.getComponentByPosition(0)
        
class Sequence(unittest.TestCase):
    def setUp(self):
        self.s1 = univ.Sequence(componentType=namedtype.NamedTypes(
            namedtype.NamedType('name', univ.OctetString('')),
            namedtype.OptionalNamedType('nick', univ.OctetString('')),
            namedtype.DefaultedNamedType('age', univ.Integer(34))
            ))
    def testTag(self):
        assert self.s1.getTagSet() == tag.TagSet(
            (),
            tag.Tag(tag.tagClassUniversal, tag.tagFormatConstructed, 0x10)
            ), 'wrong tagSet'
    def testById(self):
        self.s1.setComponentByName('name', univ.OctetString('abc'))
        assert self.s1.getComponentByName('name') == str2octs('abc'), 'set by name fails'
    def testByKey(self):
        self.s1['name'] = 'abc'
        assert self.s1['name'] == str2octs('abc'), 'set by key fails'
    def testGetNearPosition(self):
        assert self.s1.getComponentTagMapNearPosition(1).getPosMap() == {
            univ.OctetString.tagSet: univ.OctetString(''),
            univ.Integer.tagSet: univ.Integer(34)
            }
        assert self.s1.getComponentPositionNearType(
            univ.OctetString.tagSet, 1
            ) == 1
    def testGetDefaultComponentByPosition(self):
        self.s1.clear()
        assert self.s1.getDefaultComponentByPosition(0) == None
        assert self.s1.getDefaultComponentByPosition(2) == univ.Integer(34)
    def testSetDefaultComponents(self):
        self.s1.clear()
        assert self.s1.getComponentByPosition(2) == None
        self.s1.setComponentByPosition(0, univ.OctetString('Ping'))
        self.s1.setComponentByPosition(1, univ.OctetString('Pong'))
        self.s1.setDefaultComponents()
        assert self.s1.getComponentByPosition(2) == 34
    def testClone(self):
        self.s1.setComponentByPosition(0, univ.OctetString('abc'))
        self.s1.setComponentByPosition(1, univ.OctetString('def'))
        self.s1.setComponentByPosition(2, univ.Integer(123))
        s = self.s1.clone()
        assert s.getComponentByPosition(0) != self.s1.getComponentByPosition(0)
        assert s.getComponentByPosition(1) != self.s1.getComponentByPosition(1)
        assert s.getComponentByPosition(2) != self.s1.getComponentByPosition(2)
        s = self.s1.clone(cloneValueFlag=1)
        assert s.getComponentByPosition(0) == self.s1.getComponentByPosition(0)
        assert s.getComponentByPosition(1) == self.s1.getComponentByPosition(1)
        assert s.getComponentByPosition(2) == self.s1.getComponentByPosition(2)

class SetOf(unittest.TestCase):
    def setUp(self):
        self.s1 = univ.SetOf(componentType=univ.OctetString(''))
    def testTag(self):
        assert self.s1.getTagSet() == tag.TagSet(
            (),
            tag.Tag(tag.tagClassUniversal, tag.tagFormatConstructed, 0x11)
            ), 'wrong tagSet'
    def testSeq(self):
        self.s1.setComponentByPosition(0, univ.OctetString('abc'))
        assert self.s1[0] == str2octs('abc'), 'set by idx fails'
        self.s1.setComponentByPosition(0, self.s1[0].clone('cba'))
        assert self.s1[0] == str2octs('cba'), 'set by idx fails'

class Set(unittest.TestCase):
    def setUp(self):
        self.s1 = univ.Set(componentType=namedtype.NamedTypes(
            namedtype.NamedType('name', univ.OctetString('')),
            namedtype.OptionalNamedType('null', univ.Null('')),
            namedtype.DefaultedNamedType('age', univ.Integer(34))
            ))
        self.s2 = self.s1.clone()
    def testTag(self):
        assert self.s1.getTagSet() == tag.TagSet(
            (),
            tag.Tag(tag.tagClassUniversal, tag.tagFormatConstructed, 0x11)
            ), 'wrong tagSet'
    def testByTypeWithPythonValue(self):
        self.s1.setComponentByType(univ.OctetString.tagSet, 'abc')
        assert self.s1.getComponentByType(
            univ.OctetString.tagSet
            ) == str2octs('abc'), 'set by name fails'
    def testByTypeWithInstance(self):
        self.s1.setComponentByType(univ.OctetString.tagSet, univ.OctetString('abc'))
        assert self.s1.getComponentByType(
            univ.OctetString.tagSet
            ) == str2octs('abc'), 'set by name fails'
    def testGetTagMap(self):
        assert self.s1.getTagMap().getPosMap() == {
            univ.Set.tagSet: univ.Set()
            }
    def testGetComponentTagMap(self):
        assert self.s1.getComponentTagMap().getPosMap() == {
            univ.OctetString.tagSet: univ.OctetString(''),
            univ.Null.tagSet: univ.Null(''),
            univ.Integer.tagSet: univ.Integer(34)
            }
    def testGetPositionByType(self):
        assert self.s1.getComponentPositionByType(
            univ.Null().getTagSet()
            ) == 1

class Choice(unittest.TestCase):
    def setUp(self):
        innerComp = univ.Choice(componentType=namedtype.NamedTypes(
            namedtype.NamedType('count', univ.Integer()),
            namedtype.NamedType('flag', univ.Boolean())
            ))
        self.s1 = univ.Choice(componentType=namedtype.NamedTypes(
            namedtype.NamedType('name', univ.OctetString()),
            namedtype.NamedType('sex', innerComp)
            ))
    def testTag(self):
        assert self.s1.getTagSet() == tag.TagSet(), 'wrong tagSet'
    def testOuterByTypeWithPythonValue(self):
        self.s1.setComponentByType(univ.OctetString.tagSet, 'abc')
        assert self.s1.getComponentByType(
            univ.OctetString.tagSet
            ) == str2octs('abc')
    def testOuterByTypeWithInstanceValue(self):
        self.s1.setComponentByType(
            univ.OctetString.tagSet, univ.OctetString('abc')
            )
        assert self.s1.getComponentByType(
            univ.OctetString.tagSet
            ) == str2octs('abc')
    def testInnerByTypeWithPythonValue(self):
        self.s1.setComponentByType(univ.Integer.tagSet, 123, 1)
        assert self.s1.getComponentByType(
            univ.Integer.tagSet, 1
            ) == 123
    def testInnerByTypeWithInstanceValue(self):
        self.s1.setComponentByType(
            univ.Integer.tagSet, univ.Integer(123), 1
            )
        assert self.s1.getComponentByType(
            univ.Integer.tagSet, 1
            ) == 123
    def testCmp(self):
        self.s1.setComponentByName('name', univ.OctetString('abc'))
        assert self.s1 == str2octs('abc'), '__cmp__() fails'
    def testGetComponent(self):
        self.s1.setComponentByType(univ.OctetString.tagSet, 'abc')
        assert self.s1.getComponent() == str2octs('abc'), 'getComponent() fails'
    def testGetName(self):
        self.s1.setComponentByType(univ.OctetString.tagSet, 'abc')
        assert self.s1.getName() == 'name', 'getName() fails'
    def testSetComponentByPosition(self):
        self.s1.setComponentByPosition(0, univ.OctetString('Jim'))
        assert self.s1 == str2octs('Jim')
    def testClone(self):
        self.s1.setComponentByPosition(0, univ.OctetString('abc'))
        s = self.s1.clone()
        assert len(s) == 0
        s = self.s1.clone(cloneValueFlag=1)
        assert len(s) == 1
        assert s.getComponentByPosition(0) == self.s1.getComponentByPosition(0)
        
if __name__ == '__main__': unittest.main()
