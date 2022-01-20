# rust.py - Generate rust bindings from IDL.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Print a runtime Rust bindings file for the IDL file specified"""

# --- Safety Hazards ---

# We currently don't generate some bindings for some IDL methods in rust code,
# due to there being ABI safety hazards if we were to do so. This is the
# documentation for the reasons why we don't generate certain types of bindings,
# so that we don't accidentally start generating them in the future.

# notxpcom methods and attributes return their results directly by value. The x86
# windows stdcall ABI returns aggregates by value differently for methods than
# functions, and rust only exposes the function ABI, so that's the one we're
# using. The correct ABI can be emulated for notxpcom methods returning aggregates
# by passing an &mut ReturnType parameter as the second parameter.  This strategy
# is used by the winapi-rs crate.
# https://github.com/retep998/winapi-rs/blob/7338a5216a6a7abeefcc6bb1bc34381c81d3e247/src/macros.rs#L220-L231
#
# Right now we can generate code for notxpcom methods and attributes, as we don't
# support passing aggregates by value over these APIs ever (the types which are
# allowed in xpidl.py shouldn't include any aggregates), so the code is
# correct. In the future if we want to start supporting returning aggregates by
# value, we will need to use a workaround such as the one used by winapi.rs.

# nostdcall methods on x86 windows will use the thiscall ABI, which is not
# stable in rust right now, so we cannot generate bindings to them.

# In general, passing C++ objects by value over the C ABI is not a good idea,
# and when possible we should avoid doing so. We don't generate bindings for
# these methods here currently.

from __future__ import absolute_import

import os.path
import re
from xpidl import xpidl


class AutoIndent(object):
    """A small autoindenting wrapper around a fd.
    Used to make the code output more readable."""

    def __init__(self, fd):
        self.fd = fd
        self.indent = 0

    def write(self, string):
        """A smart write function which automatically adjusts the
        indentation of each line as it is written by counting braces"""
        for s in string.split("\n"):
            s = s.strip()
            indent = self.indent
            if len(s) == 0:
                indent = 0
            elif s[0] == "}":
                indent -= 1

            self.fd.write("    " * indent + s + "\n")
            for c in s:
                if c == "(" or c == "{" or c == "[":
                    self.indent += 1
                elif c == ")" or c == "}" or c == "]":
                    self.indent -= 1


def rustSanitize(s):
    keywords = [
        "abstract",
        "alignof",
        "as",
        "become",
        "box",
        "break",
        "const",
        "continue",
        "crate",
        "do",
        "else",
        "enum",
        "extern",
        "false",
        "final",
        "fn",
        "for",
        "if",
        "impl",
        "in",
        "let",
        "loop",
        "macro",
        "match",
        "mod",
        "move",
        "mut",
        "offsetof",
        "override",
        "priv",
        "proc",
        "pub",
        "pure",
        "ref",
        "return",
        "Self",
        "self",
        "sizeof",
        "static",
        "struct",
        "super",
        "trait",
        "true",
        "type",
        "typeof",
        "unsafe",
        "unsized",
        "use",
        "virtual",
        "where",
        "while",
        "yield",
    ]
    if s in keywords:
        return s + "_"
    return s


# printdoccomments = False
printdoccomments = True

if printdoccomments:

    def printComments(fd, clist, indent):
        fd.write("%s%s" % (indent, doccomments(clist)))

    def doccomments(clist):
        if len(clist) == 0:
            return ""
        s = "/// ```text"
        for c in clist:
            for cc in c.splitlines():
                s += "\n/// " + cc
        s += "\n/// ```\n///\n"
        return s


else:

    def printComments(fd, clist, indent):
        pass

    def doccomments(clist):
        return ""


def firstCap(str):
    return str[0].upper() + str[1:]


# Attribute VTable Methods
def attributeNativeName(a, getter):
    binaryname = rustSanitize(a.binaryname if a.binaryname else firstCap(a.name))
    return "%s%s" % ("Get" if getter else "Set", binaryname)


def attributeReturnType(a, getter):
    if a.notxpcom:
        if getter:
            return a.realtype.rustType("in").strip()
        return "::libc::c_void"
    return "::nserror::nsresult"


def attributeParamName(a):
    return "a" + firstCap(a.name)


def attributeRawParamList(iface, a, getter):
    if getter and a.notxpcom:
        l = []
    else:
        l = [(attributeParamName(a), a.realtype.rustType("out" if getter else "in"))]
    if a.implicit_jscontext:
        raise xpidl.RustNoncompat("jscontext is unsupported")
    if a.nostdcall:
        raise xpidl.RustNoncompat("nostdcall is unsupported")
    return l


