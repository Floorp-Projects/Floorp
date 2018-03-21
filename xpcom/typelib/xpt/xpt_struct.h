/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/*
 * Structures matching the in-memory representation of typelib structures.
 * http://www.mozilla.org/scriptable/typelib_file.html
 */

#ifndef xpt_struct_h
#define xpt_struct_h

#include "nsID.h"
#include <stdint.h>
#include "mozilla/Assertions.h"

/*
 * Originally, I was going to have structures that exactly matched the on-disk
 * representation, but that proved difficult: different compilers can pack
 * their structs differently, and that makes overlaying them atop a
 * read-from-disk byte buffer troublesome.  So now I just have some structures
 * that are used in memory, and we're going to write a nice XDR library to
 * write them to disk and stuff.  It is pure joy. -- shaver
 */

/* Structures for the typelib components */

struct XPTHeader;
struct XPTInterfaceDirectoryEntry;
struct XPTInterfaceDescriptor;
struct XPTConstDescriptor;
struct XPTMethodDescriptor;
struct XPTParamDescriptor;
struct XPTTypeDescriptor;
struct XPTTypeDescriptorPrefix;

/*
 * Every XPCOM typelib file begins with a header.
 */
struct XPTHeader {
  // Some of these fields exists in the on-disk format but don't need to be
  // stored in memory (other than very briefly, which can be done with local
  // variables).

  //uint8_t mMagic[16];
  uint8_t mMajorVersion;
  //uint8_t mMinorVersion;
  uint16_t mNumInterfaces;
  //uint32_t mFileLength;
  const XPTInterfaceDirectoryEntry* mInterfaceDirectory;
  //uint32_t mDataPool;
};

/*
 * Any file with a major version number of XPT_MAJOR_INCOMPATIBLE_VERSION
 * or higher is to be considered incompatible by this version of xpt and
 * we will refuse to read it. We will return a header with magic, major and
 * minor versions set from the file. num_interfaces will be set to zero to
 * confirm our inability to read the file; i.e. even if some client of this
 * library gets out of sync with us regarding the agreed upon value for
 * XPT_MAJOR_INCOMPATIBLE_VERSION, anytime num_interfaces is zero we *know*
 * that this library refused to read the file due to version incompatibility.
 */
#define XPT_MAJOR_INCOMPATIBLE_VERSION 0x02

/*
 * A contiguous array of fixed-size InterfaceDirectoryEntry records begins at
 * the byte offset identified by the mInterfaceDirectory field in the file
 * header.  The array is used to quickly locate an interface description
 * using its IID.  No interface should appear more than once in the array.
 */
struct XPTInterfaceDirectoryEntry {
  nsID mIID;
  const char* mName;

  // This field exists in the on-disk format. But it isn't used so we don't
  // allocate space for it in memory.
  //const char* mNameSpace;

  const XPTInterfaceDescriptor* mInterfaceDescriptor;
};

/*
 * An InterfaceDescriptor describes a single XPCOM interface, including all of
 * its methods.
 */
struct XPTInterfaceDescriptor {
  static const uint8_t kScriptableMask =                0x80;
  static const uint8_t kFunctionMask =                  0x40;
  static const uint8_t kBuiltinClassMask =              0x20;
  static const uint8_t kMainProcessScriptableOnlyMask = 0x10;

  bool IsScriptable() const { return !!(mFlags & kScriptableMask); }
  bool IsFunction() const { return !!(mFlags & kFunctionMask); }
  bool IsBuiltinClass() const { return !!(mFlags & kBuiltinClassMask); }
  bool IsMainProcessScriptableOnly() const { return !!(mFlags & kMainProcessScriptableOnlyMask); }

  /*
   * This field ordering minimizes the size of this struct.
   * The fields are serialized on disk in a different order.
   * See DoInterfaceDescriptor().
   */
  const XPTMethodDescriptor* mMethodDescriptors;
  const XPTConstDescriptor* mConstDescriptors;
  const XPTTypeDescriptor* mAdditionalTypes;
  uint16_t mParentInterface;
  uint16_t mNumMethods;
  uint16_t mNumConstants;
  uint8_t mFlags;

  /*
   * mAdditionalTypes are used for arrays where we may need multiple
   * XPTTypeDescriptors for a single XPTMethodDescriptor. Since we still
   * want to have a simple array of XPTMethodDescriptor (each with a single
   * embedded XPTTypeDescriptor), a XPTTypeDescriptor can have a reference
   * to an 'additional_type'. That reference is an index in this
   * "mAdditionalTypes" array. So a given XPTMethodDescriptor might have
   * a whole chain of these XPTTypeDescriptors to represent, say, a multi
   * dimensional array.
   *
   * Note that in the typelib file these additional types are stored 'inline'
   * in the MethodDescriptor. But, in the typelib MethodDescriptors can be
   * of varying sizes, where in XPT's in memory mapping of the data we want
   * them to be of fixed size. This mAdditionalTypes scheme is here to allow
   * for that.
   */
  uint8_t mNumAdditionalTypes;
};

