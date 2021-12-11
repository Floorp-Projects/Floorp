#!/usr/bin/env python
# header.py - Generate C++ header files from IDL.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Print a C++ header file for the IDL files specified on the command line"""

from __future__ import absolute_import

import os.path
import re
from xpidl import xpidl
import itertools

printdoccomments = False

if printdoccomments:

    def printComments(fd, clist, indent):
        for c in clist:
            fd.write("%s%s\n" % (indent, c))


else:

    def printComments(fd, clist, indent):
        pass


def firstCap(str):
    return str[0].upper() + str[1:]


def attributeParamName(a):
    return "a" + firstCap(a.name)


def attributeParamNames(a, getter, return_param=True):
    if getter and (a.notxpcom or not return_param):
        l = []
    else:
        l = [attributeParamName(a)]
    if a.implicit_jscontext:
        l.insert(0, "cx")
    return ", ".join(l)


def attributeNativeName(a, getter):
    binaryname = a.binaryname is not None and a.binaryname or firstCap(a.name)
    return "%s%s" % (getter and "Get" or "Set", binaryname)


def attributeAttributes(a, getter):
    ret = ""

    if a.must_use:
        ret = "[[nodiscard]] " + ret

    # Ideally, we'd set MOZ_CAN_RUN_SCRIPT in the "scriptable and not
    # builtinclass" case too, so we'd just have memberCanRunScript() check
    # explicit_setter_can_run_script/explicit_setter_can_run_script and call it
    # here.  But that would likely require a fair amount of Gecko-side
    # annotation work.  See bug 1534292.
    if (a.explicit_getter_can_run_script and getter) or (
        a.explicit_setter_can_run_script and not getter
    ):
        ret = "MOZ_CAN_RUN_SCRIPT " + ret

    return ret


def attributeReturnType(a, getter, macro):
    """macro should be NS_IMETHOD or NS_IMETHODIMP"""
    # Pick the type to be returned from the getter/setter.
    if a.notxpcom:
        ret = a.realtype.nativeType("in").strip() if getter else "void"
    else:
        ret = "nsresult"

    # Set calling convention and virtual-ness
    if a.nostdcall:
        if macro == "NS_IMETHOD":
            # This is the declaration.
            ret = "virtual %s" % ret
    else:
        if ret == "nsresult":
            ret = macro
        else:
            ret = "%s_(%s)" % (macro, ret)

    return attributeAttributes(a, getter) + ret


def attributeParamlist(a, getter, return_param=True):
    if getter and (a.notxpcom or not return_param):
        l = []
    else:
        l = [
            "%s%s"
            % (a.realtype.nativeType(getter and "out" or "in"), attributeParamName(a))
        ]
    if a.implicit_jscontext:
        l.insert(0, "JSContext* cx")

    return ", ".join(l)


def attributeAsNative(a, getter, declType="NS_IMETHOD"):
    params = {
        "returntype": attributeReturnType(a, getter, declType),
        "binaryname": attributeNativeName(a, getter),
        "paramlist": attributeParamlist(a, getter),
    }
    return "%(returntype)s %(binaryname)s(%(paramlist)s)" % params


def methodNativeName(m):
    return m.binaryname is not None and m.binaryname or firstCap(m.name)


def methodAttributes(m):
    ret = ""

    if m.must_use:
        ret = "[[nodiscard]] " + ret

    # Ideally, we'd set MOZ_CAN_RUN_SCRIPT in the "scriptable and not
    # builtinclass" case too, so we'd just have memberCanRunScript() check
    # explicit_can_run_script and call it here.  But that would likely require
    # a fair amount of Gecko-side annotation work.  See bug 1534292.
    if m.explicit_can_run_script:
        ret = "MOZ_CAN_RUN_SCRIPT " + ret

    return ret


def methodReturnType(m, macro):
    """macro should be NS_IMETHOD or NS_IMETHODIMP"""
    if m.notxpcom:
        ret = m.realtype.nativeType("in").strip()
    else:
        ret = "nsresult"

    # Set calling convention and virtual-ness
    if m.nostdcall:
        if macro == "NS_IMETHOD":
            # This is the declaration
            ret = "virtual %s" % ret
    else:
        if ret == "nsresult":
            ret = macro
        else:
            ret = "%s_(%s)" % (macro, ret)

    return methodAttributes(m) + ret


