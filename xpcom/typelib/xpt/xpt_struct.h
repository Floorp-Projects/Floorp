/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/*
 * Structures for representing typelib structures in memory.
 * http://www.mozilla.org/scriptable/typelib_file.html
 */

#ifndef xpt_struct_h
#define xpt_struct_h

#include "nsID.h"
#include <stdint.h>
#include "mozilla/Assertions.h"

struct XPTInterfaceDescriptor;
struct XPTConstDescriptor;
struct XPTMethodDescriptor;
struct XPTParamDescriptor;
struct XPTTypeDescriptor;
struct XPTTypeDescriptorPrefix;

struct XPTHeader {
  friend struct XPTInterfaceDescriptor;
  friend struct XPTConstDescriptor;
  friend struct XPTMethodDescriptor;
  friend struct XPTTypeDescriptor;

  static const uint16_t kNumInterfaces;
  static const XPTInterfaceDescriptor kInterfaces[];

private:
  static const XPTTypeDescriptor kTypes[];
  static const XPTParamDescriptor kParams[];
  static const XPTMethodDescriptor kMethods[];
  static const XPTConstDescriptor kConsts[];

  // All of the strings for this header, including their null
  // terminators, concatenated into a single string.
  static const char kStrings[];
};

/*
 * An InterfaceDescriptor describes a single XPCOM interface, including all of
 * its methods.
 */
struct XPTInterfaceDescriptor {
  constexpr XPTInterfaceDescriptor(nsID aIID,
                                   uint32_t aName,
                                   uint16_t aMethodDescriptors,
                                   uint16_t aConstDescriptors,
                                   uint16_t aParentInterface,
                                   uint16_t aNumMethods,
                                   uint16_t aNumConstants,
                                   uint8_t aFlags)
    : mIID(aIID)
    , mName(aName)
    , mMethodDescriptors(aMethodDescriptors)
    , mConstDescriptors(aConstDescriptors)
    , mParentInterface(aParentInterface)
    , mNumMethods(aNumMethods)
    , mNumConstants(aNumConstants)
    , mFlags(aFlags)
  {}

  static const uint8_t kScriptableMask =                0x80;
  static const uint8_t kFunctionMask =                  0x40;
  static const uint8_t kBuiltinClassMask =              0x20;
  static const uint8_t kMainProcessScriptableOnlyMask = 0x10;

  bool IsScriptable() const { return !!(mFlags & kScriptableMask); }
  bool IsFunction() const { return !!(mFlags & kFunctionMask); }
  bool IsBuiltinClass() const { return !!(mFlags & kBuiltinClassMask); }
  bool IsMainProcessScriptableOnly() const { return !!(mFlags & kMainProcessScriptableOnlyMask); }

  inline const char* Name() const;
  inline const XPTMethodDescriptor& Method(size_t aIndex) const;
  inline const XPTConstDescriptor& Const(size_t aIndex) const;

  /*
   * This field ordering minimizes the size of this struct.
   */
  nsID mIID;

private:
  uint32_t mName; // Index into XPTHeader::mStrings.
  uint16_t mMethodDescriptors; // Index into XPTHeader::mMethods.
  uint16_t mConstDescriptors; // Index into XPTHeader::mConsts.

public:
  uint16_t mParentInterface;
  uint16_t mNumMethods;
  uint16_t mNumConstants;

private:
  uint8_t mFlags;
};

