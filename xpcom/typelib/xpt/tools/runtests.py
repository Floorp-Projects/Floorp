#!/usr/bin/env python
# Copyright 2010,2011 Mozilla Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#   1. Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#
#   2. Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in
#      the documentation and/or other materials provided with the
#      distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE MOZILLA FOUNDATION ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE MOZILLA FOUNDATION OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# The views and conclusions contained in the software and documentation
# are those of the authors and should not be interpreted as representing
# official policies, either expressed or implied, of the Mozilla
# Foundation.

import difflib
import os
import shutil
from StringIO import StringIO
import subprocess
import sys
import tempfile
import mozunit
import unittest
import xpt

def get_output(bin, file):
    p = subprocess.Popen([bin, file], stdout=subprocess.PIPE)
    stdout, _ = p.communicate()
    return stdout

if "MOZILLA_OBJDIR" in os.environ:
    class CheckXPTDump(unittest.TestCase):
        def test_xpt_dump_diffs(self):
            MOZILLA_OBJDIR = os.environ["MOZILLA_OBJDIR"]
            xptdump = os.path.abspath(os.path.join(MOZILLA_OBJDIR,
                                                   "dist", "bin", "xpt_dump"))
            components = os.path.abspath(os.path.join(MOZILLA_OBJDIR,
                                                   "dist", "bin", "components"))
            for f in os.listdir(components):
                if not f.endswith(".xpt"):
                    continue
                fullpath = os.path.join(components, f)
                # read a Typelib and dump it to a string
                t = xpt.Typelib.read(fullpath)
                self.assert_(t is not None)
                outf = StringIO()
                t.dump(outf)
                out = outf.getvalue()
                # now run xpt_dump on it
                out2 = get_output(xptdump, fullpath)
                if out != out2:
                    print "diff %s" % f
                    for line in difflib.unified_diff(out2.split("\n"), out.split("\n"), lineterm=""):
                        print line
                self.assert_(out == out2, "xpt_dump output should be identical for %s" % f)

class TestIIDString(unittest.TestCase):
    def test_iid_str_roundtrip(self):
        iid_str = "11223344-5566-7788-9900-aabbccddeeff"
        iid = xpt.Typelib.string_to_iid(iid_str)
        self.assertEqual(iid_str, xpt.Typelib.iid_to_string(iid))

    def test_iid_roundtrip(self):
        iid = "\x11\x22\x33\x44\x55\x66\x77\x88\x99\x00\xaa\xbb\xcc\xdd\xee\xff"
        iid_str = xpt.Typelib.iid_to_string(iid)
        self.assertEqual(iid, xpt.Typelib.string_to_iid(iid_str))

