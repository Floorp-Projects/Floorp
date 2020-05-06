# rust_macros.py - Generate rust_macros bindings from IDL.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Generate rust bindings information for the IDL file specified"""

from __future__ import absolute_import

from xpidl import rust
from xpidl import xpidl


derive_method_tmpl = """\
Method {
    name: "%(name)s",
    params: &[%(params)s],
    ret: "%(ret)s",
}"""


def attrAsMethodStruct(iface, m, getter):
    params = ['Param { name: "%s", ty: "%s" }' % x
              for x in rust.attributeRawParamList(iface, m, getter)]
    return derive_method_tmpl % {
        'name': rust.attributeNativeName(m, getter),
        'params': ', '.join(params),
        'ret': '::nserror::nsresult',
    }


def methodAsMethodStruct(iface, m):
    params = ['Param { name: "%s", ty: "%s" }' % x
              for x in rust.methodRawParamList(iface, m)]
    return derive_method_tmpl % {
        'name': rust.methodNativeName(m),
        'params': ', '.join(params),
        'ret': rust.methodReturnType(m),
    }


derive_iface_tmpl = """\
Interface {
    name: "%(name)s",
    base: %(base)s,
    methods: %(methods)s,
},
"""


def write_interface(iface, fd):
    if iface.namemap is None:
        raise Exception("Interface was not resolved.")

    assert iface.base or (iface.name == "nsISupports")

    base = 'Some("%s")' % iface.base if iface.base is not None else 'None'
    try:
        methods = ""
        for member in iface.members:
            if type(member) == xpidl.Attribute:
                methods += "/* %s */\n" % member.toIDL()
                methods += "%s,\n" % attrAsMethodStruct(iface, member, True)
                if not member.readonly:
                    methods += "%s,\n" % attrAsMethodStruct(iface, member, False)
                methods += "\n"

            elif type(member) == xpidl.Method:
                methods += "/* %s */\n" % member.toIDL()
                methods += "%s,\n\n" % methodAsMethodStruct(iface, member)
        fd.write(derive_iface_tmpl % {
            'name': iface.name,
            'base': base,
            'methods': 'Ok(&[\n%s])' % methods,
        })
    except xpidl.RustNoncompat as reason:
        fd.write(derive_iface_tmpl % {
            'name': iface.name,
            'base': base,
            'methods': 'Err("%s")' % reason,
        })


header = """\
//
// DO NOT EDIT.  THIS FILE IS GENERATED FROM $SRCDIR/%(relpath)s
//

"""


def print_rust_macros_bindings(idl, fd, relpath):
    fd = rust.AutoIndent(fd)

    fd.write(header % {'relpath': relpath})
    fd.write("{static D: &'static [Interface] = &[\n")

    for p in idl.productions:
        if p.kind == 'interface':
            write_interface(p, fd)

    fd.write("]; D}\n")
