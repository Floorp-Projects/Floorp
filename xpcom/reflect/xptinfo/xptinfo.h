/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/**
 * Structures and methods with information about XPCOM interfaces for use by
 * XPConnect. The static backing data structures used by this file are generated
 * from xpidl interfaces by the jsonxpt.py and xptcodegen.py scripts.
 */

#ifndef xptinfo_h
#define xptinfo_h

#include <stdint.h>
#include "nsID.h"
#include "mozilla/Assertions.h"
#include "jsapi.h"
#include "js/Symbol.h"
#include "js/Value.h"
#include "nsString.h"
#include "nsTArray.h"

// Forward Declarations
namespace mozilla {
namespace dom {
struct NativePropertyHooks;
}  // namespace dom
}  // namespace mozilla

struct nsXPTInterfaceInfo;
struct nsXPTType;
struct nsXPTParamInfo;
struct nsXPTMethodInfo;
struct nsXPTConstantInfo;
struct nsXPTDOMObjectInfo;

// Internal helper methods.
namespace xpt {
namespace detail {

inline const nsXPTInterfaceInfo* GetInterface(uint16_t aIndex);
inline const nsXPTType& GetType(uint16_t aIndex);
inline const nsXPTParamInfo& GetParam(uint16_t aIndex);
inline const nsXPTMethodInfo& GetMethod(uint16_t aIndex);
inline const nsXPTConstantInfo& GetConstant(uint16_t aIndex);
inline const nsXPTDOMObjectInfo& GetDOMObjectInfo(uint16_t aIndex);
inline const char* GetString(uint32_t aIndex);

const nsXPTInterfaceInfo* InterfaceByIID(const nsIID& aIID);
const nsXPTInterfaceInfo* InterfaceByName(const char* aName);

extern const uint16_t sInterfacesSize;

}  // namespace detail
}  // namespace xpt

/*
 * An Interface describes a single XPCOM interface, including all of its
 * methods. We don't record non-scriptable interfaces.
 */
struct nsXPTInterfaceInfo {
  // High efficiency getters for Interfaces based on perfect hashes.
  static const nsXPTInterfaceInfo* ByIID(const nsIID& aIID) {
    return xpt::detail::InterfaceByIID(aIID);
  }
  static const nsXPTInterfaceInfo* ByName(const char* aName) {
    return xpt::detail::InterfaceByName(aName);
  }

  // These are only needed for Components_interfaces's enumerator.
  static const nsXPTInterfaceInfo* ByIndex(uint16_t aIndex) {
    // NOTE: We add 1 here, as the internal index 0 is reserved for null.
    return xpt::detail::GetInterface(aIndex + 1);
  }
  static uint16_t InterfaceCount() { return xpt::detail::sInterfacesSize; }

  // Interface flag getters
  bool IsFunction() const { return mFunction; }
  bool IsBuiltinClass() const { return mBuiltinClass; }
  bool IsMainProcessScriptableOnly() const {
    return mMainProcessScriptableOnly;
  }

  const char* Name() const { return xpt::detail::GetString(mName); }
  const nsIID& IID() const { return mIID; }

  // Get the parent interface, or null if this interface doesn't have a parent.
  const nsXPTInterfaceInfo* GetParent() const {
    return xpt::detail::GetInterface(mParent);
  }

  // Do we have an ancestor interface with the given IID?
  bool HasAncestor(const nsIID& aIID) const;

  // Get methods & constants
  uint16_t ConstantCount() const { return mNumConsts; }
  const nsXPTConstantInfo& Constant(uint16_t aIndex) const;
  uint16_t MethodCount() const { return mNumMethods; }
  const nsXPTMethodInfo& Method(uint16_t aIndex) const;

  nsresult GetMethodInfo(uint16_t aIndex, const nsXPTMethodInfo** aInfo) const;
  nsresult GetConstant(uint16_t aIndex, JS::MutableHandleValue constant,
                       char** aName) const;

