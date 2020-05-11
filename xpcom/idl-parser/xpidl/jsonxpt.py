#!/usr/bin/env python
# jsonxpt.py - Generate json XPT typelib files from IDL.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Generate a json XPT typelib for an IDL file"""

from __future__ import absolute_import

from xpidl import xpidl
import json
import itertools

# A map of xpidl.py types to xpt enum variants
TypeMap = {
    # builtins
    'boolean':            'TD_BOOL',
    'void':               'TD_VOID',
    'int16_t':            'TD_INT16',
    'int32_t':            'TD_INT32',
    'int64_t':            'TD_INT64',
    'uint8_t':            'TD_UINT8',
    'uint16_t':           'TD_UINT16',
    'uint32_t':           'TD_UINT32',
    'uint64_t':           'TD_UINT64',
    'octet':              'TD_UINT8',
    'short':              'TD_INT16',
    'long':               'TD_INT32',
    'long long':          'TD_INT64',
    'unsigned short':     'TD_UINT16',
    'unsigned long':      'TD_UINT32',
    'unsigned long long': 'TD_UINT64',
    'float':              'TD_FLOAT',
    'double':             'TD_DOUBLE',
    'char':               'TD_CHAR',
    'string':             'TD_PSTRING',
    'wchar':              'TD_WCHAR',
    'wstring':            'TD_PWSTRING',
    # special types
    'nsid':               'TD_NSID',
    'astring':            'TD_ASTRING',
    'utf8string':         'TD_UTF8STRING',
    'cstring':            'TD_CSTRING',
    'jsval':              'TD_JSVAL',
    'promise':            'TD_PROMISE',
}


def flags(*flags):
    return [flag for flag, cond in flags if cond]


def get_type(type, calltype, iid_is=None, size_is=None):
    while isinstance(type, xpidl.Typedef):
        type = type.realtype

    if isinstance(type, xpidl.Builtin):
        ret = {'tag': TypeMap[type.name]}
        if type.name in ['string', 'wstring'] and size_is is not None:
            ret['tag'] += '_SIZE_IS'
            ret['size_is'] = size_is
        return ret

    if isinstance(type, xpidl.Array):
        # NB: For a Array<T> we pass down the iid_is to get the type of T.
        #     This allows Arrays of InterfaceIs types to work.
        return {
            'tag': 'TD_ARRAY',
            'element': get_type(type.type, calltype, iid_is),
        }

    if isinstance(type, xpidl.LegacyArray):
        # NB: For a Legacy [array] T we pass down iid_is to get the type of T.
        #     This allows [array] of InterfaceIs types to work.
        return {
            'tag': 'TD_LEGACY_ARRAY',
            'size_is': size_is,
            'element': get_type(type.type, calltype, iid_is),
        }

    if isinstance(type, xpidl.Interface) or isinstance(type, xpidl.Forward):
        return {
            'tag': 'TD_INTERFACE_TYPE',
            'name': type.name,
        }

    if isinstance(type, xpidl.WebIDL):
        return {
            'tag': 'TD_DOMOBJECT',
            'name': type.name,
            'native': type.native,
            'headerFile': type.headerFile,
        }

    if isinstance(type, xpidl.Native):
        if type.specialtype == 'nsid' and type.isPtr(calltype):
            return {'tag': 'TD_NSIDPTR'}
        elif type.specialtype:
            return {
                'tag': TypeMap[type.specialtype]
            }
        elif iid_is is not None:
            return {
                'tag': 'TD_INTERFACE_IS_TYPE',
                'iid_is': iid_is,
            }
        else:
            return {'tag': 'TD_VOID'}

    if isinstance(type, xpidl.CEnum):
        # As far as XPConnect is concerned, cenums are just unsigned integers.
        return {'tag': 'TD_UINT%d' % type.width}

    raise Exception("Unknown type!")


def mk_param(type, in_=0, out=0, optional=0):
    return {
        'type': type,
        'flags': flags(
            ('in', in_),
            ('out', out),
            ('optional', optional),
        ),
    }