class TypelibCompareMixin:
    def assertEqualTypelibs(self, t1, t2):
        self.assert_(t1 is not None, "Should not be None")
        self.assert_(t2 is not None, "Should not be None")
        self.assertEqual(t1.version, t2.version, "Versions should be equal")
        self.assertEqual(len(t1.interfaces), len(t2.interfaces),
                         "Number of interfaces should be equal")
        for i, j in zip(t1.interfaces, t2.interfaces):
            self.assertEqualInterfaces(i, j)

    def assertEqualInterfaces(self, i1, i2):
        self.assert_(i1 is not None, "Should not be None")
        self.assert_(i2 is not None, "Should not be None")
        self.assertEqual(i1.name, i2.name, "Names should be equal")
        self.assertEqual(i1.iid, i2.iid, "IIDs should be equal")
        self.assertEqual(i1.namespace, i2.namespace,
                         "Namespaces should be equal")
        self.assertEqual(i1.resolved, i2.resolved,
                         "Resolved status should be equal")
        if i1.resolved:
            if i1.parent or i2.parent:
                # Can't test exact equality, probably different objects
                self.assertEqualInterfaces(i1.parent, i2.parent)
            self.assertEqual(len(i1.methods), len(i2.methods))
            for m, n in zip(i1.methods, i2.methods):
                self.assertEqualMethods(m, n)
            self.assertEqual(len(i1.constants), len(i2.constants))
            for c, d in zip(i1.constants, i2.constants):
                self.assertEqualConstants(c, d)
            self.assertEqual(i1.scriptable, i2.scriptable,
                             "Scriptable status should be equal")
            self.assertEqual(i1.function, i2.function,
                             "Function status should be equal")

    def assertEqualMethods(self, m1, m2):
        self.assert_(m1 is not None, "Should not be None")
        self.assert_(m2 is not None, "Should not be None")
        self.assertEqual(m1.name, m2.name, "Names should be equal")
        self.assertEqual(m1.getter, m2.getter, "Getter flag should be equal")
        self.assertEqual(m1.setter, m2.setter, "Setter flag should be equal")
        self.assertEqual(m1.notxpcom, m2.notxpcom,
                         "notxpcom flag should be equal")
        self.assertEqual(m1.constructor, m2.constructor,
                         "constructor flag should be equal")
        self.assertEqual(m1.hidden, m2.hidden, "hidden flag should be equal")
        self.assertEqual(m1.optargc, m2.optargc, "optargc flag should be equal")
        self.assertEqual(m1.implicit_jscontext, m2.implicit_jscontext,
                         "implicit_jscontext flag should be equal")
        for p1, p2 in zip(m1.params, m2.params):
            self.assertEqualParams(p1, p2)
        self.assertEqualParams(m1.result, m2.result)
        
    def assertEqualConstants(self, c1, c2):
        self.assert_(c1 is not None, "Should not be None")
        self.assert_(c2 is not None, "Should not be None")
        self.assertEqual(c1.name, c2.name)
        self.assertEqual(c1.value, c2.value)
        self.assertEqualTypes(c1.type, c2.type)

    def assertEqualParams(self, p1, p2):
        self.assert_(p1 is not None, "Should not be None")
        self.assert_(p2 is not None, "Should not be None")
        self.assertEqualTypes(p1.type, p2.type)
        self.assertEqual(p1.in_, p2.in_)
        self.assertEqual(p1.out, p2.out)
        self.assertEqual(p1.retval, p2.retval)
        self.assertEqual(p1.shared, p2.shared)
        self.assertEqual(p1.dipper, p2.dipper)
        self.assertEqual(p1.optional, p2.optional)
        
    def assertEqualTypes(self, t1, t2):
        self.assert_(t1 is not None, "Should not be None")
        self.assert_(t2 is not None, "Should not be None")
        self.assertEqual(type(t1), type(t2), "type types should be equal")
        self.assertEqual(t1.pointer, t2.pointer,
                         "pointer flag should be equal for %s and %s" % (t1, t2))
        self.assertEqual(t1.reference, t2.reference)
        if isinstance(t1, xpt.SimpleType):
            self.assertEqual(t1.tag, t2.tag)
        elif isinstance(t1, xpt.InterfaceType):
            self.assertEqualInterfaces(t1.iface, t2.iface)
        elif isinstance(t1, xpt.InterfaceIsType):
            self.assertEqual(t1.param_index, t2.param_index)
        elif isinstance(t1, xpt.ArrayType):
            self.assertEqualTypes(t1.element_type, t2.element_type)
            self.assertEqual(t1.size_is_arg_num, t2.size_is_arg_num)
            self.assertEqual(t1.length_is_arg_num, t2.length_is_arg_num)
        elif isinstance(t1, xpt.StringWithSizeType) or isinstance(t1, xpt.WideStringWithSizeType):
            self.assertEqual(t1.size_is_arg_num, t2.size_is_arg_num)
            self.assertEqual(t1.length_is_arg_num, t2.length_is_arg_num)

class TestTypelibReadWrite(unittest.TestCase, TypelibCompareMixin):
    def test_read_file(self):
        """
        Test that a Typelib can be read/written from/to a file.
        """
        t = xpt.Typelib()
        # add an unresolved interface
        t.interfaces.append(xpt.Interface("IFoo"))
        fd, f = tempfile.mkstemp()
        os.close(fd)
        t.write(f)
        t2 = xpt.Typelib.read(f)
        os.remove(f)
        self.assert_(t2 is not None)
        self.assertEqualTypelibs(t, t2)