  ////////////////////////////////////////////////////////////////
  // Ensure these fields are in the same order as xptcodegen.py //
  ////////////////////////////////////////////////////////////////

  nsID mIID;
  uint32_t mName;  // Index into xpt::detail::sStrings

  uint16_t mParent : 14;
  uint16_t mBuiltinClass : 1;
  // XXX(nika): Do we need this if we don't have addons anymore?
  uint16_t mMainProcessScriptableOnly : 1;

  uint16_t mMethods;  // Index into xpt::detail::sMethods

  uint16_t mConsts : 14;  // Index into xpt::detail::sConsts
  uint16_t mFunction : 1;
  // uint16_t unused : 1;

  uint8_t mNumMethods;  // NOTE(24/04/18): largest=nsIDocShell (193)
  uint8_t mNumConsts;   // NOTE(24/04/18): largest=nsIAccessibleRole (175)
};

// The fields in nsXPTInterfaceInfo were carefully ordered to minimize size.
static_assert(sizeof(nsXPTInterfaceInfo) == 28, "wrong size?");

/*
 * The following enum represents contains the different tag types which
 * can be found in nsXPTTypeInfo::mTag.
 *
 * WARNING: mTag is 5 bits wide, supporting at most 32 tags.
 */
enum nsXPTTypeTag : uint8_t {
  // Arithmetic (POD) Types
  //  - Do not require cleanup,
  //  - All bit patterns are valid,
  //  - Outparams may be uninitialized by caller,
  //  - Directly supported in xptcall.
  //
  // NOTE: The name 'Arithmetic' comes from Harbison/Steele. Despite being a tad
  // unclear, it is used frequently in xptcall, so is unlikely to be changed.
  TD_INT8 = 0,
  TD_INT16 = 1,
  TD_INT32 = 2,
  TD_INT64 = 3,
  TD_UINT8 = 4,
  TD_UINT16 = 5,
  TD_UINT32 = 6,
  TD_UINT64 = 7,
  TD_FLOAT = 8,
  TD_DOUBLE = 9,
  TD_BOOL = 10,
  TD_CHAR = 11,
  TD_WCHAR = 12,
  _TD_LAST_ARITHMETIC = TD_WCHAR,

  // Pointer Types
  //  - Require cleanup unless NULL,
  //  - All-zeros (NULL) bit pattern is valid,
  //  - Outparams may be uninitialized by caller,
  //  - Supported in xptcall as raw pointer.
  TD_VOID = 13,
  TD_NSIDPTR = 14,
  TD_PSTRING = 15,
  TD_PWSTRING = 16,
  TD_INTERFACE_TYPE = 17,
  TD_INTERFACE_IS_TYPE = 18,
  TD_LEGACY_ARRAY = 19,
  TD_PSTRING_SIZE_IS = 20,
  TD_PWSTRING_SIZE_IS = 21,
  TD_DOMOBJECT = 22,
  TD_PROMISE = 23,
  _TD_LAST_POINTER = TD_PROMISE,

  // Complex Types
  //  - Require cleanup,
  //  - Always passed indirectly,
  //  - Outparams must be initialized by caller,
  //  - Supported in xptcall due to indirection.
  TD_UTF8STRING = 24,
  TD_CSTRING = 25,
  TD_ASTRING = 26,
  TD_NSID = 27,
  TD_JSVAL = 28,
  TD_ARRAY = 29,
  _TD_LAST_COMPLEX = TD_ARRAY
};

static_assert(_TD_LAST_COMPLEX < 32, "nsXPTTypeTag must fit in 5 bits");

/*
 * A nsXPTType is a union used to identify the type of a method argument or
 * return value. The internal data is stored as an 5-bit tag, and two 8-bit
 * integers, to keep alignment requirements low.
 *
 * nsXPTType contains 3 extra bits, reserved for use by nsXPTParamInfo.
 */
struct nsXPTType {
  nsXPTTypeTag Tag() const { return static_cast<nsXPTTypeTag>(mTag); }