def attributeParamList(iface, a, getter):
    l = ["this: *const " + iface.name]
    l += ["%s: %s" % x for x in attributeRawParamList(iface, a, getter)]
    return ", ".join(l)


def attrAsVTableEntry(iface, m, getter):
    try:
        return 'pub %s: unsafe extern "system" fn (%s) -> %s' % (
            attributeNativeName(m, getter),
            attributeParamList(iface, m, getter),
            attributeReturnType(m, getter),
        )
    except xpidl.RustNoncompat as reason:
        return """\
/// Unable to generate binding because `%s`
pub %s: *const ::libc::c_void""" % (
            reason,
            attributeNativeName(m, getter),
        )


# Method VTable generation functions
def methodNativeName(m):
    binaryname = m.binaryname is not None and m.binaryname or firstCap(m.name)
    return rustSanitize(binaryname)


def methodReturnType(m):
    if m.notxpcom:
        return m.realtype.rustType("in").strip()
    return "::nserror::nsresult"


def methodRawParamList(iface, m):
    l = [(rustSanitize(p.name), p.rustType()) for p in m.params]

    if m.implicit_jscontext:
        raise xpidl.RustNoncompat("jscontext is unsupported")

    if m.optional_argc:
        raise xpidl.RustNoncompat("optional_argc is unsupported")

    if m.nostdcall:
        raise xpidl.RustNoncompat("nostdcall is unsupported")

    if not m.notxpcom and m.realtype.name != "void":
        l.append(("_retval", m.realtype.rustType("out")))

    return l


def methodParamList(iface, m):
    l = ["this: *const %s" % iface.name]
    l += ["%s: %s" % x for x in methodRawParamList(iface, m)]
    return ", ".join(l)


def methodAsVTableEntry(iface, m):
    try:
        return 'pub %s: unsafe extern "system" fn (%s) -> %s' % (
            methodNativeName(m),
            methodParamList(iface, m),
            methodReturnType(m),
        )
    except xpidl.RustNoncompat as reason:
        return """\
/// Unable to generate binding because `%s`
pub %s: *const ::libc::c_void""" % (
            reason,
            methodNativeName(m),
        )


method_impl_tmpl = """\
#[inline]
pub unsafe fn %(name)s(&self, %(params)s) -> %(ret_ty)s {
    ((*self.vtable).%(name)s)(self, %(args)s)
}
"""


def methodAsWrapper(iface, m):
    try:
        param_list = methodRawParamList(iface, m)
        params = ["%s: %s" % x for x in param_list]
        args = [x[0] for x in param_list]

        return method_impl_tmpl % {
            "name": methodNativeName(m),
            "params": ", ".join(params),
            "ret_ty": methodReturnType(m),
            "args": ", ".join(args),
        }
    except xpidl.RustNoncompat:
        # Dummy field for the doc comments to attach to.
        # Private so that it's not shown in rustdoc.
        return "const _%s: () = ();" % methodNativeName(m)


infallible_impl_tmpl = """\
#[inline]
pub unsafe fn %(name)s(&self) -> %(realtype)s {
    let mut result = <%(realtype)s as ::std::default::Default>::default();
    let _rv = ((*self.vtable).%(name)s)(self, &mut result);
    debug_assert!(_rv.succeeded());
    result
}
"""


def attrAsWrapper(iface, m, getter):
    try:
        if m.implicit_jscontext:
            raise xpidl.RustNoncompat("jscontext is unsupported")

        if m.nostdcall:
            raise xpidl.RustNoncompat("nostdcall is unsupported")

        name = attributeParamName(m)

        if getter and m.infallible and m.realtype.kind == "builtin":
            # NOTE: We don't support non-builtin infallible getters in Rust code.
            return infallible_impl_tmpl % {
                "name": attributeNativeName(m, getter),
                "realtype": m.realtype.rustType("in"),
            }

        param_list = attributeRawParamList(iface, m, getter)
        params = ["%s: %s" % x for x in param_list]
        return method_impl_tmpl % {
            "name": attributeNativeName(m, getter),
            "params": ", ".join(params),
            "ret_ty": attributeReturnType(m, getter),
            "args": "" if getter and m.notxpcom else name,
        }

    except xpidl.RustNoncompat:
        # Dummy field for the doc comments to attach to.
        # Private so that it's not shown in rustdoc.
        return "const _%s: () = ();" % attributeNativeName(m, getter)