#TODO: test flags in various combinations
class TestTypelibRoundtrip(unittest.TestCase, TypelibCompareMixin):
    def checkRoundtrip(self, t):
        s = StringIO()
        t.write(s)
        s.seek(0)
        t2 = xpt.Typelib.read(s)
        self.assert_(t2 is not None)
        self.assertEqualTypelibs(t, t2)
        
    def test_simple(self):
        t = xpt.Typelib()
        # add an unresolved interface
        t.interfaces.append(xpt.Interface("IFoo"))
        self.checkRoundtrip(t)
        
        t = xpt.Typelib()
        # add an unresolved interface with an IID
        t.interfaces.append(xpt.Interface("IBar", "11223344-5566-7788-9900-aabbccddeeff"))
        self.checkRoundtrip(t)

    def test_parent(self):
        """
        Test that an interface's parent property is correctly serialized
        and deserialized.

        """
        t = xpt.Typelib()
        pi = xpt.Interface("IParent")
        t.interfaces.append(pi)
        t.interfaces.append(xpt.Interface("IChild", iid="11223344-5566-7788-9900-aabbccddeeff",
                                          parent=pi, resolved=True))
        self.checkRoundtrip(t)

    def test_ifaceFlags(self):
        """
        Test that an interface's flags are correctly serialized
        and deserialized.

        """
        t = xpt.Typelib()
        t.interfaces.append(xpt.Interface("IFlags", iid="11223344-5566-7788-9900-aabbccddeeff",
                                          resolved=True,
                                          scriptable=True,
                                          function=True))
        self.checkRoundtrip(t)

    def test_constants(self):
        c = xpt.Constant("X", xpt.SimpleType(xpt.Type.Tags.uint32),
                         0xF000F000)
        i = xpt.Interface("IFoo", iid="11223344-5566-7788-9900-aabbccddeeff",
                          constants=[c])
        t = xpt.Typelib(interfaces=[i])
        self.checkRoundtrip(t)
        # tack on some more constants
        i.constants.append(xpt.Constant("Y",
                                        xpt.SimpleType(xpt.Type.Tags.int16),
                                        -30000))
        i.constants.append(xpt.Constant("Z",
                                        xpt.SimpleType(xpt.Type.Tags.uint16),
                                        0xB0B0))
        i.constants.append(xpt.Constant("A",
                                        xpt.SimpleType(xpt.Type.Tags.int32),
                                        -1000000))
        self.checkRoundtrip(t)

    def test_methods(self):
        p = xpt.Param(xpt.SimpleType(xpt.Type.Tags.void))
        m = xpt.Method("Bar", p)
        i = xpt.Interface("IFoo", iid="11223344-5566-7788-9900-aabbccddeeff",
                          methods=[m])
        t = xpt.Typelib(interfaces=[i])
        self.checkRoundtrip(t)
        # add some more methods
        i.methods.append(xpt.Method("One", xpt.Param(xpt.SimpleType(xpt.Type.Tags.int32)),
                                    params=[
                                        xpt.Param(xpt.SimpleType(xpt.Type.Tags.int64)),
                                        xpt.Param(xpt.SimpleType(xpt.Type.Tags.float, pointer=True))
                                        ]))
        self.checkRoundtrip(t)
        # test some other types (should really be more thorough)
        i.methods.append(xpt.Method("Two", xpt.Param(xpt.SimpleType(xpt.Type.Tags.int32)),
                                    params=[
                                        xpt.Param(xpt.SimpleType(xpt.Type.Tags.UTF8String, pointer=True)),
                                        xpt.Param(xpt.SimpleType(xpt.Type.Tags.wchar_t_ptr, pointer=True))
                                        ]))
        self.checkRoundtrip(t)
        # add a method with an InterfaceType argument
        bar = xpt.Interface("IBar")
        t.interfaces.append(bar)
        i.methods.append(xpt.Method("IFaceMethod", xpt.Param(xpt.SimpleType(xpt.Type.Tags.int32)),
                                    params=[
                                        xpt.Param(xpt.InterfaceType(bar))
                                        ]))
        self.checkRoundtrip(t)

        # add a method with an InterfaceIsType argument
        i.methods.append(xpt.Method("IFaceIsMethod", xpt.Param(xpt.SimpleType(xpt.Type.Tags.void)),
                                    params=[
                                        xpt.Param(xpt.InterfaceIsType(1)),
                                        xpt.Param(xpt.SimpleType(xpt.Type.Tags.nsIID))
                                        ]))
        self.checkRoundtrip(t)

        # add a method with an ArrayType argument
        i.methods.append(xpt.Method("ArrayMethod", xpt.Param(xpt.SimpleType(xpt.Type.Tags.void)),
                                    params=[
                                        xpt.Param(xpt.ArrayType(
                                            xpt.SimpleType(xpt.Type.Tags.int32),
                                            1, 2)),
                                        xpt.Param(xpt.SimpleType(xpt.Type.Tags.int32)),
                                        xpt.Param(xpt.SimpleType(xpt.Type.Tags.int32)),
                                        ]))
        self.checkRoundtrip(t)
        
        # add a method with a StringWithSize and WideStringWithSize arguments
        i.methods.append(xpt.Method("StringWithSizeMethod", xpt.Param(xpt.SimpleType(xpt.Type.Tags.void)),
                                    params=[
                                        xpt.Param(xpt.StringWithSizeType(
                                            1, 2)),
                                        xpt.Param(xpt.SimpleType(xpt.Type.Tags.int32)),
                                        xpt.Param(xpt.SimpleType(xpt.Type.Tags.int32)),
                                        xpt.Param(xpt.WideStringWithSizeType(
                                            4, 5)),
                                        xpt.Param(xpt.SimpleType(xpt.Type.Tags.int32)),
                                        xpt.Param(xpt.SimpleType(xpt.Type.Tags.int32)),
                                        ]))
        self.checkRoundtrip(t)

