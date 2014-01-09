#!/usr/bin/env python
# header.py - Generate C++ header files from IDL.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Print a C++ header file for the IDL files specified on the command line"""

import sys, os.path, re, xpidl, itertools, glob

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

def attributeParamNames(a):
    l = [attributeParamName(a)]
    if a.implicit_jscontext:
        l.insert(0, "cx")
    return ", ".join(l)

def attributeNativeName(a, getter):
    binaryname = a.binaryname is not None and a.binaryname or firstCap(a.name)
    return "%s%s" % (getter and 'Get' or 'Set', binaryname)

def attributeReturnType(a, macro):
    """macro should be NS_IMETHOD or NS_IMETHODIMP"""
    if (a.nostdcall):
        return macro == "NS_IMETHOD" and "virtual nsresult" or "nsresult"
    else:
        return macro

def attributeParamlist(a, getter):
    l = ["%s%s" % (a.realtype.nativeType(getter and 'out' or 'in'),
                   attributeParamName(a))]
    if a.implicit_jscontext:
        l.insert(0, "JSContext* cx")

    return ", ".join(l)

def attributeAsNative(a, getter):
        deprecated = a.deprecated and "NS_DEPRECATED " or ""
        params = {'deprecated': deprecated,
                  'returntype': attributeReturnType(a, 'NS_IMETHOD'),
                  'binaryname': attributeNativeName(a, getter),
                  'paramlist': attributeParamlist(a, getter)}
        return "%(deprecated)s%(returntype)s %(binaryname)s(%(paramlist)s)" % params

def methodNativeName(m):
    return m.binaryname is not None and m.binaryname or firstCap(m.name)

def methodReturnType(m, macro):
    """macro should be NS_IMETHOD or NS_IMETHODIMP"""
    if m.nostdcall and m.notxpcom:
        return "%s%s" % (macro == "NS_IMETHOD" and "virtual " or "",
                         m.realtype.nativeType('in').strip())
    elif m.nostdcall:
        return "%snsresult" % (macro == "NS_IMETHOD" and "virtual " or "")
    elif m.notxpcom:
        return "%s_(%s)" % (macro, m.realtype.nativeType('in').strip())
    else:
        return macro

def methodAsNative(m):
    return "%s %s(%s)" % (methodReturnType(m, 'NS_IMETHOD'),
                          methodNativeName(m),
                          paramlistAsNative(m))

def paramlistAsNative(m, empty='void'):
    l = [paramAsNative(p) for p in m.params]

    if m.implicit_jscontext:
        l.append("JSContext* cx")

    if m.optional_argc:
        l.append('uint8_t _argc')

    if not m.notxpcom and m.realtype.name != 'void':
        l.append(paramAsNative(xpidl.Param(paramtype='out',
                                           type=None,
                                           name='_retval',
                                           attlist=[],
                                           location=None,
                                           realtype=m.realtype)))

    if len(l) == 0:
        return empty

    return ", ".join(l)

def paramAsNative(p):
    return "%s%s" % (p.nativeType(),
                     p.name)

def paramlistNames(m):
    names = [p.name for p in m.params]

    if m.implicit_jscontext:
        names.append('cx')

    if m.optional_argc:
        names.append('_argc')

    if not m.notxpcom and m.realtype.name != 'void':
        names.append('_retval')

    if len(names) == 0:
        return ''
    return ', '.join(names)

header = """/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM %(filename)s
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
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
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
    return os.path.basename(f).rpartition('.')[0]

def print_header(idl, fd, filename):
    fd.write(header % {'filename': filename,
                       'basename': idl_basename(filename)})

    foundinc = False
    for inc in idl.includes():
        if not foundinc:
            foundinc = True
            fd.write('\n')
        fd.write(include % {'basename': idl_basename(inc.filename)})

    if idl.needsJSTypes():
        fd.write(jsvalue_include)

    # Include some extra files if any attributes are infallible.
    for iface in [p for p in idl.productions if p.kind == 'interface']:
        for attr in [m for m in iface.members if isinstance(m, xpidl.Attribute)]:
            if attr.infallible:
                fd.write(infallible_includes)
                break

    fd.write('\n')
    fd.write(header_end)

    for p in idl.productions:
        if p.kind == 'include': continue
        if p.kind == 'cdata':
            fd.write(p.data)
            continue

        if p.kind == 'forward':
            fd.write(forward_decl % {'name': p.name})
            continue
        if p.kind == 'interface':
            write_interface(p, fd)
            continue
        if p.kind == 'typedef':
            printComments(fd, p.doccomments, '')
            fd.write("typedef %s %s;\n\n" % (p.realtype.nativeType('in'),
                                             p.name))

    fd.write(footer % {'basename': idl_basename(filename)})

iface_header = r"""
/* starting interface:    %(name)s */
#define %(defname)s_IID_STR "%(iid)s"

#define %(defname)s_IID \
  {0x%(m0)s, 0x%(m1)s, 0x%(m2)s, \
    { %(m3joined)s }}

