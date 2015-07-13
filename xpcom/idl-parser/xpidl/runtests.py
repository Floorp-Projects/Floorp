#!/usr/bin/env python
#
# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/
#
# Unit tests for xpidl.py

import mozunit
import unittest
import xpidl


class TestParser(unittest.TestCase):
    def setUp(self):
        self.p = xpidl.IDLParser()

    def testEmpty(self):
        i = self.p.parse("", filename='f')
        self.assertTrue(isinstance(i, xpidl.IDL))
        self.assertEqual([], i.productions)

    def testForwardInterface(self):
        i = self.p.parse("interface foo;", filename='f')
        self.assertTrue(isinstance(i, xpidl.IDL))
        self.assertTrue(isinstance(i.productions[0], xpidl.Forward))
        self.assertEqual("foo", i.productions[0].name)

    def testInterface(self):
        i = self.p.parse("[uuid(abc)] interface foo {};", filename='f')
        self.assertTrue(isinstance(i, xpidl.IDL))
        self.assertTrue(isinstance(i.productions[0], xpidl.Interface))
        self.assertEqual("foo", i.productions[0].name)

    def testAttributes(self):
        i = self.p.parse("[scriptable, builtinclass, function, deprecated, uuid(abc)] interface foo {};", filename='f')
        self.assertTrue(isinstance(i, xpidl.IDL))
        self.assertTrue(isinstance(i.productions[0], xpidl.Interface))
        iface = i.productions[0]
        self.assertEqual("foo", iface.name)
        self.assertTrue(iface.attributes.scriptable)
        self.assertTrue(iface.attributes.builtinclass)
        self.assertTrue(iface.attributes.function)
        self.assertTrue(iface.attributes.deprecated)

        i = self.p.parse("[noscript, uuid(abc)] interface foo {};", filename='f')
        self.assertTrue(isinstance(i, xpidl.IDL))
        self.assertTrue(isinstance(i.productions[0], xpidl.Interface))
        iface = i.productions[0]
        self.assertEqual("foo", iface.name)
        self.assertTrue(iface.attributes.noscript)

    def testMethod(self):
        i = self.p.parse("""[uuid(abc)] interface foo {
void bar();
};""", filename='f')
        self.assertTrue(isinstance(i, xpidl.IDL))
        self.assertTrue(isinstance(i.productions[0], xpidl.Interface))
        iface = i.productions[0]
        m = iface.members[0]
        self.assertTrue(isinstance(m, xpidl.Method))
        self.assertEqual("bar", m.name)
        self.assertEqual("void", m.type)

    def testMethodParams(self):
        i = self.p.parse("""[uuid(abc)] interface foo {
long bar(in long a, in float b, [array] in long c);
};""", filename='f')
        i.resolve([], self.p)
        self.assertTrue(isinstance(i, xpidl.IDL))
        self.assertTrue(isinstance(i.productions[0], xpidl.Interface))
        iface = i.productions[0]
        m = iface.members[0]
        self.assertTrue(isinstance(m, xpidl.Method))
        self.assertEqual("bar", m.name)
        self.assertEqual("long", m.type)
        self.assertEqual(3, len(m.params))
        self.assertEqual("long", m.params[0].type)
        self.assertEqual("in", m.params[0].paramtype)
        self.assertEqual("float", m.params[1].type)
        self.assertEqual("in", m.params[1].paramtype)
        self.assertEqual("long", m.params[2].type)
        self.assertEqual("in", m.params[2].paramtype)
        self.assertTrue(isinstance(m.params[2].realtype, xpidl.Array))
        self.assertEqual("long", m.params[2].realtype.type.name)

    def testAttribute(self):
        i = self.p.parse("""[uuid(abc)] interface foo {
attribute long bar;
};""", filename='f')
        self.assertTrue(isinstance(i, xpidl.IDL))
        self.assertTrue(isinstance(i.productions[0], xpidl.Interface))
        iface = i.productions[0]
        a = iface.members[0]
        self.assertTrue(isinstance(a, xpidl.Attribute))
        self.assertEqual("bar", a.name)
        self.assertEqual("long", a.type)

if __name__ == '__main__':
    mozunit.main()
