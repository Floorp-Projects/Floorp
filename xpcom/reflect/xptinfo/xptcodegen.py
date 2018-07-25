#!/usr/bin/env python
# jsonlink.py - Merge JSON typelib files into a .cpp file
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# NOTE: Once shims are removed, this code can be cleaned up, removing all
# reference to them.

import json
from perfecthash import PerfectHash
from collections import OrderedDict

# We fix the number of entries in our intermediate table used by the perfect
# hashes to 512. This number is constant in xptinfo, allowing the compiler to
# generate a more efficient modulo due to it being a power of 2.
PHFSIZE = 512


def indented(s):
    return s.replace('\n', '\n  ')


def cpp(v):
    if type(v) == bool:
        return "true" if v else "false"
    return str(v)


def mkstruct(*fields):
    def mk(comment, **vals):
        assert len(fields) == len(vals)
        r = "{ // " + comment
        r += indented(','.join(
            "\n/* %s */ %s" % (k, cpp(vals[k])) for k in fields))
        r += "\n}"
        return r
    return mk


##########################################################
# Ensure these fields are in the same order as xptinfo.h #
##########################################################
nsXPTInterfaceInfo = mkstruct(
    "mIID",
    "mName",
    "mParent",
    "mBuiltinClass",
    "mMainProcessScriptableOnly",
    "mMethods",
    "mConsts",
    "mIsShim",
    "mFunction",
    "mNumMethods",
    "mNumConsts",
)

##########################################################
# Ensure these fields are in the same order as xptinfo.h #
##########################################################
nsXPTType = mkstruct(
    "mTag",
    "mInParam",
    "mOutParam",
    "mOptionalParam",
    "mData1",
    "mData2",
)

##########################################################
# Ensure these fields are in the same order as xptinfo.h #
##########################################################
nsXPTParamInfo = mkstruct(
    "mType",
)

##########################################################
# Ensure these fields are in the same order as xptinfo.h #
##########################################################
nsXPTMethodInfo = mkstruct(
    "mName",
    "mParams",
    "mNumParams",
    "mGetter",
    "mSetter",
    "mNotXPCOM",
    "mHidden",
    "mOptArgc",
    "mContext",
    "mHasRetval",
)

##########################################################
# Ensure these fields are in the same order as xptinfo.h #
##########################################################
nsXPTDOMObjectInfo = mkstruct(
    "mUnwrap",
    "mWrap",
    "mCleanup",
)

##########################################################
# Ensure these fields are in the same order as xptinfo.h #
##########################################################
ConstInfo = mkstruct(
    "mName",
    "mSigned",
    "mValue",
)


# Helper functions for dealing with IIDs.
#
# Unfortunately, the way we represent IIDs in memory depends on the endianness
# of the target architecture. We store an nsIID as a 16-byte, 4-tuple of:
#
#   (uint32_t, uint16_t, uint16_t, [uint8_t; 8])
#
# Unfortunately, this means that when we hash the bytes of the nsIID on a
# little-endian target system, we need to hash them in little-endian order.
# These functions let us split the input hexadecimal string into components,
# encoding each as a little-endian value, and producing an accurate bytearray.
#
# It would be nice to have a consistent representation of IIDs in memory such
# that we don't have to do these gymnastics to get an accurate hash.

def split_at_idxs(s, lengths):
    idx = 0
    for length in lengths:
        yield s[idx:idx+length]
        idx += length
    assert idx == len(s)


def split_iid(iid):  # Get the individual components out of an IID string.
    iid = iid.replace('-', '')  # Strip any '-' delimiters
    return tuple(split_at_idxs(iid, (8, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2)))


def iid_bytes(iid):  # Get the byte representation of the IID for hashing.
    bs = bytearray()
    for num in split_iid(iid):
        b = bytearray.fromhex(num)
        # We store the bytes in little-endian. On big-endian systems, the C++
        # code will flip the bytes to little-endian before hashing in order to
        # keep the tables consistent.
        b.reverse()
        bs += b
    return bs