/*
 * A TypeDescriptor is a variable-size record used to identify the type of a
 * method argument or return value.
 *
 * There are three types of TypeDescriptors:
 *
 * SimpleTypeDescriptor
 * InterfaceTypeDescriptor
 * InterfaceIsTypeDescriptor
 *
 * The tag field in the prefix indicates which of the variant TypeDescriptor
 * records is being used, and hence the way any remaining fields should be
 * parsed. Values from 0 to 17 refer to SimpleTypeDescriptors. The value 18
 * designates an InterfaceTypeDescriptor, while 19 represents an
 * InterfaceIsTypeDescriptor.
 */

/* why bother with a struct?  - other code relies on this being a struct */
struct XPTTypeDescriptorPrefix {
  uint8_t TagPart() const {
    static const uint8_t kFlagMask = 0xe0;
    return (uint8_t) (mFlags & ~kFlagMask);
  }

  uint8_t mFlags;
};

/*
 * The following enum maps mnemonic names to the different numeric values
 * of XPTTypeDescriptor->tag.
 */
enum XPTTypeDescriptorTags {
  TD_INT8              = 0,
  TD_INT16             = 1,
  TD_INT32             = 2,
  TD_INT64             = 3,
  TD_UINT8             = 4,
  TD_UINT16            = 5,
  TD_UINT32            = 6,
  TD_UINT64            = 7,
  TD_FLOAT             = 8,
  TD_DOUBLE            = 9,
  TD_BOOL              = 10,
  TD_CHAR              = 11,
  TD_WCHAR             = 12,
  TD_VOID              = 13,
  TD_PNSIID            = 14,
  TD_DOMSTRING         = 15,
  TD_PSTRING           = 16,
  TD_PWSTRING          = 17,
  TD_INTERFACE_TYPE    = 18,
  TD_INTERFACE_IS_TYPE = 19,
  TD_ARRAY             = 20,
  TD_PSTRING_SIZE_IS   = 21,
  TD_PWSTRING_SIZE_IS  = 22,
  TD_UTF8STRING        = 23,
  TD_CSTRING           = 24,
  TD_ASTRING           = 25,
  TD_JSVAL             = 26
};

struct XPTTypeDescriptor {
  uint8_t Tag() const {
    return mPrefix.TagPart();
  }

  uint8_t ArgNum() const {
    MOZ_ASSERT(Tag() == TD_INTERFACE_IS_TYPE ||
               Tag() == TD_PSTRING_SIZE_IS ||
               Tag() == TD_PWSTRING_SIZE_IS ||
               Tag() == TD_ARRAY);
    return mData1;
  }

  const XPTTypeDescriptor* ArrayElementType(const XPTInterfaceDescriptor* aDescriptor) const {
    MOZ_ASSERT(Tag() == TD_ARRAY);
    return &aDescriptor->mAdditionalTypes[mData2];
  }

  // We store the 16-bit iface value as two 8-bit values in order to
  // avoid 16-bit alignment requirements for XPTTypeDescriptor, which
  // reduces its size and also the size of XPTParamDescriptor.
  uint16_t InterfaceIndex() const {
    MOZ_ASSERT(Tag() == TD_INTERFACE_TYPE);
    return (mData1 << 8) | mData2;
  }

  XPTTypeDescriptorPrefix mPrefix;

  // The data for the different variants is stored in these two data fields.
  // These should only be accessed via the getter methods above, which will
  // assert if the tag is invalid. The memory layout here doesn't exactly match
  // the on-disk format. This is to save memory. Some fields for some cases are
  // smaller than they are on disk or omitted entirely.
  uint8_t mData1;
  uint8_t mData2;
};

/*
 * A ConstDescriptor is a variable-size record that records the name and
 * value of a scoped interface constant. This is allowed only for a subset
 * of types.
 *
 * The type (and thus the size) of the value record is determined by the
 * contents of the associated TypeDescriptor record. For instance, if type
 * corresponds to int16_t, then value is a two-byte record consisting of a
 * 16-bit signed integer.
 */
union XPTConstValue {
  int16_t i16;
  uint16_t ui16;
  int32_t i32;
  uint32_t ui32;
}; /* varies according to type */

struct XPTConstDescriptor {
  const char* mName;
  XPTTypeDescriptor mType;
  union XPTConstValue mValue;
};

/*
 * A ParamDescriptor is a variable-size record used to describe either a
 * single argument to a method or a method's result.
 */
struct XPTParamDescriptor {
  uint8_t mFlags;
  XPTTypeDescriptor mType;
};

/*
 * A MethodDescriptor is a variable-size record used to describe a single
 * interface method.
 */
struct XPTMethodDescriptor {
  const char* mName;
  const XPTParamDescriptor* mParams;
  //XPTParamDescriptor mResult; // Present on disk, omitted here.
  uint8_t mFlags;
  uint8_t mNumArgs;
};

#endif /* xpt_struct_h */