"""

uuid_decoder = re.compile(r"""(?P<m0>[a-f0-9]{8})-
                              (?P<m1>[a-f0-9]{4})-
                              (?P<m2>[a-f0-9]{4})-
                              (?P<m3>[a-f0-9]{4})-
                              (?P<m4>[a-f0-9]{12})$""", re.X)

iface_prolog = """ {
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(%(defname)s_IID)

"""

iface_epilog = """};

  NS_DEFINE_STATIC_IID_ACCESSOR(%(name)s, %(defname)s_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_%(macroname)s """


iface_forward = """

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_%(macroname)s(_to) """

iface_forward_safe = """

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_%(macroname)s(_to) """

iface_template_prolog = """

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class %(implclass)s : public %(name)s
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_%(macroname)s

  %(implclass)s();

private:
  ~%(implclass)s();

protected:
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(%(implclass)s, %(name)s)

%(implclass)s::%(implclass)s()
{
  /* member initializers and constructor code */
}

%(implclass)s::~%(implclass)s()
{
  /* destructor code */
}

"""

example_tmpl = """%(returntype)s %(implclass)s::%(nativeName)s(%(paramList)s)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
"""

iface_template_epilog = """/* End of implementation class template. */
#endif

"""

attr_infallible_tmpl = """\
  inline %(realtype)s%(nativename)s(%(args)s)
  {
    %(realtype)sresult;
    mozilla::DebugOnly<nsresult> rv = %(nativename)s(%(argnames)s&result);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    return result;
  }
"""

def write_interface(iface, fd):
    if iface.namemap is None:
        raise Exception("Interface was not resolved.")

    def write_const_decls(g):
        fd.write("  enum {\n")
        enums = []
        for c in g:
            printComments(fd, c.doccomments, '  ')
            basetype = c.basetype
            value = c.getValue()
            enums.append("    %(name)s = %(value)s%(signed)s" % {
                         'name': c.name,
                         'value': value,
                         'signed': (not basetype.signed) and 'U' or ''})
        fd.write(",\n".join(enums))
        fd.write("\n  };\n\n")

    def write_method_decl(m):
        printComments(fd, m.doccomments, '  ')

        fd.write("  /* %s */\n" % m.toIDL())
        fd.write("  %s = 0;\n\n" % methodAsNative(m))
                                                                           
    def write_attr_decl(a):
        printComments(fd, a.doccomments, '  ')

        fd.write("  /* %s */\n" % a.toIDL());

        fd.write("  %s = 0;\n" % attributeAsNative(a, True))
        if a.infallible:
            fd.write(attr_infallible_tmpl %
                     {'realtype': a.realtype.nativeType('in'),
                      'nativename': attributeNativeName(a, getter=True),
                      'args': '' if not a.implicit_jscontext else 'JSContext* cx',
                      'argnames': '' if not a.implicit_jscontext else 'cx, '})

        if not a.readonly:
            fd.write("  %s = 0;\n" % attributeAsNative(a, False))
        fd.write("\n")

    defname = iface.name.upper()
    if iface.name[0:2] == 'ns':
        defname = 'NS_' + defname[2:]

    names = uuid_decoder.match(iface.attributes.uuid).groupdict()
    m3str = names['m3'] + names['m4']
    names['m3joined'] = ", ".join(["0x%s" % m3str[i:i+2] for i in xrange(0, 16, 2)])

    if iface.name[2] == 'I':
        implclass = iface.name[:2] + iface.name[3:]
    else:
        implclass = '_MYCLASS_'

    names.update({'defname': defname,
                  'macroname': iface.name.upper(),
                  'name': iface.name,
                  'iid': iface.attributes.uuid,
                  'implclass': implclass})

    fd.write(iface_header % names)

    printComments(fd, iface.doccomments, '')

    fd.write("class ")
    foundcdata = False
    for m in iface.members:
        if isinstance(m, xpidl.CDATA):
            foundcdata = True

    if not foundcdata:
        fd.write("NS_NO_VTABLE ")

    if iface.attributes.deprecated:
        fd.write("MOZ_DEPRECATED ")
    fd.write(iface.name)
    if iface.base:
        fd.write(" : public %s" % iface.base)
    fd.write(iface_prolog % names)

    for key, group in itertools.groupby(iface.members, key=type):
        if key == xpidl.ConstMember:
            write_const_decls(group) # iterator of all the consts
        else:
            for member in group:
                if key == xpidl.Attribute:
                    write_attr_decl(member)
                elif key == xpidl.Method:
                    write_method_decl(member)
                elif key == xpidl.CDATA:
                    fd.write(" %s" % member.data)
                else:
                    raise Exception("Unexpected interface member: %s" % member)

    fd.write(iface_epilog % names)

    for member in iface.members:
        if isinstance(member, xpidl.Attribute):
            fd.write("\\\n  %s; " % attributeAsNative(member, True))
            if not member.readonly:
                fd.write("\\\n  %s; " % attributeAsNative(member, False))
        elif isinstance(member, xpidl.Method):
            fd.write("\\\n  %s; " % methodAsNative(member))
    if len(iface.members) == 0:
        fd.write('\\\n  /* no methods! */')
    elif not member.kind in ('attribute', 'method'):
       fd.write('\\')

    fd.write(iface_forward % names)

    def emitTemplate(tmpl, tmpl_notxpcom=None):
        if tmpl_notxpcom == None:
            tmpl_notxpcom = tmpl
        for member in iface.members:
            if isinstance(member, xpidl.Attribute):
                fd.write(tmpl % {'asNative': attributeAsNative(member, True),
                                 'nativeName': attributeNativeName(member, True),
                                 'paramList': attributeParamNames(member)})
                if not member.readonly:
                    fd.write(tmpl % {'asNative': attributeAsNative(member, False),
                                     'nativeName': attributeNativeName(member, False),
                                     'paramList': attributeParamNames(member)})
            elif isinstance(member, xpidl.Method):
                if member.notxpcom:
                    fd.write(tmpl_notxpcom % {'asNative': methodAsNative(member),
                                              'nativeName': methodNativeName(member),
                                              'paramList': paramlistNames(member)})
                else:
                    fd.write(tmpl % {'asNative': methodAsNative(member),
                                     'nativeName': methodNativeName(member),
                                     'paramList': paramlistNames(member)})
        if len(iface.members) == 0:
            fd.write('\\\n  /* no methods! */')
        elif not member.kind in ('attribute', 'method'):
            fd.write('\\')

    emitTemplate("\\\n  %(asNative)s { return _to %(nativeName)s(%(paramList)s); } ")

    fd.write(iface_forward_safe % names)

    # Don't try to safely forward notxpcom functions, because we have no
    # sensible default error return.  Instead, the caller will have to
    # implement them.
    emitTemplate("\\\n  %(asNative)s { return !_to ? NS_ERROR_NULL_POINTER : _to->%(nativeName)s(%(paramList)s); } ",
                 "\\\n  %(asNative)s; ")

    fd.write(iface_template_prolog % names)

    for member in iface.members:
        if isinstance(member, xpidl.ConstMember) or isinstance(member, xpidl.CDATA): continue
        fd.write("/* %s */\n" % member.toIDL())
        if isinstance(member, xpidl.Attribute):
            fd.write(example_tmpl % {'implclass': implclass,
                                     'returntype': attributeReturnType(member, 'NS_IMETHODIMP'),
                                     'nativeName': attributeNativeName(member, True),
                                     'paramList': attributeParamlist(member, True)})
            if not member.readonly:
                fd.write(example_tmpl % {'implclass': implclass,
                                         'returntype': attributeReturnType(member, 'NS_IMETHODIMP'),
                                         'nativeName': attributeNativeName(member, False),
                                         'paramList': attributeParamlist(member, False)})
        elif isinstance(member, xpidl.Method):
            fd.write(example_tmpl % {'implclass': implclass,
                                     'returntype': methodReturnType(member, 'NS_IMETHODIMP'),
                                     'nativeName': methodNativeName(member),
                                     'paramList': paramlistAsNative(member, empty='')})
        fd.write('\n')

    fd.write(iface_template_epilog)

if __name__ == '__main__':
    from optparse import OptionParser
    o = OptionParser()
    o.add_option('-I', action='append', dest='incdirs', default=['.'],
                 help="Directory to search for imported files")
    o.add_option('--cachedir', dest='cachedir', default=None,
                 help="Directory in which to cache lex/parse tables.")
    o.add_option('-o', dest='outfile', default=None,
                 help="Output file (default is stdout)")
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

    # The only thing special about a regen is that there are no input files.
    if options.regen:
        if options.cachedir is None:
            print >>sys.stderr, "--regen useless without --cachedir"
        # Delete the lex/yacc files.  Ply is too stupid to regenerate them
        # properly
        for fileglobs in [os.path.join(options.cachedir, f) for f in ["xpidllex.py*", "xpidlyacc.py*"]]:
            for filename in glob.glob(fileglobs):
                os.remove(filename)

    # Instantiate the parser.
    p = xpidl.IDLParser(outputdir=options.cachedir)

    if options.regen:
        sys.exit(0)

    if options.depfile is not None and options.outfile is None:
        print >>sys.stderr, "-d requires -o"
        sys.exit(1)

    if options.outfile is not None:
        outfd = open(options.outfile, 'w')
        closeoutfd = True
    else:
        outfd = sys.stdout
        closeoutfd = False

    idl = p.parse(open(file).read(), filename=file)
    idl.resolve(options.incdirs, p)
    print_header(idl, outfd, file)

    if closeoutfd:
        outfd.close()

    if options.depfile is not None:
        dirname = os.path.dirname(options.depfile)
        if dirname:
            try:
                os.makedirs(dirname)
            except:
                pass
        depfd = open(options.depfile, 'w')
        deps = [dep.replace('\\', '/') for dep in idl.deps]

        print >>depfd, "%s: %s" % (options.outfile, " ".join(deps))
        for dep in deps:
            print >>depfd, "%s:" % dep