class TestInterfaceCmp(unittest.TestCase):
    def test_unresolvedName(self):
        """
        Test comparison function on xpt.Interface by name.
        
        """
        i1 = xpt.Interface("ABC")
        i2 = xpt.Interface("DEF")
        self.assert_(i1 < i2)
        self.assert_(i1 != i2)

    def test_unresolvedEqual(self):
        """
        Test comparison function on xpt.Interface with equal names and IIDs.
        
        """
        i1 = xpt.Interface("ABC")
        i2 = xpt.Interface("ABC")
        self.assert_(i1 == i2)

    def test_unresolvedIID(self):
        """
        Test comparison function on xpt.Interface with different IIDs.
        
        """
        # IIDs sort before names
        i1 = xpt.Interface("ABC", iid="22334411-5566-7788-9900-aabbccddeeff")
        i2 = xpt.Interface("DEF", iid="11223344-5566-7788-9900-aabbccddeeff")
        self.assert_(i2 < i1)
        self.assert_(i2 != i1)

    def test_unresolvedResolved(self):
        """
        Test comparison function on xpt.Interface with interfaces with
        identical names and IIDs but different resolved status.
        
        """
        i1 = xpt.Interface("ABC", iid="11223344-5566-7788-9900-aabbccddeeff")
        p = xpt.Param(xpt.SimpleType(xpt.Type.Tags.void))
        m = xpt.Method("Bar", p)
        i2 = xpt.Interface("ABC", iid="11223344-5566-7788-9900-aabbccddeeff",
                           methods=[m])
        self.assert_(i2 < i1)
        self.assert_(i2 != i1)

    def test_resolvedIdentical(self):
        """
        Test comparison function on xpt.Interface with interfaces with
        identical names and IIDs, both of which are resolved.
        
        """
        p = xpt.Param(xpt.SimpleType(xpt.Type.Tags.void))
        m = xpt.Method("Bar", p)
        i1 = xpt.Interface("ABC", iid="11223344-5566-7788-9900-aabbccddeeff",
                           methods=[m])
        i2 = xpt.Interface("ABC", iid="11223344-5566-7788-9900-aabbccddeeff",
                           methods=[m])
        self.assert_(i2 == i1)