/*
 * A TypeDescriptor is a union used to identify the type of a method
 * argument or return value.
 *
 * There are three types of TypeDescriptors:
 *
 * SimpleTypeDescriptor
 * InterfaceTypeDescriptor
 * InterfaceIsTypeDescriptor
 *
 * The tag field in the prefix indicates which of the variant TypeDescriptor
 * records is being used, and hence which union members are valid. Values from 0
 * to 17 refer to SimpleTypeDescriptors. The value 18 designates an
 * InterfaceTypeDescriptor, while 19 represents an InterfaceIsTypeDescriptor.
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
  constexpr XPTTypeDescriptor(XPTTypeDescriptorPrefix aPrefix,
                              uint8_t aData1 = 0,
                              uint8_t aData2 = 0)
    : mPrefix(aPrefix)
    , mData1(aData1)
    , mData2(aData2)
  {}

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

  const XPTTypeDescriptor* ArrayElementType() const {
    MOZ_ASSERT(Tag() == TD_ARRAY);
    return &XPTHeader::kTypes[mData2];
  }

  // We store the 16-bit iface value as two 8-bit values in order to
  // avoid 16-bit alignment requirements for XPTTypeDescriptor, which
  // reduces its size and also the size of XPTParamDescriptor.
  uint16_t InterfaceIndex() const {
    MOZ_ASSERT(Tag() == TD_INTERFACE_TYPE);
    return (mData1 << 8) | mData2;
  }

  XPTTypeDescriptorPrefix mPrefix;

private:
  // The data for the different variants is stored in these two data fields.
  // These should only be accessed via the getter methods above, which will
  // assert if the tag is invalid. The memory layout here doesn't exactly match
  // the on-disk format. This is to save memory. Some fields for some cases are
  // smaller than they are on disk or omitted entirely.
  uint8_t mData1;
  uint8_t mData2;
};

/*
 * A ConstDescriptor records the name and value of a scoped interface constant.
 * This is allowed only for a subset of types.
 *
 * The type of the value record is determined by the contents of the associated
 * TypeDescriptor record. For instance, if type corresponds to int16_t, then
 * value is a 16-bit signed integer.
 */
union XPTConstValue {
  int16_t i16;
  uint16_t ui16;
  int32_t i32;
  uint32_t ui32;

  // These constructors are needed to statically initialize different cases of
  // the union because MSVC does not support the use of designated initializers
  // in C++ code. They need to be constexpr to ensure that no initialization code
  // is run at startup, to enable sharing of this memory between Firefox processes.
  explicit constexpr XPTConstValue(int16_t aInt) : i16(aInt) {}
  explicit constexpr XPTConstValue(uint16_t aInt) : ui16(aInt) {}
  explicit constexpr XPTConstValue(int32_t aInt) : i32(aInt) {}
  explicit constexpr XPTConstValue(uint32_t aInt) : ui32(aInt) {}
};

struct XPTConstDescriptor {
  constexpr XPTConstDescriptor(uint32_t aName,
                               const XPTTypeDescriptor& aType,
                               const XPTConstValue& aValue)
    : mName(aName)
    , mType(aType)
    , mValue(aValue)
  {}

  const char* Name() const {
    return &XPTHeader::kStrings[mName];
  }

private:
  uint32_t mName; // Index into XPTHeader::mStrings.

public:
  XPTTypeDescriptor mType;
  XPTConstValue mValue;
};

/*
 * A ParamDescriptor is used to describe either a single argument to a method or
 * a method's result.
 */
struct XPTParamDescriptor {
  constexpr XPTParamDescriptor(uint8_t aFlags,
                               const XPTTypeDescriptor& aType)
    : mFlags(aFlags)
    , mType(aType)
  {}

protected:
  uint8_t mFlags;

public:
  XPTTypeDescriptor mType;
};

/*
 * A MethodDescriptor is used to describe a single interface method.
 */
struct XPTMethodDescriptor {
  constexpr XPTMethodDescriptor(uint32_t aName,
                                uint32_t aParams,
                                uint8_t aFlags,
                                uint8_t aNumArgs)
    : mName(aName)
    , mParams(aParams)
    , mFlags(aFlags)
    , mNumArgs(aNumArgs)
  {}

  const char* Name() const {
    return &XPTHeader::kStrings[mName];
  }
  const XPTParamDescriptor& Param(uint8_t aIndex) const {
    return XPTHeader::kParams[mParams + aIndex];
  }

private:
  uint32_t mName; // Index into XPTHeader::mStrings.
  uint32_t mParams; // Index into XPTHeader::mParams.

protected:
  uint8_t mFlags;
  uint8_t mNumArgs;
};

const char*
XPTInterfaceDescriptor::Name() const {
  return &XPTHeader::kStrings[mName];
}

const XPTMethodDescriptor&
XPTInterfaceDescriptor::Method(size_t aIndex) const {
  return XPTHeader::kMethods[mMethodDescriptors + aIndex];
}

const XPTConstDescriptor&
XPTInterfaceDescriptor::Const(size_t aIndex) const {
  return XPTHeader::kConsts[mConstDescriptors + aIndex];
}

#endif /* xpt_struct_h */