# Split a 16-bit integer into its high and low 8 bits


def splitint(i):
    assert i < 2**16
    return (i >> 8, i & 0xff)


# Occasionally in xpconnect, we need to fabricate types to pass into the
# conversion methods. In some cases, these types need to be arrays, which hold
# indicies into the extra types array.
#
# These are some types which should have known indexes into the extra types
# array.
utility_types = [
    {'tag': 'TD_INT8'},
    {'tag': 'TD_UINT8'},
    {'tag': 'TD_INT16'},
    {'tag': 'TD_UINT16'},
    {'tag': 'TD_INT32'},
    {'tag': 'TD_UINT32'},
    {'tag': 'TD_INT64'},
    {'tag': 'TD_UINT64'},
    {'tag': 'TD_FLOAT'},
    {'tag': 'TD_DOUBLE'},
    {'tag': 'TD_BOOL'},
    {'tag': 'TD_CHAR'},
    {'tag': 'TD_WCHAR'},
    {'tag': 'TD_PNSIID'},
    {'tag': 'TD_PSTRING'},
    {'tag': 'TD_PWSTRING'},
    {'tag': 'TD_INTERFACE_IS_TYPE', 'iid_is': 0},
]


# Core of the code generator. Takes a list of raw JSON XPT interfaces, and
# writes out a file containing the necessary static declarations into fd.
def link_to_cpp(interfaces, fd):
    # Perfect Hash from IID into the ifaces array.
    iid_phf = PerfectHash(PHFSIZE, [
        (iid_bytes(iface['uuid']), iface)
        for iface in interfaces
    ])
    # Perfect Hash from name to index in the ifaces array.
    name_phf = PerfectHash(PHFSIZE, [
        (bytearray(iface['name'], 'ascii'), idx)
        for idx, iface in enumerate(iid_phf.values)
    ])

    def interface_idx(name):
        if name is not None:
            idx = name_phf.lookup(bytearray(name, 'ascii'))
            if iid_phf.values[idx]['name'] == name:
                return idx + 1  # One-based, so we can use 0 as a sentinel.
        return 0

    # NOTE: State used while linking. This is done with closures rather than a
    # class due to how this file's code evolved.
    includes = set()
    types = []
    type_cache = {}
    ifaces = []
    params = []
    param_cache = {}
    methods = []
    consts = []
    prophooks = []
    domobjects = []
    domobject_cache = {}
    strings = OrderedDict()

    def lower_uuid(uuid):
        return ("{0x%s, 0x%s, 0x%s, {0x%s, 0x%s, 0x%s, 0x%s, 0x%s, 0x%s, 0x%s, 0x%s}}" %
                split_iid(uuid))

    def lower_domobject(do):
        assert do['tag'] == 'TD_DOMOBJECT'

        idx = domobject_cache.get(do['name'])
        if idx is None:
            idx = domobject_cache[do['name']] = len(domobjects)

            includes.add(do['headerFile'])
            domobjects.append(nsXPTDOMObjectInfo(
                "%d = %s" % (idx, do['name']),
                # These methods are defined at the top of the generated file.
                mUnwrap="UnwrapDOMObject<mozilla::dom::prototypes::id::%s, %s>" %
                (do['name'], do['native']),
                mWrap="WrapDOMObject<%s>" % do['native'],
                mCleanup="CleanupDOMObject<%s>" % do['native'],
            ))

        return idx

    def lower_string(s):
        if s in strings:
            # We've already seen this string.
            return strings[s]
        elif len(strings):
            # Get the last string we inserted (should be O(1) on OrderedDict).
            last_s = next(reversed(strings))
            strings[s] = strings[last_s] + len(last_s) + 1
        else:
            strings[s] = 0
        return strings[s]

    def lower_extra_type(type):
        key = describe_type(type)
        idx = type_cache.get(key)
        if idx is None:
            idx = type_cache[key] = len(types)
            types.append(lower_type(type))
        return idx

    def describe_type(type):  # Create the type's documentation comment.
        tag = type['tag'][3:].lower()
        if tag == 'array':
            return '%s[size_is=%d]' % (
                describe_type(type['element']), type['size_is'])
        elif tag == 'interface_type' or tag == 'domobject':
            return type['name']
        elif tag == 'interface_is_type':
            return 'iid_is(%d)' % type['iid_is']
        elif tag.endswith('_size_is'):
            return '%s(size_is=%d)' % (tag, type['size_is'])
        return tag

    def lower_type(type, in_=False, out=False, optional=False):
        tag = type['tag']
        d1 = d2 = 0

        if tag == 'TD_ARRAY':
            d1 = type['size_is']
            d2 = lower_extra_type(type['element'])

        elif tag == 'TD_INTERFACE_TYPE':
            d1, d2 = splitint(interface_idx(type['name']))

        elif tag == 'TD_INTERFACE_IS_TYPE':
            d1 = type['iid_is']

        elif tag == 'TD_DOMOBJECT':
            d1, d2 = splitint(lower_domobject(type))

        elif tag.endswith('_SIZE_IS'):
            d1 = type['size_is']

        assert d1 < 256 and d2 < 256, "Data values too large"
        return nsXPTType(
            describe_type(type),
            mTag=tag,
            mData1=d1,
            mData2=d2,
            mInParam=in_,
            mOutParam=out,
            mOptionalParam=optional,
        )

    def lower_param(param, paramname):
        params.append(nsXPTParamInfo(
            "%d = %s" % (len(params), paramname),
            mType=lower_type(param['type'],
                             in_='in' in param['flags'],
                             out='out' in param['flags'],
                             optional='optional' in param['flags'])
        ))

    def lower_method(method, ifacename):
        methodname = "%s::%s" % (ifacename, method['name'])

        if 'notxpcom' in method['flags'] or 'hidden' in method['flags']:
            paramidx = name = numparams = 0  # hide parameters
        else:
            name = lower_string(method['name'])
            numparams = len(method['params'])

            # Check cache for parameters
            cachekey = json.dumps(method['params'])
            paramidx = param_cache.get(cachekey)
            if paramidx is None:
                paramidx = param_cache[cachekey] = len(params)
                for idx, param in enumerate(method['params']):
                    lower_param(param, "%s[%d]" % (methodname, idx))

        methods.append(nsXPTMethodInfo(
            "%d = %s" % (len(methods), methodname),

            # If our method is hidden, we can save some memory by not
            # generating parameter info about it.
            mName=name,
            mParams=paramidx,
            mNumParams=numparams,

            # Flags
            mGetter='getter' in method['flags'],
            mSetter='setter' in method['flags'],
            mNotXPCOM='notxpcom' in method['flags'],
            mHidden='hidden' in method['flags'],
            mOptArgc='optargc' in method['flags'],
            mContext='jscontext' in method['flags'],
            mHasRetval='hasretval' in method['flags'],
        ))

    def lower_const(const, ifacename):
        assert const['type']['tag'] in \
            ['TD_INT16', 'TD_INT32', 'TD_UINT16', 'TD_UINT32']
        is_signed = const['type']['tag'] in ['TD_INT16', 'TD_INT32']

        # Constants are always either signed or unsigned 16 or 32 bit integers,
        # which we will only need to convert to JS values. To save on space,
        # don't bother storing the type, and instead just store a 32-bit
        # unsigned integer, and stash whether to interpret it as signed.
        consts.append(ConstInfo(
            "%d = %s::%s" % (len(consts), ifacename, const['name']),

            mName=lower_string(const['name']),
            mSigned=is_signed,
            mValue="(uint32_t)%d" % const['value'],
        ))

    def lower_prop_hooks(iface):  # XXX: Used by xpt shims
        assert iface['shim'] is not None

        # Add an include for the Binding file for the shim.
        includes.add("mozilla/dom/%sBinding.h" %
                     (iface['shimfile'] or iface['shim']))

        # Add the property hook reference to the sPropHooks table.
        prophooks.append(
            "mozilla::dom::%s_Binding::sNativePropertyHooks, // %d = %s(%s)" %
            (iface['shim'], len(prophooks), iface['name'], iface['shim']))

    def collect_base_info(iface):
        methods = 0
        consts = 0
        while iface is not None:
            methods += len(iface['methods'])
            consts += len(iface['consts'])
            idx = interface_idx(iface['parent'])
            if idx == 0:
                break
            iface = iid_phf.values[idx - 1]

        return methods, consts

    def lower_iface(iface):
        isshim = iface['shim'] is not None
        assert isshim or 'scriptable' in iface['flags']

        method_off = len(methods)
        consts_off = len(consts)
        method_cnt = const_cnt = 0
        if isshim:
            # If we are looking at a shim, don't lower any methods or constants,
            # as they will be pulled from the WebIDL binding instead. Instead,
            # we use the constants offset field to store the index into the prop
            # hooks table.
            consts_off = len(prophooks)
        else:
            method_cnt, const_cnt = collect_base_info(iface)

        # The number of maximum methods is not arbitrary. It is the same value
        # as in xpcom/reflect/xptcall/genstubs.pl; do not change this value
        # without changing that one or you WILL see problems.
        #
        # In addition, mNumMethods and mNumConsts are stored as a 8-bit ints,
        # meaning we cannot exceed 255 methods/consts on any interface.
        assert method_cnt < 250, "%s has too many methods" % iface['name']
        assert const_cnt < 256, "%s has too many constants" % iface['name']

        ifaces.append(nsXPTInterfaceInfo(
            "%d = %s" % (len(ifaces), iface['name']),

            mIID=lower_uuid(iface['uuid']),
            mName=lower_string(iface['name']),
            mParent=interface_idx(iface['parent']),

            mMethods=method_off,
            mNumMethods=method_cnt,
            mConsts=consts_off,
            mNumConsts=const_cnt,

            # Flags
            mIsShim=isshim,
            mBuiltinClass='builtinclass' in iface['flags'],
            mMainProcessScriptableOnly='main_process_only' in iface['flags'],
            mFunction='function' in iface['flags'],
        ))

        if isshim:
            lower_prop_hooks(iface)
            return

        # Lower the methods and constants used by this interface
        for method in iface['methods']:
            lower_method(method, iface['name'])
        for const in iface['consts']:
            lower_const(const, iface['name'])

    # Lower the types which have fixed indexes first, and check that the indexes
    # seem correct.
    for expected, ty in enumerate(utility_types):
        got = lower_extra_type(ty)
        assert got == expected, "Wrong index when lowering"

    # Lower interfaces in the order of the IID phf's values lookup.
    for iface in iid_phf.values:
        lower_iface(iface)

    # Write out the final output file
    fd.write("/* THIS FILE WAS GENERATED BY xptcodegen.py - DO NOT EDIT */\n\n")

    # Include any bindings files which we need to include due to XPT shims.
    for include in includes:
        fd.write('#include "%s"\n' % include)

    # Write out our header
    fd.write("""
#include "xptinfo.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/dom/BindingUtils.h"

// These template methods are specialized to be used in the sDOMObjects table.
template<mozilla::dom::prototypes::ID PrototypeID, typename T>
static nsresult UnwrapDOMObject(JS::HandleValue aHandle, void** aObj)
{
  RefPtr<T> p;
  nsresult rv = mozilla::dom::UnwrapObject<PrototypeID, T>(aHandle, p);
  p.forget(aObj);
  return rv;
}

template<typename T>
static bool WrapDOMObject(JSContext* aCx, void* aObj, JS::MutableHandleValue aHandle)
{
  return mozilla::dom::GetOrCreateDOMReflector(aCx, reinterpret_cast<T*>(aObj), aHandle);
}

template<typename T>
static void CleanupDOMObject(void* aObj)
{
  RefPtr<T> p = already_AddRefed<T>(reinterpret_cast<T*>(aObj));
}

namespace xpt {
namespace detail {

""")

    # Static data arrays
    def array(ty, name, els):
        fd.write("const %s %s[] = {%s\n};\n\n" %
                 (ty, name, ','.join(indented('\n' + str(e)) for e in els)))
    array("nsXPTInterfaceInfo", "sInterfaces", ifaces)
    array("nsXPTType", "sTypes", types)
    array("nsXPTParamInfo", "sParams", params)
    array("nsXPTMethodInfo", "sMethods", methods)
    array("nsXPTDOMObjectInfo", "sDOMObjects", domobjects)
    array("ConstInfo", "sConsts", consts)
    array("mozilla::dom::NativePropertyHooks*", "sPropHooks", prophooks)

    # The strings array. We write out individual characters to avoid MSVC restrictions.
    fd.write("const char sStrings[] = {\n")
    for s, off in strings.iteritems():
        fd.write("  // %d = %s\n  '%s','\\0',\n" % (off, s, "','".join(s)))
    fd.write("};\n\n")

    # Record the information required for perfect hashing.
    # NOTE: Intermediates stored as 32-bit for safety. Shouldn't need >16-bit.
    def phfarr(name, ty, it):
        fd.write("const %s %s[] = {" % (ty, name))
        for idx, v in enumerate(it):
            if idx % 8 == 0:
                fd.write('\n ')
            fd.write(" 0x%04x," % v)
        fd.write("\n};\n\n")
    phfarr("sPHF_IIDs", "uint32_t", iid_phf.intermediate)
    phfarr("sPHF_Names", "uint32_t", name_phf.intermediate)
    phfarr("sPHF_NamesIdxs", "uint16_t", name_phf.values)

    # Generate some checks that the indexes for the utility types match the
    # declared ones in xptinfo.h
    for idx, ty in enumerate(utility_types):
        fd.write("static_assert(%d == (uint8_t)nsXPTType::Idx::%s, \"Bad idx\");\n" %
                 (idx, ty['tag'][3:]))

    # The footer contains some checks re: the size of the generated arrays.
    fd.write("""
const uint16_t sInterfacesSize = mozilla::ArrayLength(sInterfaces);
static_assert(sInterfacesSize == mozilla::ArrayLength(sPHF_NamesIdxs),
              "sPHF_NamesIdxs must have same size as sInterfaces");

static_assert(kPHFSize == mozilla::ArrayLength(sPHF_Names),
              "sPHF_IIDs must have size kPHFSize");
static_assert(kPHFSize == mozilla::ArrayLength(sPHF_IIDs),
              "sPHF_Names must have size kPHFSize");

} // namespace detail
} // namespace xpt
""")


def link_and_write(files, outfile):
    interfaces = []
    for file in files:
        with open(file, 'r') as fd:
            interfaces += json.load(fd)

    iids = set()
    names = set()
    for interface in interfaces:
        assert interface['uuid'] not in iids, "duplicated UUID %s" % interface['uuid']
        assert interface['name'] not in names, "duplicated name %s" % interface['name']
        iids.add(interface['uuid'])
        names.add(interface['name'])

    link_to_cpp(interfaces, outfile)


def main():
    from argparse import ArgumentParser
    import sys

    parser = ArgumentParser()
    parser.add_argument('outfile', help='Output C++ file to generate')
    parser.add_argument('xpts', nargs='*', help='source xpt files')

    args = parser.parse_args(sys.argv[1:])
    with open(args.outfile, 'w') as fd:
        link_and_write(args.xpts, fd)


if __name__ == '__main__':
    main()