def methodAsNative(m, declType="NS_IMETHOD"):
    return "%s %s(%s)" % (
        methodReturnType(m, declType),
        methodNativeName(m),
        paramlistAsNative(m),
    )


def paramlistAsNative(m, empty="void", return_param=True):
    l = [paramAsNative(p) for p in m.params]

    if m.implicit_jscontext:
        l.append("JSContext* cx")

    if m.optional_argc:
        l.append("uint8_t _argc")

    if not m.notxpcom and m.realtype.name != "void" and return_param:
        l.append(
            paramAsNative(
                xpidl.Param(
                    paramtype="out",
                    type=None,
                    name="_retval",
                    attlist=[],
                    location=None,
                    realtype=m.realtype,
                )
            )
        )

    # Set any optional out params to default to nullptr. Skip if we just added
    # extra non-optional args to l.
    if len(l) == len(m.params):
        paramIter = len(m.params) - 1
        while (
            paramIter >= 0
            and m.params[paramIter].optional
            and m.params[paramIter].paramtype == "out"
        ):
            t = m.params[paramIter].type
            # Strings can't be optional, so this shouldn't happen, but let's make sure:
            if t == "AString" or t == "ACString" or t == "AUTF8String":
                break
            l[paramIter] += " = nullptr"
            paramIter -= 1

    if len(l) == 0:
        return empty

    return ", ".join(l)


def memberCanRunScript(member):
    # This can only happen if the member is scriptable and its interface is not builtinclass.
    return member.isScriptable() and not member.iface.attributes.builtinclass


def runScriptAnnotation(member):
    return "JS_HAZ_CAN_RUN_SCRIPT " if memberCanRunScript(member) else ""


def paramAsNative(p):
    default_spec = ""
    if p.default_value:
        default_spec = " = " + p.default_value
    return "%s%s%s" % (p.nativeType(), p.name, default_spec)


def paramlistNames(m, return_param=True):
    names = [p.name for p in m.params]

    if m.implicit_jscontext:
        names.append("cx")

    if m.optional_argc:
        names.append("_argc")

    if not m.notxpcom and m.realtype.name != "void" and return_param:
        names.append("_retval")

    if len(names) == 0:
        return ""
    return ", ".join(names)


header = """/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM $SRCDIR/%(relpath)s
 */

#ifndef __gen_%(basename)s_h__
#define __gen_%(basename)s_h__
"""

include = """
#ifndef __gen_%(basename)s_h__
#include "%(basename)s.h"
#endif
"""

jsvalue_include = """
#include "js/Value.h"
"""

infallible_includes = """
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
"""

can_run_script_includes = """
#include "js/GCAnnotations.h"
"""

header_end = """/* For IDL files that don't want to include root IDL files. */
#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif
"""

footer = """
#endif /* __gen_%(basename)s_h__ */
"""

forward_decl = """class %(name)s; /* forward declaration */

"""


def idl_basename(f):
    """returns the base name of a file with the last extension stripped"""
    return os.path.basename(f).rpartition(".")[0]


