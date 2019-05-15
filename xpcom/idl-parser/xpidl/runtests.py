#!/usr/bin/env python
#
# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/
#
# Unit tests for xpidl.py

import mozunit
import unittest
import xpidl
import header


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
        i = self.p.parse(
            "[scriptable, builtinclass, function, uuid(abc)] interface foo {};", filename='f')
        self.assertTrue(isinstance(i, xpidl.IDL))
        self.assertTrue(isinstance(i.productions[0], xpidl.Interface))
        iface = i.productions[0]
        self.assertEqual("foo", iface.name)
        self.assertTrue(iface.attributes.scriptable)
        self.assertTrue(iface.attributes.builtinclass)
        self.assertTrue(iface.attributes.function)

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
        self.assertEqual(xpidl.TypeId("void"), m.type)

    def testMethodParams(self):
        i = self.p.parse("""[uuid(abc)] interface foo {
long bar(in long a, in float b, [array] in long c);
};""", filename='f')
        i.resolve([], self.p, {})
        self.assertTrue(isinstance(i, xpidl.IDL))
        self.assertTrue(isinstance(i.productions[0], xpidl.Interface))
        iface = i.productions[0]
        m = iface.members[0]
        self.assertTrue(isinstance(m, xpidl.Method))
        self.assertEqual("bar", m.name)
        self.assertEqual(xpidl.TypeId("long"), m.type)
        self.assertEqual(3, len(m.params))
        self.assertEqual(xpidl.TypeId("long"), m.params[0].type)
        self.assertEqual("in", m.params[0].paramtype)
        self.assertEqual(xpidl.TypeId("float"), m.params[1].type)
        self.assertEqual("in", m.params[1].paramtype)
        self.assertEqual(xpidl.TypeId("long"), m.params[2].type)
        self.assertEqual("in", m.params[2].paramtype)
        self.assertTrue(isinstance(m.params[2].realtype, xpidl.LegacyArray))
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
        self.assertEqual(xpidl.TypeId("long"), a.type)

    def testOverloadedVirtual(self):
        i = self.p.parse("""[uuid(abc)] interface foo {
attribute long bar;
void getBar();
};""", filename='f')
        self.assertTrue(isinstance(i, xpidl.IDL))
        i.resolve([], self.p, {})

        class FdMock:
            def write(self, s):
                pass
        try:
            header.print_header(i, FdMock(), filename='f')
            self.assertTrue(False, "Header printing failed to fail")
        except Exception as e:
            self.assertEqual(
                e.args[0], "Unexpected overloaded virtual method GetBar in interface foo")

    def testBuiltinClassParent(self):
        i = self.p.parse("""
        [scriptable, builtinclass, uuid(abc)] interface foo {};
        [scriptable, uuid(def)] interface bar : foo {};
        """, filename='f')
        self.assertTrue(isinstance(i, xpidl.IDL))
        try:
            i.resolve([], self.p, {})
            self.assertTrue(False,
                            "non-builtinclasses can't inherit from builtinclasses")
        except xpidl.IDLError as e:
            self.assertEqual(
                e.message,
                "interface 'bar' is not builtinclass but derives from builtinclass 'foo'")

    def testScriptableNotXPCOM(self):
        # notxpcom method requires builtinclass on the interface
        i = self.p.parse("""
        [scriptable, uuid(abc)] interface nsIScriptableWithNotXPCOM {
          [notxpcom] void method2();
        };
        """, filename='f')
        self.assertTrue(isinstance(i, xpidl.IDL))
        try:
            i.resolve([], self.p, {})
            self.assertTrue(False,
                            "Resolve should fail for non-builtinclasses with notxpcom methods")
        except xpidl.IDLError as e:
            self.assertEqual(
                e.message, ("scriptable interface 'nsIScriptableWithNotXPCOM' "
                            "must be marked [builtinclass] because it contains a [notxpcom] "
                            "method 'method2'"))

        # notxpcom attribute requires builtinclass on the interface
        i = self.p.parse("""
        interface nsISomeInterface;
        [scriptable, uuid(abc)] interface nsIScriptableWithNotXPCOM {
          [notxpcom] attribute nsISomeInterface attrib;
        };
        """, filename='f')
        self.assertTrue(isinstance(i, xpidl.IDL))
        try:
            i.resolve([], self.p, {})
            self.assertTrue(False,
                            "Resolve should fail for non-builtinclasses with notxpcom attributes")
        except xpidl.IDLError as e:
            self.assertEqual(
                e.message, ("scriptable interface 'nsIScriptableWithNotXPCOM' must be marked "
                            "[builtinclass] because it contains a [notxpcom] attribute 'attrib'"))


if __name__ == '__main__':
    mozunit.main(runwith='unittest')
