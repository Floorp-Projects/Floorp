#
# This file is part of pyasn1 software.
#
# Copyright (c) 2005-2019, Ilya Etingof <etingof@gmail.com>
# License: http://snmplabs.com/pyasn1/license.html
#
import sys

try:
    import unittest2 as unittest

except ImportError:
    import unittest

from tests.base import BaseTestCase

from pyasn1.type import constraint
from pyasn1.type import error


class SingleValueConstraintTestCase(BaseTestCase):
    def setUp(self):
        BaseTestCase.setUp(self)
        self.v1 = 1, 2
        self.v2 = 3, 4
        self.c1 = constraint.SingleValueConstraint(*self.v1)
        self.c2 = constraint.SingleValueConstraint(*self.v2)

    def testCmp(self):
        assert self.c1 == self.c1, 'comparison fails'

    def testHash(self):
        assert hash(self.c1) != hash(self.c2), 'hash() fails'

    def testGoodVal(self):
        try:
            self.c1(1)

        except error.ValueConstraintError:
            assert 0, 'constraint check fails'

    def testBadVal(self):
        try:
            self.c1(4)
        except error.ValueConstraintError:
            pass
        else:
            assert 0, 'constraint check fails'

    def testContains(self):
        for v in self.v1:
            assert v in self.c1
            assert v not in self.c2

        for v in self.v2:
            assert v in self.c2
            assert v not in self.c1

    def testIter(self):
        assert set(self.v1) == set(self.c1)
        assert set(self.v2) == set(self.c2)

    def testSub(self):
        subconst = self.c1 - constraint.SingleValueConstraint(self.v1[0])
        assert list(subconst) == [self.v1[1]]

    def testAdd(self):
        superconst = self.c1 + self.c2
        assert set(superconst) == set(self.v1 + self.v2)


class ContainedSubtypeConstraintTestCase(BaseTestCase):
    def setUp(self):
        BaseTestCase.setUp(self)
        self.c1 = constraint.ContainedSubtypeConstraint(
            constraint.SingleValueConstraint(12)
        )

    def testGoodVal(self):
        try:
            self.c1(12)
        except error.ValueConstraintError:
            assert 0, 'constraint check fails'

    def testBadVal(self):
        try:
            self.c1(4)
        except error.ValueConstraintError:
            pass
        else:
            assert 0, 'constraint check fails'


class ValueRangeConstraintTestCase(BaseTestCase):
    def setUp(self):
        BaseTestCase.setUp(self)
        self.c1 = constraint.ValueRangeConstraint(1, 4)

    def testGoodVal(self):
        try:
            self.c1(1)
        except error.ValueConstraintError:
            assert 0, 'constraint check fails'

    def testBadVal(self):
        try:
            self.c1(-5)
        except error.ValueConstraintError:
            pass
        else:
            assert 0, 'constraint check fails'


class ValueSizeConstraintTestCase(BaseTestCase):
    def setUp(self):
        BaseTestCase.setUp(self)
        self.c1 = constraint.ValueSizeConstraint(1, 2)

    def testGoodVal(self):
        try:
            self.c1('a')
        except error.ValueConstraintError:
            assert 0, 'constraint check fails'

    def testBadVal(self):
        try:
            self.c1('abc')
        except error.ValueConstraintError:
            pass
        else:
            assert 0, 'constraint check fails'


class PermittedAlphabetConstraintTestCase(SingleValueConstraintTestCase):
    def setUp(self):
        self.v1 = 'A', 'B'
        self.v2 = 'C', 'D'
        self.c1 = constraint.PermittedAlphabetConstraint(*self.v1)
        self.c2 = constraint.PermittedAlphabetConstraint(*self.v2)

    def testGoodVal(self):
        try:
            self.c1('A')

        except error.ValueConstraintError:
            assert 0, 'constraint check fails'

    def testBadVal(self):
        try:
            self.c1('E')

        except error.ValueConstraintError:
            pass

        else:
            assert 0, 'constraint check fails'


class WithComponentsConstraintTestCase(BaseTestCase):

    def testGoodVal(self):
        c = constraint.WithComponentsConstraint(
            ('A', constraint.ComponentPresentConstraint()),
            ('B', constraint.ComponentAbsentConstraint()))

        try:
            c({'A': 1})

        except error.ValueConstraintError:
            assert 0, 'constraint check fails'

    def testGoodValWithExtraFields(self):
        c = constraint.WithComponentsConstraint(
            ('A', constraint.ComponentPresentConstraint()),
            ('B', constraint.ComponentAbsentConstraint())
        )

        try:
            c({'A': 1, 'C': 2})

        except error.ValueConstraintError:
            assert 0, 'constraint check fails'

    def testEmptyConstraint(self):
        c = constraint.WithComponentsConstraint()

        try:
            c({'A': 1})

        except error.ValueConstraintError:
            assert 0, 'constraint check fails'

    def testBadVal(self):
        c = constraint.WithComponentsConstraint(
            ('A', constraint.ComponentPresentConstraint())
        )

        try:
            c({'B': 2})

        except error.ValueConstraintError:
            pass

        else:
            assert 0, 'constraint check fails'

    def testBadValExtraFields(self):
        c = constraint.WithComponentsConstraint(
            ('A', constraint.ComponentPresentConstraint())
        )

        try:
            c({'B': 2, 'C': 3})

        except error.ValueConstraintError:
            pass

        else:
            assert 0, 'constraint check fails'


