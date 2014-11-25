from pyasn1.type import namedtype, univ
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

class NamedTypeCaseBase(unittest.TestCase):
    def setUp(self):
        self.e = namedtype.NamedType('age', univ.Integer())
    def testIter(self):
        n, t = self.e
        assert n == 'age' or t == univ.Integer(), 'unpack fails'

class NamedTypesCaseBase(unittest.TestCase):
    def setUp(self):
        self.e = namedtype.NamedTypes(
            namedtype.NamedType('first-name', univ.OctetString('')),
            namedtype.OptionalNamedType('age', univ.Integer(0)),
            namedtype.NamedType('family-name', univ.OctetString(''))
            )
    def testIter(self):
        for t in self.e:
            break
        else:
            assert 0, '__getitem__() fails'
            
    def testGetTypeByPosition(self):
        assert self.e.getTypeByPosition(0) == univ.OctetString(''), \
               'getTypeByPosition() fails'

    def testGetNameByPosition(self):
        assert self.e.getNameByPosition(0) == 'first-name', \
               'getNameByPosition() fails'

    def testGetPositionByName(self):
        assert self.e.getPositionByName('first-name') == 0, \
               'getPositionByName() fails'

    def testGetTypesNearPosition(self):
        assert self.e.getTagMapNearPosition(0).getPosMap() == {
            univ.OctetString.tagSet: univ.OctetString('')
            }
        assert self.e.getTagMapNearPosition(1).getPosMap() == {
            univ.Integer.tagSet: univ.Integer(0),
            univ.OctetString.tagSet: univ.OctetString('')
            }
        assert self.e.getTagMapNearPosition(2).getPosMap() == {
            univ.OctetString.tagSet: univ.OctetString('')
            }

    def testGetTagMap(self):
        assert self.e.getTagMap().getPosMap() == {
            univ.OctetString.tagSet: univ.OctetString(''),
            univ.Integer.tagSet: univ.Integer(0)
            }

    def testGetTagMapWithDups(self):
        try:
            self.e.getTagMap(1)
        except PyAsn1Error:
            pass
        else:
            assert 0, 'Duped types not noticed'
        
    def testGetPositionNearType(self):
        assert self.e.getPositionNearType(univ.OctetString.tagSet, 0) == 0
        assert self.e.getPositionNearType(univ.Integer.tagSet, 1) == 1
        assert self.e.getPositionNearType(univ.OctetString.tagSet, 2) == 2

class OrderedNamedTypesCaseBase(unittest.TestCase):
    def setUp(self):
        self.e = namedtype.NamedTypes(
            namedtype.NamedType('first-name', univ.OctetString('')),
            namedtype.NamedType('age', univ.Integer(0))
            )
            
    def testGetTypeByPosition(self):
        assert self.e.getTypeByPosition(0) == univ.OctetString(''), \
               'getTypeByPosition() fails'

if __name__ == '__main__': unittest.main()