  // The index in the function argument list which should be used when
  // determining the iid_is or size_is properties of this dependent type.
  uint8_t ArgNum() const {
    MOZ_ASSERT(Tag() == TD_INTERFACE_IS_TYPE || Tag() == TD_PSTRING_SIZE_IS ||
               Tag() == TD_PWSTRING_SIZE_IS || Tag() == TD_LEGACY_ARRAY);
    return mData1;
  }

 private:
  // Helper for reading 16-bit data values split between mData1 and mData2.
  uint16_t Data16() const {
    return static_cast<uint16_t>(mData1 << 8) | mData2;
  }

 public:
  // Get the type of the element in the current array or sequence. Arrays only
  // fit 8 bits of type data, while sequences support up to 16 bits of type data
  // due to not needing to store an ArgNum.
  const nsXPTType& ArrayElementType() const {
    if (Tag() == TD_LEGACY_ARRAY) {
      return xpt::detail::GetType(mData2);
    }
    MOZ_ASSERT(Tag() == TD_ARRAY);
    return xpt::detail::GetType(Data16());
  }

  // We store the 16-bit iface value as two 8-bit values in order to
  // avoid 16-bit alignment requirements for XPTTypeDescriptor, which
  // reduces its size and also the size of XPTParamDescriptor.
  const nsXPTInterfaceInfo* GetInterface() const {
    MOZ_ASSERT(Tag() == TD_INTERFACE_TYPE);
    return xpt::detail::GetInterface(Data16());
  }

  const nsXPTDOMObjectInfo& GetDOMObjectInfo() const {
    MOZ_ASSERT(Tag() == TD_DOMOBJECT);
    return xpt::detail::GetDOMObjectInfo(Data16());
  }

  // See the comments in nsXPTTypeTag for an explanation as to what each of
  // these categories mean.
  bool IsArithmetic() const { return Tag() <= _TD_LAST_ARITHMETIC; }
  bool IsPointer() const {
    return !IsArithmetic() && Tag() <= _TD_LAST_POINTER;
  }
  bool IsComplex() const { return Tag() > _TD_LAST_POINTER; }

  bool IsInterfacePointer() const {
    return Tag() == TD_INTERFACE_TYPE || Tag() == TD_INTERFACE_IS_TYPE;
  }

  bool IsDependent() const {
    return (Tag() == TD_ARRAY && InnermostType().IsDependent()) ||
           Tag() == TD_INTERFACE_IS_TYPE || Tag() == TD_LEGACY_ARRAY ||
           Tag() == TD_PSTRING_SIZE_IS || Tag() == TD_PWSTRING_SIZE_IS;
  }

  // Unwrap a nested type to its innermost value (e.g. through arrays).
  const nsXPTType& InnermostType() const {
    if (Tag() == TD_LEGACY_ARRAY || Tag() == TD_ARRAY) {
      return ArrayElementType().InnermostType();
    }
    return *this;
  }

  // In-memory size of native type in bytes.
  inline size_t Stride() const;

  // Offset the given base pointer to reference the element at the given index.
  void* ElementPtr(const void* aBase, uint32_t aIndex) const {
    return (char*)aBase + (aIndex * Stride());
  }

  // Zero out a native value of the given type. The type must not be 'complex'.
  void ZeroValue(void* aValue) const {
    MOZ_RELEASE_ASSERT(!IsComplex(), "Cannot zero a complex value");
    memset(aValue, 0, Stride());
  }

  // Indexes into the extra types array of a small set of known types.
  enum class Idx : uint8_t {
    INT8 = 0,
    UINT8,
    INT16,
    UINT16,
    INT32,
    UINT32,
    INT64,
    UINT64,
    FLOAT,
    DOUBLE,
    BOOL,
    CHAR,
    WCHAR,
    NSIDPTR,
    PSTRING,
    PWSTRING,
    INTERFACE_IS_TYPE
  };