header = """\
//
// DO NOT EDIT.  THIS FILE IS GENERATED FROM $SRCDIR/%(relpath)s
//

"""


def idl_basename(f):
    """returns the base name of a file with the last extension stripped"""
    return os.path.splitext(os.path.basename(f))[0]


def print_rust_bindings(idl, fd, relpath):
    fd = AutoIndent(fd)

    fd.write(header % {"relpath": relpath})

    # All of the idl files will be included into the same rust module, as we
    # can't do forward declarations. Because of this, we want to ignore all
    # import statements

    for p in idl.productions:
        if p.kind == "include" or p.kind == "cdata" or p.kind == "forward":
            continue

        if p.kind == "interface":
            write_interface(p, fd)
            continue

        if p.kind == "typedef":
            try:
                # We have to skip the typedef of bool to bool (it doesn't make any sense anyways)
                if p.name == "bool":
                    continue

                if printdoccomments:
                    fd.write(
                        "/// `typedef %s %s;`\n///\n"
                        % (p.realtype.nativeType("in"), p.name)
                    )
                    fd.write(doccomments(p.doccomments))
                fd.write("pub type %s = %s;\n\n" % (p.name, p.realtype.rustType("in")))
            except xpidl.RustNoncompat as reason:
                fd.write(
                    "/* unable to generate %s typedef because `%s` */\n\n"
                    % (p.name, reason)
                )


base_vtable_tmpl = """
/// We need to include the members from the base interface's vtable at the start
/// of the VTable definition.
pub __base: %sVTable,

"""


vtable_tmpl = """\
// This struct represents the interface's VTable. A pointer to a statically
// allocated version of this struct is at the beginning of every %(name)s
// object. It contains one pointer field for each method in the interface. In
// the case where we can't generate a binding for a method, we include a void
// pointer.
#[doc(hidden)]
#[repr(C)]
pub struct %(name)sVTable {%(base)s%(entries)s}

"""


# NOTE: This template is not generated for nsISupports, as it has no base interfaces.
deref_tmpl = """\
// Every interface struct type implements `Deref` to its base interface. This
// causes methods on the base interfaces to be directly avaliable on the
// object. For example, you can call `.AddRef` or `.QueryInterface` directly
// on any interface which inherits from `nsISupports`.
impl ::std::ops::Deref for %(name)s {
    type Target = %(base)s;
    #[inline]
    fn deref(&self) -> &%(base)s {
        unsafe {
            ::std::mem::transmute(self)
        }
    }
}

// Ensure we can use .coerce() to cast to our base types as well. Any type which
// our base interface can coerce from should be coercable from us as well.
impl<T: %(base)sCoerce> %(name)sCoerce for T {
    #[inline]
    fn coerce_from(v: &%(name)s) -> &Self {
        T::coerce_from(v)
    }
}
"""


struct_tmpl = """\
// The actual type definition for the interface. This struct has methods
// declared on it which will call through its vtable. You never want to pass
// this type around by value, always pass it behind a reference.

#[repr(C)]
pub struct %(name)s {
    vtable: *const %(name)sVTable,

    /// This field is a phantomdata to ensure that the VTable type and any
    /// struct containing it is not safe to send across threads, as XPCOM is
    /// generally not threadsafe.
    ///
    /// XPCOM interfaces in general are not safe to send across threads.
    __nosync: ::std::marker::PhantomData<::std::rc::Rc<u8>>,
}

// Implementing XpCom for an interface exposes its IID, which allows for easy
// use of the `.query_interface<T>` helper method. This also defines that
// method for %(name)s.
unsafe impl XpCom for %(name)s {
    const IID: nsIID = nsID(0x%(m0)s, 0x%(m1)s, 0x%(m2)s,
                            [%(m3joined)s]);
}

// We need to implement the RefCounted trait so we can be used with `RefPtr`.
// This trait teaches `RefPtr` how to manage our memory.
unsafe impl RefCounted for %(name)s {
    #[inline]
    unsafe fn addref(&self) {
        self.AddRef();
    }
    #[inline]
    unsafe fn release(&self) {
        self.Release();
    }
}

// This trait is implemented on all types which can be coerced to from %(name)s.
// It is used in the implementation of `fn coerce<T>`. We hide it from the
// documentation, because it clutters it up a lot.
#[doc(hidden)]
pub trait %(name)sCoerce {
    /// Cheaply cast a value of this type from a `%(name)s`.
    fn coerce_from(v: &%(name)s) -> &Self;
}

// The trivial implementation: We can obviously coerce ourselves to ourselves.
impl %(name)sCoerce for %(name)s {
    #[inline]
    fn coerce_from(v: &%(name)s) -> &Self {
        v
    }
}

impl %(name)s {
    /// Cast this `%(name)s` to one of its base interfaces.
    #[inline]
    pub fn coerce<T: %(name)sCoerce>(&self) -> &T {
        T::coerce_from(self)
    }
}
"""