class ConstraintsIntersectionTestCase(BaseTestCase):
    def setUp(self):
        BaseTestCase.setUp(self)
        self.c1 = constraint.ConstraintsIntersection(
            constraint.SingleValueConstraint(4),
            constraint.ValueRangeConstraint(2, 4)
        )

    def testCmp1(self):
        assert constraint.SingleValueConstraint(4) in self.c1, '__cmp__() fails'

    def testCmp2(self):
        assert constraint.SingleValueConstraint(5) not in self.c1, \
            '__cmp__() fails'

    def testCmp3(self):
        c = constraint.ConstraintsUnion(constraint.ConstraintsIntersection(
            constraint.SingleValueConstraint(4),
            constraint.ValueRangeConstraint(2, 4))
        )
        assert self.c1 in c, '__cmp__() fails'

    def testCmp4(self):
        c = constraint.ConstraintsUnion(
            constraint.ConstraintsIntersection(constraint.SingleValueConstraint(5))
        )
        assert self.c1 not in c, '__cmp__() fails'

    def testGoodVal(self):
        try:
            self.c1(4)
        except error.ValueConstraintError:
            assert 0, 'constraint check fails'

    def testBadVal(self):
        try:
            self.c1(-5)
        except error.ValueConstraintError:
            pass
        else:
            assert 0, 'constraint check fails'


class InnerTypeConstraintTestCase(BaseTestCase):
    def testConst1(self):
        c = constraint.InnerTypeConstraint(
            constraint.SingleValueConstraint(4)
        )
        try:
            c(4, 32)
        except error.ValueConstraintError:
            assert 0, 'constraint check fails'
        try:
            c(5, 32)
        except error.ValueConstraintError:
            pass
        else:
            assert 0, 'constraint check fails'

    def testConst2(self):
        c = constraint.InnerTypeConstraint(
            (0, constraint.SingleValueConstraint(4), 'PRESENT'),
            (1, constraint.SingleValueConstraint(4), 'ABSENT')
        )
        try:
            c(4, 0)
        except error.ValueConstraintError:
            assert 0, 'constraint check fails'
        try:
            c(4, 1)
        except error.ValueConstraintError:
            pass
        else:
            assert 0, 'constraint check fails'
        try:
            c(3, 0)
        except error.ValueConstraintError:
            pass
        else:
            assert 0, 'constraint check fails'

        # Constraints compositions


class ConstraintsIntersectionRangeTestCase(BaseTestCase):
    def setUp(self):
        BaseTestCase.setUp(self)
        self.c1 = constraint.ConstraintsIntersection(
            constraint.ValueRangeConstraint(1, 9),
            constraint.ValueRangeConstraint(2, 5)
        )

    def testGoodVal(self):
        try:
            self.c1(3)
        except error.ValueConstraintError:
            assert 0, 'constraint check fails'

    def testBadVal(self):
        try:
            self.c1(0)
        except error.ValueConstraintError:
            pass
        else:
            assert 0, 'constraint check fails'


class ConstraintsUnionTestCase(BaseTestCase):
    def setUp(self):
        BaseTestCase.setUp(self)
        self.c1 = constraint.ConstraintsUnion(
            constraint.SingleValueConstraint(5),
            constraint.ValueRangeConstraint(1, 3)
        )

    def testGoodVal(self):
        try:
            self.c1(2)
            self.c1(5)
        except error.ValueConstraintError:
            assert 0, 'constraint check fails'

    def testBadVal(self):
        try:
            self.c1(-5)
        except error.ValueConstraintError:
            pass
        else:
            assert 0, 'constraint check fails'


class ConstraintsExclusionTestCase(BaseTestCase):
    def setUp(self):
        BaseTestCase.setUp(self)
        self.c1 = constraint.ConstraintsExclusion(
            constraint.ValueRangeConstraint(2, 4)
        )

    def testGoodVal(self):
        try:
            self.c1(6)
        except error.ValueConstraintError:
            assert 0, 'constraint check fails'

    def testBadVal(self):
        try:
            self.c1(2)
        except error.ValueConstraintError:
            pass
        else:
            assert 0, 'constraint check fails'


# Constraints derivations

class DirectDerivationTestCase(BaseTestCase):
    def setUp(self):
        BaseTestCase.setUp(self)

        self.c1 = constraint.SingleValueConstraint(5)

        self.c2 = constraint.ConstraintsUnion(
            self.c1, constraint.ValueRangeConstraint(1, 3)
        )

    def testGoodVal(self):
        assert self.c1.isSuperTypeOf(self.c2), 'isSuperTypeOf failed'
        assert not self.c1.isSubTypeOf(self.c2), 'isSubTypeOf failed'

    def testBadVal(self):
        assert not self.c2.isSuperTypeOf(self.c1), 'isSuperTypeOf failed'
        assert self.c2.isSubTypeOf(self.c1), 'isSubTypeOf failed'


class IndirectDerivationTestCase(BaseTestCase):
    def setUp(self):
        BaseTestCase.setUp(self)

        self.c1 = constraint.ConstraintsIntersection(
            constraint.ValueRangeConstraint(1, 30)
        )

        self.c2 = constraint.ConstraintsIntersection(
            self.c1, constraint.ValueRangeConstraint(1, 20)
        )

        self.c2 = constraint.ConstraintsIntersection(
            self.c2, constraint.ValueRangeConstraint(1, 10)
        )

    def testGoodVal(self):
        assert self.c1.isSuperTypeOf(self.c2), 'isSuperTypeOf failed'
        assert not self.c1.isSubTypeOf(self.c2), 'isSubTypeOf failed'

    def testBadVal(self):
        assert not self.c2.isSuperTypeOf(self.c1), 'isSuperTypeOf failed'
        assert self.c2.isSubTypeOf(self.c1), 'isSubTypeOf failed'

# TODO: how to apply size constraints to constructed types?

suite = unittest.TestLoader().loadTestsFromModule(sys.modules[__name__])

if __name__ == '__main__':
    unittest.TextTestRunner(verbosity=2).run(suite)