  // Helper methods for fabricating nsXPTType values used by xpconnect.
  static nsXPTType MkArrayType(Idx aInner) {
    MOZ_ASSERT(aInner <= Idx::INTERFACE_IS_TYPE);
    return {TD_LEGACY_ARRAY, false, false, false, 0, (uint8_t)aInner};
  }
  static const nsXPTType& Get(Idx aInner) {
    MOZ_ASSERT(aInner <= Idx::INTERFACE_IS_TYPE);
    return xpt::detail::GetType((uint8_t)aInner);
  }

  ///////////////////////////////////////
  // nsXPTType backwards compatibility //
  ///////////////////////////////////////

  nsXPTType& operator=(nsXPTTypeTag aPrefix) {
    mTag = aPrefix;
    return *this;
  }
  operator nsXPTTypeTag() const { return Tag(); }

#define TD_ALIAS_(name_, value_) static constexpr nsXPTTypeTag name_ = value_
  TD_ALIAS_(T_I8, TD_INT8);
  TD_ALIAS_(T_I16, TD_INT16);
  TD_ALIAS_(T_I32, TD_INT32);
  TD_ALIAS_(T_I64, TD_INT64);
  TD_ALIAS_(T_U8, TD_UINT8);
  TD_ALIAS_(T_U16, TD_UINT16);
  TD_ALIAS_(T_U32, TD_UINT32);
  TD_ALIAS_(T_U64, TD_UINT64);
  TD_ALIAS_(T_FLOAT, TD_FLOAT);
  TD_ALIAS_(T_DOUBLE, TD_DOUBLE);
  TD_ALIAS_(T_BOOL, TD_BOOL);
  TD_ALIAS_(T_CHAR, TD_CHAR);
  TD_ALIAS_(T_WCHAR, TD_WCHAR);
  TD_ALIAS_(T_VOID, TD_VOID);
  TD_ALIAS_(T_NSIDPTR, TD_NSIDPTR);
  TD_ALIAS_(T_CHAR_STR, TD_PSTRING);
  TD_ALIAS_(T_WCHAR_STR, TD_PWSTRING);
  TD_ALIAS_(T_INTERFACE, TD_INTERFACE_TYPE);
  TD_ALIAS_(T_INTERFACE_IS, TD_INTERFACE_IS_TYPE);
  TD_ALIAS_(T_LEGACY_ARRAY, TD_LEGACY_ARRAY);
  TD_ALIAS_(T_PSTRING_SIZE_IS, TD_PSTRING_SIZE_IS);
  TD_ALIAS_(T_PWSTRING_SIZE_IS, TD_PWSTRING_SIZE_IS);
  TD_ALIAS_(T_UTF8STRING, TD_UTF8STRING);
  TD_ALIAS_(T_CSTRING, TD_CSTRING);
  TD_ALIAS_(T_ASTRING, TD_ASTRING);
  TD_ALIAS_(T_NSID, TD_NSID);
  TD_ALIAS_(T_JSVAL, TD_JSVAL);
  TD_ALIAS_(T_DOMOBJECT, TD_DOMOBJECT);
  TD_ALIAS_(T_PROMISE, TD_PROMISE);
  TD_ALIAS_(T_ARRAY, TD_ARRAY);
#undef TD_ALIAS_

  ////////////////////////////////////////////////////////////////
  // Ensure these fields are in the same order as xptcodegen.py //
  ////////////////////////////////////////////////////////////////

  uint8_t mTag : 5;

  // Parameter bitflags are packed into the XPTTypeDescriptor to save space.
  // When the TypeDescriptor is not in a parameter, these flags are ignored.
  uint8_t mInParam : 1;
  uint8_t mOutParam : 1;
  uint8_t mOptionalParam : 1;

  // The data for the different variants is stored in these two data fields.
  // These should only be accessed via the getter methods above, which will
  // assert if the tag is invalid.
  uint8_t mData1;
  uint8_t mData2;
};