def print_header(idl, fd, filename, relpath):
    fd.write(header % {"relpath": relpath, "basename": idl_basename(filename)})

    foundinc = False
    for inc in idl.includes():
        if not foundinc:
            foundinc = True
            fd.write("\n")
        fd.write(include % {"basename": idl_basename(inc.filename)})

    if idl.needsJSTypes():
        fd.write(jsvalue_include)

    # Include some extra files if any attributes are infallible.
    interfaces = [p for p in idl.productions if p.kind == "interface"]
    wroteRunScriptIncludes = False
    wroteInfallibleIncludes = False
    for iface in interfaces:
        for member in iface.members:
            if not isinstance(member, xpidl.Attribute) and not isinstance(
                member, xpidl.Method
            ):
                continue
            if not wroteInfallibleIncludes and member.infallible:
                fd.write(infallible_includes)
                wroteInfallibleIncludes = True
            if not wroteRunScriptIncludes and memberCanRunScript(member):
                fd.write(can_run_script_includes)
                wroteRunScriptIncludes = True
        if wroteRunScriptIncludes and wroteInfallibleIncludes:
            break

    fd.write("\n")
    fd.write(header_end)

    for p in idl.productions:
        if p.kind == "include":
            continue
        if p.kind == "cdata":
            fd.write(p.data)
            continue

        if p.kind == "webidl":
            write_webidl(p, fd)
            continue
        if p.kind == "forward":
            fd.write(forward_decl % {"name": p.name})
            continue
        if p.kind == "interface":
            write_interface(p, fd)
            continue
        if p.kind == "typedef":
            printComments(fd, p.doccomments, "")
            fd.write("typedef %s %s;\n\n" % (p.realtype.nativeType("in"), p.name))

    fd.write(footer % {"basename": idl_basename(filename)})


def write_webidl(p, fd):
    path = p.native.split("::")
    for seg in path[:-1]:
        fd.write("namespace %s {\n" % seg)
    fd.write("class %s; /* webidl %s */\n" % (path[-1], p.name))
    for seg in reversed(path[:-1]):
        fd.write("} // namespace %s\n" % seg)
    fd.write("\n")


iface_header = r"""
/* starting interface:    %(name)s */
#define %(defname)s_IID_STR "%(iid)s"

#define %(defname)s_IID \
  {0x%(m0)s, 0x%(m1)s, 0x%(m2)s, \
    { %(m3joined)s }}

"""

uuid_decoder = re.compile(
    r"""(?P<m0>[a-f0-9]{8})-
                              (?P<m1>[a-f0-9]{4})-
                              (?P<m2>[a-f0-9]{4})-
                              (?P<m3>[a-f0-9]{4})-
                              (?P<m4>[a-f0-9]{12})$""",
    re.X,
)

iface_prolog = """ {
 public:

  NS_DECLARE_STATIC_IID_ACCESSOR(%(defname)s_IID)

"""

iface_scriptable = """\
  /* Used by ToJSValue to check which scriptable interface is implemented. */
  using ScriptableInterfaceType = %(name)s;

"""

iface_epilog = """};

  NS_DEFINE_STATIC_IID_ACCESSOR(%(name)s, %(defname)s_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_%(macroname)s """

iface_nonvirtual = """

/* Use this macro when declaring the members of this interface when the
   class doesn't implement the interface. This is useful for forwarding. */
#define NS_DECL_NON_VIRTUAL_%(macroname)s """

iface_forward = """

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_%(macroname)s(_to) """  # NOQA: E501

iface_forward_safe = """

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_%(macroname)s(_to) """  # NOQA: E501

builtin_infallible_tmpl = """\
  %(attributes)sinline %(realtype)s %(nativename)s(%(args)s)
  {
    %(realtype)sresult;
    mozilla::DebugOnly<nsresult> rv = %(nativename)s(%(argnames)s&result);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    return result;
  }
"""

# NOTE: We don't use RefPtr::forget here because we don't want to need the
# definition of %(realtype)s in scope, which we would need for the
# AddRef/Release calls.
refcnt_infallible_tmpl = """\
  %(attributes)s inline already_AddRefed<%(realtype)s> %(nativename)s(%(args)s)
  {
    %(realtype)s* result = nullptr;
    mozilla::DebugOnly<nsresult> rv = %(nativename)s(%(argnames)s&result);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    return already_AddRefed<%(realtype)s>(result);
  }
"""