def mk_method(method, params, getter=0, setter=0, optargc=0, hasretval=0, symbol=0):
    return {
        'name': method.name,
        # NOTE: We don't include any return value information here, as we'll
        # never call the methods if they're marked notxpcom, and all xpcom
        # methods return the same type (nsresult).
        # XXX: If we ever use these files for other purposes than xptcodegen we
        # may want to write that info.
        'params': params,
        'flags': flags(
            ('getter', getter),
            ('setter', setter),
            ('hidden', method.noscript or method.notxpcom),
            ('optargc', optargc),
            ('jscontext', method.implicit_jscontext),
            ('hasretval', hasretval),
            ('symbol', method.symbol),
        ),
    }


def attr_param_idx(p, m, attr):
    if hasattr(p, attr) and getattr(p, attr):
        for i, param in enumerate(m.params):
            if param.name == getattr(p, attr):
                return i
        return None


def build_interface(iface):
    if iface.namemap is None:
        raise Exception("Interface was not resolved.")

    assert iface.attributes.scriptable, "Don't generate XPT info for non-scriptable interfaces"

    # State used while building an interface
    consts = []
    methods = []

    def build_const(c):
        consts.append({
            'name': c.name,
            'type': get_type(c.basetype, ''),
            'value': c.getValue(),  # All of our consts are numbers
        })

    def build_cenum(b):
        for var in b.variants:
            consts.append({
                'name': var.name,
                'type': get_type(b, 'in'),
                'value': var.value,
            })

    def build_method(m):
        params = []
        for p in m.params:
            params.append(mk_param(
                get_type(
                    p.realtype, p.paramtype,
                    iid_is=attr_param_idx(p, m, 'iid_is'),
                    size_is=attr_param_idx(p, m, 'size_is')),
                in_=p.paramtype.count("in"),
                out=p.paramtype.count("out"),
                optional=p.optional,
            ))

        hasretval = len(m.params) > 0 and m.params[-1].retval
        if not m.notxpcom and m.realtype.name != 'void':
            hasretval = True
            params.append(mk_param(get_type(m.realtype, 'out'), out=1))

        methods.append(mk_method(m, params, optargc=m.optional_argc,
                                 hasretval=hasretval))

    def build_attr(a):
        assert a.realtype.name != 'void'
        # Write the getter
        getter_params = []
        if not a.notxpcom:
            getter_params.append(mk_param(get_type(a.realtype, 'out'), out=1))

        methods.append(mk_method(a, getter_params, getter=1, hasretval=1))

        # And maybe the setter
        if not a.readonly:
            param = mk_param(get_type(a.realtype, 'in'), in_=1)
            methods.append(mk_method(a, [param], setter=1))

    for member in iface.members:
        if isinstance(member, xpidl.ConstMember):
            build_const(member)
        elif isinstance(member, xpidl.Attribute):
            build_attr(member)
        elif isinstance(member, xpidl.Method):
            build_method(member)
        elif isinstance(member, xpidl.CEnum):
            build_cenum(member)
        elif isinstance(member, xpidl.CDATA):
            pass
        else:
            raise Exception("Unexpected interface member: %s" % member)

    return {
        'name': iface.name,
        'uuid': iface.attributes.uuid,
        'methods': methods,
        'consts': consts,
        'parent': iface.base,
        'flags': flags(
            ('function', iface.attributes.function),
            ('builtinclass', iface.attributes.builtinclass),
            ('main_process_only', iface.attributes.main_process_scriptable_only),
        )
    }


# These functions are the public interface of this module. They are very simple
# functions, but are exported so that if we need to do something more
# complex in them in the future we can.

def build_typelib(idl):
    """Given a parsed IDL file, generate and return the typelib"""
    return [build_interface(p) for p in idl.productions
            if p.kind == 'interface' and p.attributes.scriptable]


def link(typelibs):
    """Link a list of typelibs together into a single typelib"""
    linked = list(itertools.chain.from_iterable(typelibs))
    assert len(set(iface['name'] for iface in linked)) == len(linked), \
        "Multiple typelibs containing the same interface were linked together"
    return linked


def write(typelib, fd):
    """Write typelib into fd"""
    json.dump(typelib, fd, indent=2, sort_keys=True)