// The fields in nsXPTType were carefully ordered to minimize size.
static_assert(sizeof(nsXPTType) == 3, "wrong size");

/*
 * A nsXPTParamInfo is used to describe either a single argument to a method or
 * a method's result. It stores its flags in the type descriptor to save space.
 */
struct nsXPTParamInfo {
  bool IsIn() const { return mType.mInParam; }
  bool IsOut() const { return mType.mOutParam; }
  bool IsOptional() const { return mType.mOptionalParam; }
  bool IsShared() const { return false; }  // XXX remove (backcompat)

  // Get the type of this parameter.
  const nsXPTType& Type() const { return mType; }
  const nsXPTType& GetType() const {
    return Type();
  }  // XXX remove (backcompat)

  // Whether this parameter is passed indirectly on the stack. All out/inout
  // params are passed indirectly, and complex types are always passed
  // indirectly.
  bool IsIndirect() const { return IsOut() || Type().IsComplex(); }

  ////////////////////////////////////////////////////////////////
  // Ensure these fields are in the same order as xptcodegen.py //
  ////////////////////////////////////////////////////////////////

  nsXPTType mType;
};

// The fields in nsXPTParamInfo were carefully ordered to minimize size.
static_assert(sizeof(nsXPTParamInfo) == 3, "wrong size");

/*
 * A nsXPTMethodInfo is used to describe a single interface method.
 */
struct nsXPTMethodInfo {
  bool IsGetter() const { return mGetter; }
  bool IsSetter() const { return mSetter; }
  bool IsReflectable() const { return mReflectable; }
  bool IsSymbol() const { return mIsSymbol; }
  bool WantsOptArgc() const { return mOptArgc; }
  bool WantsContext() const { return mContext; }
  uint8_t ParamCount() const { return mNumParams; }

  const char* Name() const {
    MOZ_ASSERT(!IsSymbol());
    return xpt::detail::GetString(mName);
  }
  const nsXPTParamInfo& Param(uint8_t aIndex) const {
    MOZ_ASSERT(aIndex < mNumParams);
    return xpt::detail::GetParam(mParams + aIndex);
  }

  bool HasRetval() const { return mHasRetval; }
  const nsXPTParamInfo* GetRetval() const {
    return mHasRetval ? &Param(mNumParams - 1) : nullptr;
  }

  // If this is an [implicit_jscontext] method, returns the index of the
  // implicit JSContext* argument in the C++ method's argument list.
  // Otherwise returns UINT8_MAX.
  uint8_t IndexOfJSContext() const {
    if (!WantsContext()) {
      return UINT8_MAX;
    }
    if (IsGetter() || IsSetter()) {
      // Getters/setters always have the context as first argument.
      return 0;
    }
    // The context comes before the return value, if there is one.
    MOZ_ASSERT_IF(HasRetval(), ParamCount() > 0);
    return ParamCount() - uint8_t(HasRetval());
  }

  /////////////////////////////////////////////
  // nsXPTMethodInfo backwards compatibility //
  /////////////////////////////////////////////

  const char* GetName() const { return Name(); }

  JS::SymbolCode GetSymbolCode() const {
    MOZ_ASSERT(IsSymbol());
    return JS::SymbolCode(mName);
  }

  JS::Symbol* GetSymbol(JSContext* aCx) const {
    return JS::GetWellKnownSymbol(aCx, GetSymbolCode());
  }

  void GetSymbolDescription(JSContext* aCx, nsACString& aID) const;

  uint8_t GetParamCount() const { return ParamCount(); }
  const nsXPTParamInfo& GetParam(uint8_t aIndex) const { return Param(aIndex); }

  ////////////////////////////////////////////////////////////////
  // Ensure these fields are in the same order as xptcodegen.py //
  ////////////////////////////////////////////////////////////////

  uint32_t mName;    // Index into xpt::detail::sStrings.
  uint16_t mParams;  // Index into xpt::detail::sParams.
  uint8_t mNumParams;