class TestXPTLink(unittest.TestCase):
    def test_mergeDifferent(self):
        """
        Test that merging two typelibs with completely different interfaces
        produces the correctly merged typelib.
        
        """
        t1 = xpt.Typelib()
        # add an unresolved interface
        t1.interfaces.append(xpt.Interface("IFoo"))
        t2 = xpt.Typelib()
        # add an unresolved interface
        t2.interfaces.append(xpt.Interface("IBar"))
        t3 = xpt.xpt_link([t1, t2])
        
        self.assertEqual(2, len(t3.interfaces))
        # Interfaces should wind up sorted
        self.assertEqual("IBar", t3.interfaces[0].name)
        self.assertEqual("IFoo", t3.interfaces[1].name)

        # Add some IID values
        t1 = xpt.Typelib()
        # add an unresolved interface
        t1.interfaces.append(xpt.Interface("IFoo", iid="11223344-5566-7788-9900-aabbccddeeff"))
        t2 = xpt.Typelib()
        # add an unresolved interface
        t2.interfaces.append(xpt.Interface("IBar", iid="44332211-6655-8877-0099-aabbccddeeff"))
        t3 = xpt.xpt_link([t1, t2])
        
        self.assertEqual(2, len(t3.interfaces))
        # Interfaces should wind up sorted
        self.assertEqual("IFoo", t3.interfaces[0].name)
        self.assertEqual("IBar", t3.interfaces[1].name)

    def test_mergeConflict(self):
        """
        Test that merging two typelibs with conflicting interface definitions
        raises an error.
        
        """
        # Same names, different IIDs
        t1 = xpt.Typelib()
        # add an unresolved interface
        t1.interfaces.append(xpt.Interface("IFoo", iid="11223344-5566-7788-9900-aabbccddeeff"))
        t2 = xpt.Typelib()
        # add an unresolved interface, same name different IID
        t2.interfaces.append(xpt.Interface("IFoo", iid="44332211-6655-8877-0099-aabbccddeeff"))
        self.assertRaises(xpt.DataError, xpt.xpt_link, [t1, t2])

        # Same IIDs, different names
        t1 = xpt.Typelib()
        # add an unresolved interface
        t1.interfaces.append(xpt.Interface("IFoo", iid="11223344-5566-7788-9900-aabbccddeeff"))
        t2 = xpt.Typelib()
        # add an unresolved interface, same IID different name
        t2.interfaces.append(xpt.Interface("IBar", iid="11223344-5566-7788-9900-aabbccddeeff"))
        self.assertRaises(xpt.DataError, xpt.xpt_link, [t1, t2])

    def test_mergeUnresolvedIID(self):
        """
        Test that merging a typelib with an unresolved definition of
        an interface that's also unresolved in this typelib, but one
        has a valid IID copies the IID value to the resulting typelib.

        """
        # Unresolved in both, but t1 has an IID value
        t1 = xpt.Typelib()
        # add an unresolved interface with a valid IID
        t1.interfaces.append(xpt.Interface("IFoo", iid="11223344-5566-7788-9900-aabbccddeeff"))
        t2 = xpt.Typelib()
        # add an unresolved interface, no IID
        t2.interfaces.append(xpt.Interface("IFoo"))
        t3 = xpt.xpt_link([t1, t2])
        
        self.assertEqual(1, len(t3.interfaces))
        self.assertEqual("IFoo", t3.interfaces[0].name)
        self.assertEqual("11223344-5566-7788-9900-aabbccddeeff", t3.interfaces[0].iid)
        # Unresolved in both, but t2 has an IID value
        t1 = xpt.Typelib()
        # add an unresolved interface, no IID
        t1.interfaces.append(xpt.Interface("IFoo"))
        t2 = xpt.Typelib()
        # add an unresolved interface with a valid IID
        t2.interfaces.append(xpt.Interface("IFoo", iid="11223344-5566-7788-9900-aabbccddeeff"))
        t3 = xpt.xpt_link([t1, t2])
        
        self.assertEqual(1, len(t3.interfaces))
        self.assertEqual("IFoo", t3.interfaces[0].name)
        self.assertEqual("11223344-5566-7788-9900-aabbccddeeff", t3.interfaces[0].iid)

    def test_mergeResolvedUnresolved(self):
        """
        Test that merging two typelibs, one of which contains an unresolved
        reference to an interface, and the other of which contains a
        resolved reference to the same interface results in keeping the
        resolved reference.

        """
        # t1 has an unresolved interface, t2 has a resolved version
        t1 = xpt.Typelib()
        # add an unresolved interface
        t1.interfaces.append(xpt.Interface("IFoo"))
        t2 = xpt.Typelib()
        # add a resolved interface
        p = xpt.Param(xpt.SimpleType(xpt.Type.Tags.void))
        m = xpt.Method("Bar", p)
        t2.interfaces.append(xpt.Interface("IFoo", iid="11223344-5566-7788-9900-aabbccddeeff",
                                           methods=[m]))
        t3 = xpt.xpt_link([t1, t2])
        
        self.assertEqual(1, len(t3.interfaces))
        self.assertEqual("IFoo", t3.interfaces[0].name)
        self.assertEqual("11223344-5566-7788-9900-aabbccddeeff", t3.interfaces[0].iid)
        self.assert_(t3.interfaces[0].resolved)
        self.assertEqual(1, len(t3.interfaces[0].methods))
        self.assertEqual("Bar", t3.interfaces[0].methods[0].name)

        # t1 has a resolved interface, t2 has an unresolved version
        t1 = xpt.Typelib()
        # add a resolved interface
        p = xpt.Param(xpt.SimpleType(xpt.Type.Tags.void))
        m = xpt.Method("Bar", p)
        t1.interfaces.append(xpt.Interface("IFoo", iid="11223344-5566-7788-9900-aabbccddeeff",
                                           methods=[m]))
        t2 = xpt.Typelib()
        # add an unresolved interface
        t2.interfaces.append(xpt.Interface("IFoo"))
        t3 = xpt.xpt_link([t1, t2])
        
        self.assertEqual(1, len(t3.interfaces))
        self.assertEqual("IFoo", t3.interfaces[0].name)
        self.assertEqual("11223344-5566-7788-9900-aabbccddeeff", t3.interfaces[0].iid)
        self.assert_(t3.interfaces[0].resolved)
        self.assertEqual(1, len(t3.interfaces[0].methods))
        self.assertEqual("Bar", t3.interfaces[0].methods[0].name)

    def test_mergeReplaceParents(self):
        """
        Test that merging an interface results in other interfaces' parent
        member being updated properly.

        """
        # t1 has an unresolved interface, t2 has a resolved version,
        # but t1 also has another interface whose parent is the unresolved
        # interface.
        t1 = xpt.Typelib()
        # add an unresolved interface
        pi = xpt.Interface("IFoo")
        t1.interfaces.append(pi)
        # add a child of the unresolved interface
        t1.interfaces.append(xpt.Interface("IChild", iid="11111111-1111-1111-1111-111111111111",
                                           resolved=True, parent=pi))
        t2 = xpt.Typelib()
        # add a resolved interface
        p = xpt.Param(xpt.SimpleType(xpt.Type.Tags.void))
        m = xpt.Method("Bar", p)
        t2.interfaces.append(xpt.Interface("IFoo", iid="11223344-5566-7788-9900-aabbccddeeff",
                                           methods=[m]))
        t3 = xpt.xpt_link([t1, t2])
        
        self.assertEqual(2, len(t3.interfaces))
        self.assertEqual("IChild", t3.interfaces[0].name)
        self.assertEqual("11111111-1111-1111-1111-111111111111", t3.interfaces[0].iid)
        self.assertEqual("IFoo", t3.interfaces[1].name)
        self.assertEqual("11223344-5566-7788-9900-aabbccddeeff", t3.interfaces[1].iid)
        self.assert_(t3.interfaces[0].resolved)
        # Ensure that IChild's parent has been updated
        self.assertEqual(t3.interfaces[1], t3.interfaces[0].parent)
        self.assert_(t3.interfaces[0].parent.resolved)

        # t1 has a resolved interface, t2 has an unresolved version,
        # but t2 also has another interface whose parent is the unresolved
        # interface.
        t1 = xpt.Typelib()
        # add a resolved interface
        p = xpt.Param(xpt.SimpleType(xpt.Type.Tags.void))
        m = xpt.Method("Bar", p)
        t1.interfaces.append(xpt.Interface("IFoo", iid="11223344-5566-7788-9900-aabbccddeeff",
                                           methods=[m]))
        t2 = xpt.Typelib()
        # add an unresolved interface
        pi = xpt.Interface("IFoo")
        t2.interfaces.append(pi)
        # add a child of the unresolved interface
        t2.interfaces.append(xpt.Interface("IChild", iid="11111111-1111-1111-1111-111111111111",
                                           resolved=True, parent=pi))
        t3 = xpt.xpt_link([t1, t2])
        
        self.assertEqual(2, len(t3.interfaces))
        self.assertEqual("IChild", t3.interfaces[0].name)
        self.assertEqual("11111111-1111-1111-1111-111111111111", t3.interfaces[0].iid)
        self.assertEqual("IFoo", t3.interfaces[1].name)
        self.assertEqual("11223344-5566-7788-9900-aabbccddeeff", t3.interfaces[1].iid)
        self.assert_(t3.interfaces[0].resolved)
        # Ensure that IChild's parent has been updated
        self.assertEqual(t3.interfaces[1], t3.interfaces[0].parent)
        self.assert_(t3.interfaces[0].parent.resolved)

    def test_mergeReplaceRetval(self):
        """
        Test that merging an interface correctly updates InterfaceType
        return values on methods of other interfaces.

        """
        # t1 has an unresolved interface and an interface that uses the
        # unresolved interface as a return value from a method. t2
        # has a resolved version of the unresolved interface.
        t1 = xpt.Typelib()
        # add an unresolved interface
        i = xpt.Interface("IFoo")
        t1.interfaces.append(i)
        # add an interface that uses the unresolved interface
        # as a return value in a method.
        p = xpt.Param(xpt.InterfaceType(i))
        m = xpt.Method("ReturnIface", p)
        t1.interfaces.append(xpt.Interface("IRetval", iid="11111111-1111-1111-1111-111111111111",
                                           methods=[m]))
        t2 = xpt.Typelib()
        # add a resolved interface
        p = xpt.Param(xpt.SimpleType(xpt.Type.Tags.void))
        m = xpt.Method("Bar", p)
        t2.interfaces.append(xpt.Interface("IFoo", iid="11223344-5566-7788-9900-aabbccddeeff",
                                           methods=[m]))
        t3 = xpt.xpt_link([t1, t2])
        
        self.assertEqual(2, len(t3.interfaces))
        self.assertEqual("IRetval", t3.interfaces[0].name)
        self.assertEqual("11111111-1111-1111-1111-111111111111", t3.interfaces[0].iid)
        self.assertEqual("IFoo", t3.interfaces[1].name)
        self.assertEqual("11223344-5566-7788-9900-aabbccddeeff", t3.interfaces[1].iid)
        self.assert_(t3.interfaces[1].resolved)
        # Ensure that IRetval's method's return value type has been updated.
        self.assertEqual(1, len(t3.interfaces[0].methods))
        self.assert_(t3.interfaces[0].methods[0].result.type.iface.resolved)
        self.assertEqual(t3.interfaces[1],
                         t3.interfaces[0].methods[0].result.type.iface)

        # t1 has a resolved interface. t2 has an unresolved version and
        # an interface that uses the unresolved interface as a return value
        # from a method.
        t1 = xpt.Typelib()
        # add a resolved interface
        p = xpt.Param(xpt.SimpleType(xpt.Type.Tags.void))
        m = xpt.Method("Bar", p)
        t1.interfaces.append(xpt.Interface("IFoo", iid="11223344-5566-7788-9900-aabbccddeeff",
                                           methods=[m]))
        t2 = xpt.Typelib()
        # add an unresolved interface
        i = xpt.Interface("IFoo")
        t2.interfaces.append(i)
        # add an interface that uses the unresolved interface
        # as a return value in a method.
        p = xpt.Param(xpt.InterfaceType(i))
        m = xpt.Method("ReturnIface", p)
        t2.interfaces.append(xpt.Interface("IRetval", iid="11111111-1111-1111-1111-111111111111",
                                           methods=[m]))
        t3 = xpt.xpt_link([t1, t2])
        
        self.assertEqual(2, len(t3.interfaces))
        self.assertEqual("IRetval", t3.interfaces[0].name)
        self.assertEqual("11111111-1111-1111-1111-111111111111", t3.interfaces[0].iid)
        self.assertEqual("IFoo", t3.interfaces[1].name)
        self.assertEqual("11223344-5566-7788-9900-aabbccddeeff", t3.interfaces[1].iid)
        self.assert_(t3.interfaces[1].resolved)
        # Ensure that IRetval's method's return value type has been updated.
        self.assertEqual(1, len(t3.interfaces[0].methods))
        self.assert_(t3.interfaces[0].methods[0].result.type.iface.resolved)
        self.assertEqual(t3.interfaces[1],
                         t3.interfaces[0].methods[0].result.type.iface)

    def test_mergeReplaceParams(self):
        """
        Test that merging an interface correctly updates InterfaceType
        params on methods of other interfaces.

        """
        # t1 has an unresolved interface and an interface that uses the
        # unresolved interface as a param value in a method. t2
        # has a resolved version of the unresolved interface.
        t1 = xpt.Typelib()
        # add an unresolved interface
        i = xpt.Interface("IFoo")
        t1.interfaces.append(i)
        # add an interface that uses the unresolved interface
        # as a param value in a method.
        vp = xpt.Param(xpt.SimpleType(xpt.Type.Tags.void))
        p = xpt.Param(xpt.InterfaceType(i))
        m = xpt.Method("IfaceParam", vp, params=[p])
        t1.interfaces.append(xpt.Interface("IParam", iid="11111111-1111-1111-1111-111111111111",
                                           methods=[m]))
        t2 = xpt.Typelib()
        # add a resolved interface
        m = xpt.Method("Bar", vp)
        t2.interfaces.append(xpt.Interface("IFoo", iid="11223344-5566-7788-9900-aabbccddeeff",
                                           methods=[m]))
        t3 = xpt.xpt_link([t1, t2])
        
        self.assertEqual(2, len(t3.interfaces))
        self.assertEqual("IParam", t3.interfaces[0].name)
        self.assertEqual("11111111-1111-1111-1111-111111111111", t3.interfaces[0].iid)
        self.assertEqual("IFoo", t3.interfaces[1].name)
        self.assertEqual("11223344-5566-7788-9900-aabbccddeeff", t3.interfaces[1].iid)
        self.assert_(t3.interfaces[1].resolved)
        # Ensure that IRetval's method's param type has been updated.
        self.assertEqual(1, len(t3.interfaces[0].methods))
        self.assert_(t3.interfaces[0].methods[0].params[0].type.iface.resolved)
        self.assertEqual(t3.interfaces[1],
                         t3.interfaces[0].methods[0].params[0].type.iface)

        # t1 has a resolved interface. t2 has an unresolved version
        # and an interface that uses the unresolved interface as a
        # param value in a method.
        t1 = xpt.Typelib()
        # add a resolved interface
        m = xpt.Method("Bar", vp)
        t1.interfaces.append(xpt.Interface("IFoo", iid="11223344-5566-7788-9900-aabbccddeeff",
                                           methods=[m]))
        t2 = xpt.Typelib()
        # add an unresolved interface
        i = xpt.Interface("IFoo")
        t2.interfaces.append(i)
        # add an interface that uses the unresolved interface
        # as a param value in a method.
        vp = xpt.Param(xpt.SimpleType(xpt.Type.Tags.void))
        p = xpt.Param(xpt.InterfaceType(i))
        m = xpt.Method("IfaceParam", vp, params=[p])
        t2.interfaces.append(xpt.Interface("IParam", iid="11111111-1111-1111-1111-111111111111",
                                           methods=[m]))
        t3 = xpt.xpt_link([t1, t2])
        
        self.assertEqual(2, len(t3.interfaces))
        self.assertEqual("IParam", t3.interfaces[0].name)
        self.assertEqual("11111111-1111-1111-1111-111111111111", t3.interfaces[0].iid)
        self.assertEqual("IFoo", t3.interfaces[1].name)
        self.assertEqual("11223344-5566-7788-9900-aabbccddeeff", t3.interfaces[1].iid)
        self.assert_(t3.interfaces[1].resolved)
        # Ensure that IRetval's method's param type has been updated.
        self.assertEqual(1, len(t3.interfaces[0].methods))
        self.assert_(t3.interfaces[0].methods[0].params[0].type.iface.resolved)
        self.assertEqual(t3.interfaces[1],
                         t3.interfaces[0].methods[0].params[0].type.iface)


    def test_mergeReplaceArrayTypeParams(self):
        """
        Test that merging an interface correctly updates ArrayType
        params whose element_type is an InterfaceType on methods
        of other interfaces.

        """
        # t1 has an unresolved interface and an interface that uses the
        # unresolved interface as a type in an ArrayType in a parameter
        # of a method. t2 has a resolved version of the unresolved interface.
        t1 = xpt.Typelib()
        # add an unresolved interface
        i = xpt.Interface("IFoo")
        t1.interfaces.append(i)
        # add an interface that uses the unresolved interface
        # as a type in an ArrayType in a param value in a method.
        vp = xpt.Param(xpt.SimpleType(xpt.Type.Tags.void))
        intp = xpt.Param(xpt.SimpleType(xpt.Type.Tags.int32))
        p = xpt.Param(xpt.ArrayType(xpt.InterfaceType(i), 1, 2))
        m = xpt.Method("ArrayIfaceParam", vp, params=[p, intp, intp])
        t1.interfaces.append(xpt.Interface("IParam", iid="11111111-1111-1111-1111-111111111111",
                                           methods=[m]))
        t2 = xpt.Typelib()
        # add a resolved interface
        m = xpt.Method("Bar", vp)
        t2.interfaces.append(xpt.Interface("IFoo", iid="11223344-5566-7788-9900-aabbccddeeff",
                                           methods=[m]))
        t3 = xpt.xpt_link([t1, t2])
        
        self.assertEqual(2, len(t3.interfaces))
        self.assertEqual("IParam", t3.interfaces[0].name)
        self.assertEqual("11111111-1111-1111-1111-111111111111", t3.interfaces[0].iid)
        self.assertEqual("IFoo", t3.interfaces[1].name)
        self.assertEqual("11223344-5566-7788-9900-aabbccddeeff", t3.interfaces[1].iid)
        self.assert_(t3.interfaces[1].resolved)
        # Ensure that IRetval's method's param type has been updated.
        self.assertEqual(1, len(t3.interfaces[0].methods))
        self.assert_(t3.interfaces[0].methods[0].params[0].type.element_type.iface.resolved)
        self.assertEqual(t3.interfaces[1],
                         t3.interfaces[0].methods[0].params[0].type.element_type.iface)

if __name__ == '__main__':
    mozunit.main()
