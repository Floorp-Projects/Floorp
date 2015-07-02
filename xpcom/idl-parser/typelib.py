#!/usr/bin/env python
# typelib.py - Generate XPCOM typelib files from IDL.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Generate an XPIDL typelib for the IDL files specified on the command line"""

import os
import sys
import xpidl
import xpt

# A map of xpidl.py types to xpt.py types
TypeMap = {
    # nsresult is not strictly an xpidl.py type, but it's useful here
    'nsresult':           xpt.Type.Tags.uint32,
    # builtins
    'boolean':            xpt.Type.Tags.boolean,
    'void':               xpt.Type.Tags.void,
    'int16_t':            xpt.Type.Tags.int16,
    'int32_t':            xpt.Type.Tags.int32,
    'int64_t':            xpt.Type.Tags.int64,
    'uint8_t':            xpt.Type.Tags.uint8,
    'uint16_t':           xpt.Type.Tags.uint16,
    'uint32_t':           xpt.Type.Tags.uint32,
    'uint64_t':           xpt.Type.Tags.uint64,
    'octet':              xpt.Type.Tags.uint8,
    'short':              xpt.Type.Tags.int16,
    'long':               xpt.Type.Tags.int32,
    'long long':          xpt.Type.Tags.int64,
    'unsigned short':     xpt.Type.Tags.uint16,
    'unsigned long':      xpt.Type.Tags.uint32,
    'unsigned long long': xpt.Type.Tags.uint64,
    'float':              xpt.Type.Tags.float,
    'double':             xpt.Type.Tags.double,
    'char':               xpt.Type.Tags.char,
    'string':             xpt.Type.Tags.char_ptr,
    'wchar':              xpt.Type.Tags.wchar_t,
    'wstring':            xpt.Type.Tags.wchar_t_ptr,
    # special types
    'nsid':               xpt.Type.Tags.nsIID,
    'domstring':          xpt.Type.Tags.DOMString,
    'astring':            xpt.Type.Tags.AString,
    'utf8string':         xpt.Type.Tags.UTF8String,
    'cstring':            xpt.Type.Tags.CString,
    'jsval':              xpt.Type.Tags.jsval
}


# XXXkhuey dipper types should go away (bug 677784)
def isDipperType(type):
    return type == xpt.Type.Tags.DOMString or type == xpt.Type.Tags.AString or type == xpt.Type.Tags.CString or type == xpt.Type.Tags.UTF8String