wrapper_tmpl = """\
// The implementations of the function wrappers which are exposed to rust code.
// Call these methods rather than manually calling through the VTable struct.
impl %(name)s {
%(consts)s
%(methods)s
}

"""

vtable_entry_tmpl = """\
/* %(idl)s */
%(entry)s,
"""


const_wrapper_tmpl = """\
%(docs)s
pub const %(name)s: i64 = %(val)s;
"""


method_wrapper_tmpl = """\
%(docs)s
/// `%(idl)s`
%(wrapper)s
"""


uuid_decoder = re.compile(
    r"""(?P<m0>[a-f0-9]{8})-
                              (?P<m1>[a-f0-9]{4})-
                              (?P<m2>[a-f0-9]{4})-
                              (?P<m3>[a-f0-9]{4})-
                              (?P<m4>[a-f0-9]{12})$""",
    re.X,
)


def write_interface(iface, fd):
    if iface.namemap is None:
        raise Exception("Interface was not resolved.")

    assert iface.base or (iface.name == "nsISupports")

    # Extract the UUID's information so that it can be written into the struct definition
    names = uuid_decoder.match(iface.attributes.uuid).groupdict()
    m3str = names["m3"] + names["m4"]
    names["m3joined"] = ", ".join(["0x%s" % m3str[i : i + 2] for i in range(0, 16, 2)])
    names["name"] = iface.name

    if printdoccomments:
        if iface.base is not None:
            fd.write("/// `interface %s : %s`\n///\n" % (iface.name, iface.base))
        else:
            fd.write("/// `interface %s`\n///\n" % iface.name)
    printComments(fd, iface.doccomments, "")
    fd.write(struct_tmpl % names)

    if iface.base is not None:
        fd.write(
            deref_tmpl
            % {
                "name": iface.name,
                "base": iface.base,
            }
        )

    entries = []
    for member in iface.members:
        if type(member) == xpidl.Attribute:
            entries.append(
                vtable_entry_tmpl
                % {
                    "idl": member.toIDL(),
                    "entry": attrAsVTableEntry(iface, member, True),
                }
            )
            if not member.readonly:
                entries.append(
                    vtable_entry_tmpl
                    % {
                        "idl": member.toIDL(),
                        "entry": attrAsVTableEntry(iface, member, False),
                    }
                )

        elif type(member) == xpidl.Method:
            entries.append(
                vtable_entry_tmpl
                % {
                    "idl": member.toIDL(),
                    "entry": methodAsVTableEntry(iface, member),
                }
            )

    fd.write(
        vtable_tmpl
        % {
            "name": iface.name,
            "base": base_vtable_tmpl % iface.base if iface.base is not None else "",
            "entries": "\n".join(entries),
        }
    )

    # Get all of the constants
    consts = []
    for member in iface.members:
        if type(member) == xpidl.ConstMember:
            consts.append(
                const_wrapper_tmpl
                % {
                    "docs": doccomments(member.doccomments),
                    "name": member.name,
                    "val": member.getValue(),
                }
            )

    methods = []
    for member in iface.members:
        if type(member) == xpidl.Attribute:
            methods.append(
                method_wrapper_tmpl
                % {
                    "docs": doccomments(member.doccomments),
                    "idl": member.toIDL(),
                    "wrapper": attrAsWrapper(iface, member, True),
                }
            )
            if not member.readonly:
                methods.append(
                    method_wrapper_tmpl
                    % {
                        "docs": doccomments(member.doccomments),
                        "idl": member.toIDL(),
                        "wrapper": attrAsWrapper(iface, member, False),
                    }
                )

        elif type(member) == xpidl.Method:
            methods.append(
                method_wrapper_tmpl
                % {
                    "docs": doccomments(member.doccomments),
                    "idl": member.toIDL(),
                    "wrapper": methodAsWrapper(iface, member),
                }
            )

    fd.write(
        wrapper_tmpl
        % {
            "name": iface.name,
            "consts": "\n".join(consts),
            "methods": "\n".join(methods),
        }
    )