  uint8_t mGetter : 1;
  uint8_t mSetter : 1;
  uint8_t mReflectable : 1;
  uint8_t mOptArgc : 1;
  uint8_t mContext : 1;
  uint8_t mHasRetval : 1;
  uint8_t mIsSymbol : 1;
};

// The fields in nsXPTMethodInfo were carefully ordered to minimize size.
static_assert(sizeof(nsXPTMethodInfo) == 8, "wrong size");

/**
 * A nsXPTConstantInfo is used to describe a single interface constant.
 */
struct nsXPTConstantInfo {
  const char* Name() const { return xpt::detail::GetString(mName); }

  JS::Value JSValue() const {
    if (mSigned || mValue <= uint32_t(INT32_MAX)) {
      return JS::Int32Value(int32_t(mValue));
    }
    return JS::DoubleValue(mValue);
  }

  ////////////////////////////////////////////////////////////////
  // Ensure these fields are in the same order as xptcodegen.py //
  ////////////////////////////////////////////////////////////////

  uint32_t mName : 31;  // Index into xpt::detail::mStrings.

  // Whether the value should be interpreted as a int32_t or uint32_t.
  uint32_t mSigned : 1;
  uint32_t mValue;  // The value stored as a u32
};

// The fields in nsXPTConstantInfo were carefully ordered to minimize size.
static_assert(sizeof(nsXPTConstantInfo) == 8, "wrong size");

/**
 * Object representing the information required to wrap and unwrap DOMObjects.
 *
 * This object will not live in rodata as it contains relocations.
 */
struct nsXPTDOMObjectInfo {
  nsresult Unwrap(JS::HandleValue aHandle, void** aObj, JSContext* aCx) const {
    return mUnwrap(aHandle, aObj, aCx);
  }

  bool Wrap(JSContext* aCx, void* aObj, JS::MutableHandleValue aHandle) const {
    return mWrap(aCx, aObj, aHandle);
  }

  void Cleanup(void* aObj) const { return mCleanup(aObj); }

  ////////////////////////////////////////////////////////////////
  // Ensure these fields are in the same order as xptcodegen.py //
  ////////////////////////////////////////////////////////////////

  nsresult (*mUnwrap)(JS::HandleValue aHandle, void** aObj, JSContext* aCx);
  bool (*mWrap)(JSContext* aCx, void* aObj, JS::MutableHandleValue aHandle);
  void (*mCleanup)(void* aObj);
};

namespace xpt {
namespace detail {

// The UntypedTArray type allows low-level access from XPConnect to nsTArray
// internals without static knowledge of the array element type in question.
class UntypedTArray : public nsTArray_base<nsTArrayFallibleAllocator,
                                           nsTArray_CopyWithMemutils> {
 public:
  void* Elements() const { return static_cast<void*>(Hdr() + 1); }

  // Changes the length and capacity to be at least large enough for aTo
  // elements.
  bool SetLength(const nsXPTType& aEltTy, uint32_t aTo) {
    if (!EnsureCapacity<nsTArrayFallibleAllocator>(aTo, aEltTy.Stride())) {
      return false;
    }

    if (mHdr != EmptyHdr()) {
      mHdr->mLength = aTo;
    }

    return true;
  }

  // Free backing memory for the nsTArray object.
  void Clear() {
    if (mHdr != EmptyHdr() && !UsesAutoArrayBuffer()) {
      nsTArrayFallibleAllocator::Free(mHdr);
    }
    mHdr = EmptyHdr();
  }
};

//////////////////////////////////////////////
// Raw typelib data stored in const statics //
//////////////////////////////////////////////

// XPIDL information
extern const nsXPTInterfaceInfo sInterfaces[];
extern const nsXPTType sTypes[];
extern const nsXPTParamInfo sParams[];
extern const nsXPTMethodInfo sMethods[];
extern const nsXPTConstantInfo sConsts[];
extern const nsXPTDOMObjectInfo sDOMObjects[];

extern const char sStrings[];

//////////////////////////////////////
// Helper Methods for fetching data //
//////////////////////////////////////

inline const nsXPTInterfaceInfo* GetInterface(uint16_t aIndex) {
  if (aIndex > 0 && aIndex <= sInterfacesSize) {
    return &sInterfaces[aIndex - 1];  // 1-based as 0 is a marker.
  }
  return nullptr;
}

inline const nsXPTType& GetType(uint16_t aIndex) { return sTypes[aIndex]; }

inline const nsXPTParamInfo& GetParam(uint16_t aIndex) {
  return sParams[aIndex];
}

inline const nsXPTMethodInfo& GetMethod(uint16_t aIndex) {
  return sMethods[aIndex];
}

inline const nsXPTConstantInfo& GetConstant(uint16_t aIndex) {
  return sConsts[aIndex];
}

inline const nsXPTDOMObjectInfo& GetDOMObjectInfo(uint16_t aIndex) {
  return sDOMObjects[aIndex];
}

inline const char* GetString(uint32_t aIndex) { return &sStrings[aIndex]; }

}  // namespace detail
}  // namespace xpt

