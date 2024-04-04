#!/usr/bin/env python
# typescript.py - Collect .d.json TypeScript info from xpidl.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json

import mozpack.path as mozpath

from xpidl import xpidl


def ts_enum(e):
    variants = [{"name": v.name, "value": v.getValue()} for v in e.variants]
    return {"id": e.basename, "variants": variants}


def ts_attribute(a):
    return {"name": a.name, "type": a.realtype.tsType(), "readonly": a.readonly}


def ts_method(m):
    args = []
    for p in m.params:
        if p.iid_is and not p.retval:
            raise xpidl.TSNoncompat(f"{m.name} has unsupported iid_is argument")
        args.append({"name": p.name, "optional": p.optional, "type": p.tsType()})

    iid_is = None
    type = m.realtype.tsType()
    if args and m.params[-1].retval:
        type = args.pop()["type"]
        iid_is = m.params[-1].iid_is

    return {"name": m.name, "type": type, "iid_is": iid_is, "args": args}


def ts_interface(iface):
    enums = []
    consts = []
    members = []

    for m in iface.members:
        try:
            if isinstance(m, xpidl.CEnum):
                enums.append(ts_enum(m))
            elif isinstance(m, xpidl.ConstMember):
                consts.append({"name": m.name, "value": m.getValue()})
            elif isinstance(m, xpidl.Attribute) and m.isScriptable():
                members.append(ts_attribute(m))
            elif isinstance(m, xpidl.Method) and m.isScriptable():
                members.append(ts_method(m))
        except xpidl.TSNoncompat:
            # Omit member if any type is unsupported.
            pass

    return {
        "id": iface.name,
        "base": iface.base,
        "callable": iface.attributes.function,
        "enums": enums,
        "consts": consts,
        "members": members,
    }


def ts_typedefs(idl):
    for p in idl.getNames():
        if isinstance(p, xpidl.Typedef):
            try:
                yield (p.name, p.realtype.tsType())
            except xpidl.TSNoncompat:
                pass


def ts_source(idl):
    """Collect typescript interface .d.json from a source idl file."""
    root = mozpath.join(mozpath.dirname(__file__), "../../..")
    return {
        "path": mozpath.relpath(idl.productions[0].location._file, root),
        "interfaces": [
            ts_interface(p)
            for p in idl.productions
            if isinstance(p, xpidl.Interface) and p.attributes.scriptable
        ],
        "typedefs": sorted(ts_typedefs(idl)),
    }


def write(d_json, fd):
    """Write json type info into fd"""
    json.dump(d_json, fd, indent=2, sort_keys=True)
