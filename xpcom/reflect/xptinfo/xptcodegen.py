#!/usr/bin/env python
# jsonlink.py - Merge JSON typelib files into a .cpp file
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
from perfecthash import PerfectHash
from collections import OrderedDict
import buildconfig

# Pick a nice power-of-two size for our intermediate PHF tables.
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
    "mReflectable",
    "mOptArgc",
    "mContext",
    "mHasRetval",
    "mIsSymbol",
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
nsXPTConstantInfo = mkstruct(
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
        # Match endianness of the target platform for each component
        if buildconfig.substs['TARGET_ENDIANNESS'] == 'little':
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
    {'tag': 'TD_NSIDPTR'},
    {'tag': 'TD_PSTRING'},
    {'tag': 'TD_PWSTRING'},
    {'tag': 'TD_INTERFACE_IS_TYPE', 'iid_is': 0},
]


# Core of the code generator. Takes a list of raw JSON XPT interfaces, and
# writes out a file containing the necessary static declarations into fd.
def link_to_cpp(interfaces, fd):
    # Perfect Hash from IID to interface.
    iid_phf = PerfectHash(interfaces, PHFSIZE,
                          key=lambda i: iid_bytes(i['uuid']))
    for idx, iface in enumerate(iid_phf.entries):
        iface['idx'] = idx  # Store the index in iid_phf of the entry.

    # Perfect Hash from name to iid_phf index.
    name_phf = PerfectHash(interfaces, PHFSIZE,
                           key=lambda i: i['name'].encode('ascii'))

    def interface_idx(name):
        entry = name and name_phf.get_entry(name.encode('ascii'))
        if entry:
            return entry['idx'] + 1  # 1-based, use 0 as a sentinel.
        return 0

    # NOTE: State used while linking. This is done with closures rather than a
    # class due to how this file's code evolved.
    includes = set()
    types = []
    type_cache = {}
    params = []
    param_cache = {}
    methods = []
    consts = []
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

    def lower_symbol(s):
        return "uint32_t(JS::SymbolCode::%s)" % s

    def lower_extra_type(type):
        key = describe_type(type)
        idx = type_cache.get(key)
        if idx is None:
            idx = type_cache[key] = len(types)
            types.append(lower_type(type))
        return idx

    def describe_type(type):  # Create the type's documentation comment.
        tag = type['tag'][3:].lower()
        if tag == 'legacy_array':
            return '%s[size_is=%d]' % (
                describe_type(type['element']), type['size_is'])
        elif tag == 'array':
            return 'Array<%s>' % describe_type(type['element'])
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

        # TD_VOID is used for types that can't be represented in JS, so they
        # should not be represented in the XPT info.
        assert tag != 'TD_VOID'

        if tag == 'TD_LEGACY_ARRAY':
            d1 = type['size_is']
            d2 = lower_extra_type(type['element'])

        elif tag == 'TD_ARRAY':
            # NOTE: TD_ARRAY can hold 16 bits of type index, while
            # TD_LEGACY_ARRAY can only hold 8.
            d1, d2 = splitint(lower_extra_type(type['element']))

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

    def is_method_reflectable(method):
        if 'hidden' in method['flags']:
            return False

        for param in method['params']:
            # Reflected methods can't use native types. All native types end up
            # getting tagged as void*, so this check is easy.
            if param['type']['tag'] == 'TD_VOID':
                return False

        return True

    def lower_method(method, ifacename):
        methodname = "%s::%s" % (ifacename, method['name'])

        isSymbol = 'symbol' in method['flags']
        reflectable = is_method_reflectable(method)

        if not reflectable:
            # Hide the parameters of methods that can't be called from JS to
            # reduce the size of the file.
            paramidx = name = numparams = 0
        else:
            if isSymbol:
                name = lower_symbol(method['name'])
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

            mName=name,
            mParams=paramidx,
            mNumParams=numparams,

            # Flags
            mGetter='getter' in method['flags'],
            mSetter='setter' in method['flags'],
            mReflectable=reflectable,
            mOptArgc='optargc' in method['flags'],
            mContext='jscontext' in method['flags'],
            mHasRetval='hasretval' in method['flags'],
            mIsSymbol=isSymbol,
        ))

    def lower_const(const, ifacename):
        assert const['type']['tag'] in \
            ['TD_INT16', 'TD_INT32', 'TD_UINT8', 'TD_UINT16', 'TD_UINT32']
        is_signed = const['type']['tag'] in ['TD_INT16', 'TD_INT32']

        # Constants are always either signed or unsigned 16 or 32 bit integers,
        # which we will only need to convert to JS values. To save on space,
        # don't bother storing the type, and instead just store a 32-bit
        # unsigned integer, and stash whether to interpret it as signed.
        consts.append(nsXPTConstantInfo(
            "%d = %s::%s" % (len(consts), ifacename, const['name']),

            mName=lower_string(const['name']),
            mSigned=is_signed,
            mValue="(uint32_t)%d" % const['value'],
        ))

    def ancestors(iface):
        yield iface
        while iface['parent']:
            iface = name_phf.get_entry(iface['parent'].encode('ascii'))
            yield iface

    def lower_iface(iface):
        method_cnt = sum(len(i['methods']) for i in ancestors(iface))
        const_cnt = sum(len(i['consts']) for i in ancestors(iface))

        # The number of maximum methods is not arbitrary. It is the same value
        # as in xpcom/reflect/xptcall/genstubs.pl; do not change this value
        # without changing that one or you WILL see problems.
        #
        # In addition, mNumMethods and mNumConsts are stored as a 8-bit ints,
        # meaning we cannot exceed 255 methods/consts on any interface.
        assert method_cnt < 250, "%s has too many methods" % iface['name']
        assert const_cnt < 256, "%s has too many constants" % iface['name']

        # Store the lowered interface as 'cxx' on the iface object.
        iface['cxx'] = nsXPTInterfaceInfo(
            "%d = %s" % (iface['idx'], iface['name']),

            mIID=lower_uuid(iface['uuid']),
            mName=lower_string(iface['name']),
            mParent=interface_idx(iface['parent']),

            mMethods=len(methods),
            mNumMethods=method_cnt,
            mConsts=len(consts),
            mNumConsts=const_cnt,

            # Flags
            mBuiltinClass='builtinclass' in iface['flags'],
            mMainProcessScriptableOnly='main_process_only' in iface['flags'],
            mFunction='function' in iface['flags'],
        )

        # Lower methods and constants used by this interface
        for method in iface['methods']:
            lower_method(method, iface['name'])
        for const in iface['consts']:
            lower_const(const, iface['name'])

    # Lower the types which have fixed indexes first, and check that the indexes
    # seem correct.
    for expected, ty in enumerate(utility_types):
        got = lower_extra_type(ty)
        assert got == expected, "Wrong index when lowering"

    # Lower interfaces in the order of the IID phf's entries lookup.
    for iface in iid_phf.entries:
        lower_iface(iface)

    # Write out the final output file
    fd.write("/* THIS FILE WAS GENERATED BY xptcodegen.py - DO NOT EDIT */\n\n")

    # Include any bindings files which we need to include for webidl types
    for include in sorted(includes):
        fd.write('#include "%s"\n' % include)

    # Write out our header
    fd.write("""
#include "xptinfo.h"
#include "mozilla/PerfectHash.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/dom/BindingUtils.h"

// These template methods are specialized to be used in the sDOMObjects table.
template<mozilla::dom::prototypes::ID PrototypeID, typename T>
static nsresult UnwrapDOMObject(JS::HandleValue aHandle, void** aObj, JSContext* aCx)
{
  RefPtr<T> p;
  nsresult rv = mozilla::dom::UnwrapObject<PrototypeID, T>(aHandle, p, aCx);
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
    array("nsXPTType", "sTypes", types)
    array("nsXPTParamInfo", "sParams", params)
    array("nsXPTMethodInfo", "sMethods", methods)
    array("nsXPTDOMObjectInfo", "sDOMObjects", domobjects)
    array("nsXPTConstantInfo", "sConsts", consts)

    # The strings array. We write out individual characters to avoid MSVC restrictions.
    fd.write("const char sStrings[] = {\n")
    for s, off in strings.iteritems():
        fd.write("  // %d = %s\n  '%s','\\0',\n" % (off, s, "','".join(s)))
    fd.write("};\n\n")

    # Build the perfect hash table for InterfaceByIID
    fd.write(iid_phf.cxx_codegen(
        name='InterfaceByIID',
        entry_type='nsXPTInterfaceInfo',
        entries_name='sInterfaces',
        lower_entry=lambda iface: iface['cxx'],

        # Check that the IIDs match to support IID keys not in the map.
        return_type='const nsXPTInterfaceInfo*',
        return_entry='return entry.IID().Equals(aKey) ? &entry : nullptr;',

        key_type='const nsIID&',
        key_bytes='reinterpret_cast<const char*>(&aKey)',
        key_length='sizeof(nsIID)'))
    fd.write('\n')

    # Build the perfect hash table for InterfaceByName
    fd.write(name_phf.cxx_codegen(
        name='InterfaceByName',
        entry_type='uint16_t',
        lower_entry=lambda iface: '%-4d /* %s */' % (iface['idx'], iface['name']),

        # Get the actual nsXPTInterfaceInfo from sInterfaces, and
        # double-check that names match.
        return_type='const nsXPTInterfaceInfo*',
        return_entry='return strcmp(sInterfaces[entry].Name(), aKey) == 0'
                     ' ? &sInterfaces[entry] : nullptr;'))
    fd.write('\n')

    # Generate some checks that the indexes for the utility types match the
    # declared ones in xptinfo.h
    for idx, ty in enumerate(utility_types):
        fd.write("static_assert(%d == (uint8_t)nsXPTType::Idx::%s, \"Bad idx\");\n" %
                 (idx, ty['tag'][3:]))

    fd.write("""
const uint16_t sInterfacesSize = mozilla::ArrayLength(sInterfaces);

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
