# -*- test-case-name: buildbot.test.test_util -*-

from twisted.trial import unittest

from buildbot import util


class Foo(util.ComparableMixin):
    compare_attrs = ["a", "b"]

    def __init__(self, a, b, c):
        self.a, self.b, self.c = a,b,c


class Bar(Foo, util.ComparableMixin):
    compare_attrs = ["b", "c"]

class Compare(unittest.TestCase):
    def testCompare(self):
        f1 = Foo(1, 2, 3)
        f2 = Foo(1, 2, 4)
        f3 = Foo(1, 3, 4)
        b1 = Bar(1, 2, 3)
        self.failUnless(f1 == f2)
        self.failIf(f1 == f3)
        self.failIf(f1 == b1)