#define XPT_FOR_EACH_ARITHMETIC_TYPE(MACRO) \
  MACRO(TD_INT8, int8_t)                    \
  MACRO(TD_INT16, int16_t)                  \
  MACRO(TD_INT32, int32_t)                  \
  MACRO(TD_INT64, int64_t)                  \
  MACRO(TD_UINT8, uint8_t)                  \
  MACRO(TD_UINT16, uint16_t)                \
  MACRO(TD_UINT32, uint32_t)                \
  MACRO(TD_UINT64, uint64_t)                \
  MACRO(TD_FLOAT, float)                    \
  MACRO(TD_DOUBLE, double)                  \
  MACRO(TD_BOOL, bool)                      \
  MACRO(TD_CHAR, char)                      \
  MACRO(TD_WCHAR, char16_t)

#define XPT_FOR_EACH_POINTER_TYPE(MACRO)    \
  MACRO(TD_VOID, void*)                     \
  MACRO(TD_NSIDPTR, nsID*)                  \
  MACRO(TD_PSTRING, char*)                  \
  MACRO(TD_PWSTRING, wchar_t*)              \
  MACRO(TD_INTERFACE_TYPE, nsISupports*)    \
  MACRO(TD_INTERFACE_IS_TYPE, nsISupports*) \
  MACRO(TD_LEGACY_ARRAY, void*)             \
  MACRO(TD_PSTRING_SIZE_IS, char*)          \
  MACRO(TD_PWSTRING_SIZE_IS, wchar_t*)      \
  MACRO(TD_DOMOBJECT, void*)                \
  MACRO(TD_PROMISE, mozilla::dom::Promise*)

#define XPT_FOR_EACH_COMPLEX_TYPE(MACRO) \
  MACRO(TD_UTF8STRING, nsCString)        \
  MACRO(TD_CSTRING, nsCString)           \
  MACRO(TD_ASTRING, nsString)            \
  MACRO(TD_NSID, nsID)                   \
  MACRO(TD_JSVAL, JS::Value)             \
  MACRO(TD_ARRAY, xpt::detail::UntypedTArray)

#define XPT_FOR_EACH_TYPE(MACRO)      \
  XPT_FOR_EACH_ARITHMETIC_TYPE(MACRO) \
  XPT_FOR_EACH_POINTER_TYPE(MACRO)    \
  XPT_FOR_EACH_COMPLEX_TYPE(MACRO)

inline size_t nsXPTType::Stride() const {
  // Compute the stride to use when walking an array of the given type.
  switch (Tag()) {
#define XPT_TYPE_STRIDE(tag, type) \
  case tag:                        \
    return sizeof(type);
    XPT_FOR_EACH_TYPE(XPT_TYPE_STRIDE)
#undef XPT_TYPE_STRIDE
  }

  MOZ_CRASH("Unknown type");
}

#endif /* xptinfo_h */