def infallibleDecl(member):
    isattr = isinstance(member, xpidl.Attribute)
    ismethod = isinstance(member, xpidl.Method)
    assert isattr or ismethod

    realtype = member.realtype.nativeType("in")
    tmpl = builtin_infallible_tmpl

    if member.realtype.kind != "builtin" and member.realtype.kind != "cenum":
        assert realtype.endswith(" *"), "bad infallible type"
        tmpl = refcnt_infallible_tmpl
        realtype = realtype[:-2]  # strip trailing pointer

    if isattr:
        nativename = attributeNativeName(member, getter=True)
        args = attributeParamlist(member, getter=True, return_param=False)
        argnames = attributeParamNames(member, getter=True, return_param=False)
        attributes = attributeAttributes(member, getter=True)
    else:
        nativename = methodNativeName(member)
        args = paramlistAsNative(member, return_param=False)
        argnames = paramlistNames(member, return_param=False)
        attributes = methodAttributes(member)

    return tmpl % {
        "attributes": attributes,
        "realtype": realtype,
        "nativename": nativename,
        "args": args,
        "argnames": argnames + ", " if argnames else "",
    }


def write_interface(iface, fd):
    if iface.namemap is None:
        raise Exception("Interface was not resolved.")

    # Confirm that no names of methods will overload in this interface
    names = set()

    def record_name(name):
        if name in names:
            raise Exception(
                "Unexpected overloaded virtual method %s in interface %s"
                % (name, iface.name)
            )
        names.add(name)

    for m in iface.members:
        if type(m) == xpidl.Attribute:
            record_name(attributeNativeName(m, getter=True))
            if not m.readonly:
                record_name(attributeNativeName(m, getter=False))
        elif type(m) == xpidl.Method:
            record_name(methodNativeName(m))

    def write_const_decls(g):
        fd.write("  enum {\n")
        enums = []
        for c in g:
            printComments(fd, c.doccomments, "  ")
            basetype = c.basetype
            value = c.getValue()
            enums.append(
                "    %(name)s = %(value)s%(signed)s"
                % {
                    "name": c.name,
                    "value": value,
                    "signed": (not basetype.signed) and "U" or "",
                }
            )
        fd.write(",\n".join(enums))
        fd.write("\n  };\n\n")

    def write_cenum_decl(b):
        fd.write("  enum %s : uint%d_t {\n" % (b.basename, b.width))
        for var in b.variants:
            fd.write("    %s = %s,\n" % (var.name, var.value))
        fd.write("  };\n\n")

    def write_method_decl(m):
        printComments(fd, m.doccomments, "  ")

        fd.write("  /* %s */\n" % m.toIDL())
        fd.write("  %s%s = 0;\n\n" % (runScriptAnnotation(m), methodAsNative(m)))

        if m.infallible:
            fd.write(infallibleDecl(m))

    def write_attr_decl(a):
        printComments(fd, a.doccomments, "  ")

        fd.write("  /* %s */\n" % a.toIDL())

        fd.write("  %s%s = 0;\n" % (runScriptAnnotation(a), attributeAsNative(a, True)))
        if a.infallible:
            fd.write(infallibleDecl(a))

        if not a.readonly:
            fd.write(
                "  %s%s = 0;\n" % (runScriptAnnotation(a), attributeAsNative(a, False))
            )
        fd.write("\n")

    defname = iface.name.upper()
    if iface.name[0:2] == "ns":
        defname = "NS_" + defname[2:]

    names = uuid_decoder.match(iface.attributes.uuid).groupdict()
    m3str = names["m3"] + names["m4"]
    names["m3joined"] = ", ".join(["0x%s" % m3str[i : i + 2] for i in range(0, 16, 2)])

    if iface.name[2] == "I":
        implclass = iface.name[:2] + iface.name[3:]
    else:
        implclass = "_MYCLASS_"

    names.update(
        {
            "defname": defname,
            "macroname": iface.name.upper(),
            "name": iface.name,
            "iid": iface.attributes.uuid,
            "implclass": implclass,
        }
    )

    fd.write(iface_header % names)

    printComments(fd, iface.doccomments, "")

    fd.write("class ")
    foundcdata = False
    for m in iface.members:
        if isinstance(m, xpidl.CDATA):
            foundcdata = True

    if not foundcdata:
        fd.write("NS_NO_VTABLE ")

    fd.write(iface.name)
    if iface.base:
        fd.write(" : public %s" % iface.base)
    fd.write(iface_prolog % names)

    if iface.attributes.scriptable:
        fd.write(iface_scriptable % names)

    for key, group in itertools.groupby(iface.members, key=type):
        if key == xpidl.ConstMember:
            write_const_decls(group)  # iterator of all the consts
        else:
            for member in group:
                if key == xpidl.Attribute:
                    write_attr_decl(member)
                elif key == xpidl.Method:
                    write_method_decl(member)
                elif key == xpidl.CDATA:
                    fd.write(" %s" % member.data)
                elif key == xpidl.CEnum:
                    write_cenum_decl(member)
                else:
                    raise Exception("Unexpected interface member: %s" % member)

    fd.write(iface_epilog % names)

    def writeDeclaration(fd, iface, virtual):
        declType = "NS_IMETHOD" if virtual else "nsresult"
        suffix = " override" if virtual else ""
        for member in iface.members:
            if isinstance(member, xpidl.Attribute):
                if member.infallible:
                    fd.write(
                        "\\\n  using %s::%s; "
                        % (iface.name, attributeNativeName(member, True))
                    )
                fd.write(
                    "\\\n  %s%s; " % (attributeAsNative(member, True, declType), suffix)
                )
                if not member.readonly:
                    fd.write(
                        "\\\n  %s%s; "
                        % (attributeAsNative(member, False, declType), suffix)
                    )
            elif isinstance(member, xpidl.Method):
                fd.write("\\\n  %s%s; " % (methodAsNative(member, declType), suffix))
        if len(iface.members) == 0:
            fd.write("\\\n  /* no methods! */")
        elif member.kind not in ("attribute", "method"):
            fd.write("\\")

    writeDeclaration(fd, iface, True)
    fd.write(iface_nonvirtual % names)
    writeDeclaration(fd, iface, False)
    fd.write(iface_forward % names)

    def emitTemplate(forward_infallible, tmpl, tmpl_notxpcom=None):
        if tmpl_notxpcom is None:
            tmpl_notxpcom = tmpl
        for member in iface.members:
            if isinstance(member, xpidl.Attribute):
                if forward_infallible and member.infallible:
                    fd.write(
                        "\\\n  using %s::%s; "
                        % (iface.name, attributeNativeName(member, True))
                    )
                attr_tmpl = tmpl_notxpcom if member.notxpcom else tmpl
                fd.write(
                    attr_tmpl
                    % {
                        "asNative": attributeAsNative(member, True),
                        "nativeName": attributeNativeName(member, True),
                        "paramList": attributeParamNames(member, True),
                    }
                )
                if not member.readonly:
                    fd.write(
                        attr_tmpl
                        % {
                            "asNative": attributeAsNative(member, False),
                            "nativeName": attributeNativeName(member, False),
                            "paramList": attributeParamNames(member, False),
                        }
                    )
            elif isinstance(member, xpidl.Method):
                if member.notxpcom:
                    fd.write(
                        tmpl_notxpcom
                        % {
                            "asNative": methodAsNative(member),
                            "nativeName": methodNativeName(member),
                            "paramList": paramlistNames(member),
                        }
                    )
                else:
                    fd.write(
                        tmpl
                        % {
                            "asNative": methodAsNative(member),
                            "nativeName": methodNativeName(member),
                            "paramList": paramlistNames(member),
                        }
                    )
        if len(iface.members) == 0:
            fd.write("\\\n  /* no methods! */")
        elif member.kind not in ("attribute", "method"):
            fd.write("\\")

    emitTemplate(
        True,
        "\\\n  %(asNative)s override { return _to %(nativeName)s(%(paramList)s); } ",
    )

    fd.write(iface_forward_safe % names)

    # Don't try to safely forward notxpcom functions, because we have no
    # sensible default error return.  Instead, the caller will have to
    # implement them.
    emitTemplate(
        False,
        "\\\n  %(asNative)s override { return !_to ? NS_ERROR_NULL_POINTER : _to->%(nativeName)s(%(paramList)s); } ",  # NOQA: E501
        "\\\n  %(asNative)s override; ",
    )

    fd.write("\n\n")


def main(outputfile):
    xpidl.IDLParser()


if __name__ == "__main__":
    main(None)