def build_interface(iface, ifaces):
    def get_type(type, calltype, iid_is=None, size_is=None):
        """ Return the appropriate xpt.Type object for this param """

        while isinstance(type, xpidl.Typedef):
            type = type.realtype

        if isinstance(type, xpidl.Builtin):
            if type.name == 'string' and size_is is not None:
                return xpt.StringWithSizeType(size_is, size_is)
            elif type.name == 'wstring' and size_is is not None:
                return xpt.WideStringWithSizeType(size_is, size_is)
            else:
                tag = TypeMap[type.name]
                isPtr = (tag == xpt.Type.Tags.char_ptr or tag == xpt.Type.Tags.wchar_t_ptr)
                return xpt.SimpleType(tag,
                                      pointer=isPtr,
                                      reference=False)

        if isinstance(type, xpidl.Array):
            # NB: For an Array<T> we pass down the iid_is to get the type of T.
            #     This allows Arrays of InterfaceIs types to work.
            return xpt.ArrayType(get_type(type.type, calltype, iid_is), size_is,
                                 #XXXkhuey length_is duplicates size_is (bug 677788),
                                 size_is)

        if isinstance(type, xpidl.Interface) or isinstance(type, xpidl.Forward):
            xptiface = None
            for i in ifaces:
                if i.name == type.name:
                    xptiface = i

            if not xptiface:
                xptiface = xpt.Interface(name=type.name)
                ifaces.append(xptiface)

            return xpt.InterfaceType(xptiface)

        if isinstance(type, xpidl.Native):
            if type.specialtype:
                # XXXkhuey jsval is marked differently in the typelib and in the headers :-(
                isPtr = (type.isPtr(calltype) or type.isRef(calltype)) and not type.specialtype == 'jsval'
                isRef = type.isRef(calltype) and not type.specialtype == 'jsval'
                return xpt.SimpleType(TypeMap[type.specialtype],
                                      pointer=isPtr,
                                      reference=isRef)
            elif iid_is is not None:
                return xpt.InterfaceIsType(iid_is)
            else:
                # void ptr
                return xpt.SimpleType(TypeMap['void'],
                                      pointer=True,
                                      reference=False)

        raise Exception("Unknown type!")

    def get_nsresult():
        return xpt.SimpleType(TypeMap['nsresult'])

    def build_nsresult_param():
        return xpt.Param(get_nsresult())

    def get_result_type(m):
        if not m.notxpcom:
            return get_nsresult()

        return get_type(m.realtype, '')

    def build_result_param(m):
        return xpt.Param(get_result_type(m))

    def build_retval_param(m):
        type = get_type(m.realtype, 'out')
        if isDipperType(type.tag):
            # NB: The retval bit needs to be set here, contrary to what the
            # xpt spec says.
            return xpt.Param(type, in_=True, retval=True, dipper=True)
        return xpt.Param(type, in_=False, out=True, retval=True)

    def build_attr_param(a, getter=False, setter=False):
        if not (getter or setter):
            raise Exception("Attribute param must be for a getter or a setter!")

        type = get_type(a.realtype, getter and 'out' or 'in')
        if setter:
            return xpt.Param(type)
        else:
            if isDipperType(type.tag):
                # NB: The retval bit needs to be set here, contrary to what the
                # xpt spec says.
                return xpt.Param(type, in_=True, retval=True, dipper=True)
            return xpt.Param(type, in_=False, out=True, retval=True)

    if iface.namemap is None:
        raise Exception("Interface was not resolved.")

    consts = []
    methods = []

    def build_const(c):
        consts.append(xpt.Constant(c.name, get_type(c.basetype, ''), c.getValue()))

    def build_method(m):
        params = []

        def build_param(p):
            def findattr(p, attr):
                if hasattr(p, attr) and getattr(p, attr):
                    for i, param in enumerate(m.params):
                        if param.name == getattr(p, attr):
                            return i
                    return None

            iid_is = findattr(p, 'iid_is')
            size_is = findattr(p, 'size_is')

            in_ = p.paramtype.count("in")
            out = p.paramtype.count("out")
            dipper = False
            type = get_type(p.realtype, p.paramtype, iid_is=iid_is, size_is=size_is)
            if out and isDipperType(type.tag):
                out = False
                dipper = True

            return xpt.Param(type, in_, out, p.retval, p.shared, dipper, p.optional)

        for p in m.params:
            params.append(build_param(p))

        if not m.notxpcom and m.realtype.name != 'void':
            params.append(build_retval_param(m))

        methods.append(xpt.Method(m.name, build_result_param(m), params,
                                  getter=False, setter=False, notxpcom=m.notxpcom,
                                  constructor=False, hidden=m.noscript,
                                  optargc=m.optional_argc,
                                  implicit_jscontext=m.implicit_jscontext))

    def build_attr(a):
        # Write the getter
        methods.append(xpt.Method(a.name, build_nsresult_param(),
                                  [build_attr_param(a, getter=True)],
                                  getter=True, setter=False,
                                  constructor=False, hidden=a.noscript,
                                  optargc=False,
                                  implicit_jscontext=a.implicit_jscontext))

        # And maybe the setter
        if not a.readonly:
            methods.append(xpt.Method(a.name, build_nsresult_param(),
                                      [build_attr_param(a, setter=True)],
                                      getter=False, setter=True,
                                      constructor=False, hidden=a.noscript,
                                      optargc=False,
                                      implicit_jscontext=a.implicit_jscontext))

    for member in iface.members:
        if isinstance(member, xpidl.ConstMember):
            build_const(member)
        elif isinstance(member, xpidl.Attribute):
            build_attr(member)
        elif isinstance(member, xpidl.Method):
            build_method(member)
        elif isinstance(member, xpidl.CDATA):
            pass
        else:
            raise Exception("Unexpected interface member: %s" % member)

    parent = None
    if iface.base:
        for i in ifaces:
            if i.name == iface.base:
                parent = i
        if not parent:
            parent = xpt.Interface(name=iface.base)
            ifaces.append(parent)

    return xpt.Interface(iface.name, iface.attributes.uuid, methods=methods,
                         constants=consts, resolved=True, parent=parent,
                         scriptable=iface.attributes.scriptable,
                         function=iface.attributes.function,
                         builtinclass=iface.attributes.builtinclass,
                         main_process_scriptable_only=iface.attributes.main_process_scriptable_only)


def write_typelib(idl, fd, filename):
    """ Generate the typelib. """

    # We only care about interfaces
    ifaces = []
    for p in idl.productions:
        if p.kind == 'interface':
            ifaces.append(build_interface(p, ifaces))

    typelib = xpt.Typelib(interfaces=ifaces)
    typelib.writefd(fd)

if __name__ == '__main__':
    from optparse import OptionParser
    o = OptionParser()
    o.add_option('-I', action='append', dest='incdirs', default=['.'],
                 help="Directory to search for imported files")
    o.add_option('--cachedir', dest='cachedir', default=None,
                 help="Directory in which to cache lex/parse tables.")
    o.add_option('-o', dest='outfile', default=None,
                 help="Output file")
    o.add_option('-d', dest='depfile', default=None,
                 help="Generate a make dependency file")
    o.add_option('--regen', action='store_true', dest='regen', default=False,
                 help="Regenerate IDL Parser cache")
    options, args = o.parse_args()
    file = args[0] if args else None

    if options.cachedir is not None:
        if not os.path.isdir(options.cachedir):
            os.mkdir(options.cachedir)
        sys.path.append(options.cachedir)

    if options.regen:
        if options.cachedir is None:
            print >>sys.stderr, "--regen requires --cachedir"
            sys.exit(1)

        p = xpidl.IDLParser(outputdir=options.cachedir, regen=True)
        sys.exit(0)

    if options.depfile is not None and options.outfile is None:
        print >>sys.stderr, "-d requires -o"
        sys.exit(1)

    if options.outfile is not None:
        outfd = open(options.outfile, 'wb')
        closeoutfd = True
    else:
        raise "typelib generation requires an output file"

    p = xpidl.IDLParser(outputdir=options.cachedir)
    idl = p.parse(open(file).read(), filename=file)
    idl.resolve(options.incdirs, p)
    write_typelib(idl, outfd, file)

    if closeoutfd:
        outfd.close()

    if options.depfile is not None:
        depfd = open(options.depfile, 'w')
        deps = [dep.replace('\\', '/') for dep in idl.deps]

        print >>depfd, "%s: %s" % (options.outfile, " ".join(deps))
        for dep in deps:
            print >>depfd, "%s:" % dep
