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
#include "js/Value.h"

// Forward Declarations
namespace mozilla {
namespace dom {
struct NativePropertyHooks;
} // namespace dom
} // namespace mozilla

struct nsXPTInterfaceInfo;
struct nsXPTType;
struct nsXPTParamInfo;
struct nsXPTMethodInfo;
struct nsXPTDOMObjectInfo;

// Internal helper methods.
namespace xpt {
namespace detail {

inline const nsXPTInterfaceInfo* GetInterface(uint16_t aIndex);
inline const nsXPTType& GetType(uint16_t aIndex);
inline const nsXPTParamInfo& GetParam(uint16_t aIndex);
inline const nsXPTMethodInfo& GetMethod(uint16_t aIndex);
inline const nsXPTDOMObjectInfo& GetDOMObjectInfo(uint16_t aIndex);
inline const char* GetString(uint32_t aIndex);

extern const uint16_t sInterfacesSize;

} // namespace detail
} // namespace xpt


/*
 * An Interface describes a single XPCOM interface, including all of its
 * methods. We don't record non-scriptable interfaces.
 */
struct nsXPTInterfaceInfo
{
  // High efficiency getters for Interfaces based on perfect hashes.
  static const nsXPTInterfaceInfo* ByIID(const nsIID& aIID);
  static const nsXPTInterfaceInfo* ByName(const char* aName);

  // These are only needed for Components_interfaces's enumerator.
  static const nsXPTInterfaceInfo* ByIndex(uint16_t aIndex) {
    // NOTE: We add 1 here, as the internal index 0 is reserved for null.
    return xpt::detail::GetInterface(aIndex + 1);
  }
  static uint16_t InterfaceCount() { return xpt::detail::sInterfacesSize; }


  // Interface flag getters
  bool IsScriptable() const { return true; } // XXX remove (backcompat)
  bool IsFunction() const { return mFunction; }
  bool IsBuiltinClass() const { return mBuiltinClass; }
  bool IsMainProcessScriptableOnly() const { return mMainProcessScriptableOnly; }

  const char* Name() const { return xpt::detail::GetString(mName); }
  const nsIID& IID() const { return mIID; }

  // Get the parent interface, or null if this interface doesn't have a parent.
  const nsXPTInterfaceInfo* GetParent() const {
    return xpt::detail::GetInterface(mParent);
  }

  // Do we have an ancestor interface with the given IID?
  bool HasAncestor(const nsIID& aIID) const;

  // Constant Getters and Setters.
  uint16_t ConstantCount() const;
  const char* Constant(uint16_t aIndex, JS::MutableHandleValue aConst) const;

  // Method Getters and Setters.
  uint16_t MethodCount() const { return mNumMethods; }
  const nsXPTMethodInfo& Method(uint16_t aIndex) const;


  //////////////////////////////////////////////
  // nsIInterfaceInfo backwards compatibility //
  //////////////////////////////////////////////

  nsresult GetName(char** aName) const;
  nsresult IsScriptable(bool* aRes) const;
  nsresult IsBuiltinClass(bool* aRes) const;
  nsresult GetParent(const nsXPTInterfaceInfo** aParent) const;
  nsresult GetMethodCount(uint16_t* aMethodCount) const;
  nsresult GetConstantCount(uint16_t* aConstantCount) const;
  nsresult GetMethodInfo(uint16_t aIndex, const nsXPTMethodInfo** aInfo) const;
  nsresult GetConstant(uint16_t aIndex,
                       JS::MutableHandleValue constant,
                       char** aName) const;
  nsresult GetTypeForParam(uint16_t aMethodIndex, const nsXPTParamInfo* aParam,
                           uint16_t aDimension, nsXPTType* aRetval) const;
  nsresult GetSizeIsArgNumberForParam(uint16_t aMethodIndex,
                                      const nsXPTParamInfo* aParam,
                                      uint16_t aDimension,
                                      uint8_t* aRetval) const;
  nsresult GetInterfaceIsArgNumberForParam(uint16_t aMethodIndex,
                                           const nsXPTParamInfo* aParam,
                                           uint8_t* aRetval) const;
  nsresult IsIID(const nsIID* aIID, bool* aIs) const;
  nsresult GetNameShared(const char** aName) const;
  nsresult GetIIDShared(const nsIID** aIID) const;
  nsresult IsFunction(bool* aRetval) const;
  nsresult HasAncestor(const nsIID* aIID, bool* aRetval) const;
  nsresult GetIIDForParamNoAlloc(uint16_t aMethodIndex,
                                 const nsXPTParamInfo* aParam,
                                 nsIID* aIID) const;
  nsresult IsMainProcessScriptableOnly(bool* aRetval) const;

  // XXX: We can probably get away with removing this method. A shim interface
  // _should_ never show up in code which calls EnsureResolved().
  bool EnsureResolved() const { return !mIsShim; }

  ////////////////////////////////////////////////////////////////
  // Ensure these fields are in the same order as xptcodegen.py //
  ////////////////////////////////////////////////////////////////

  nsID mIID;
  uint32_t mName; // Index into xpt::detail::sStrings

  uint16_t mParent : 14;
  uint16_t mBuiltinClass : 1;
  // XXX(nika): Do we need this if we don't have addons anymore?
  uint16_t mMainProcessScriptableOnly : 1;

  uint16_t mMethods; // Index into xpt::detail::sMethods

  uint16_t mConsts : 14; // Index into xpt::detail::sConsts
  uint16_t mIsShim : 1; // Is this interface a WebIDL shim?
  uint16_t mFunction : 1;

  uint8_t mNumMethods; // NOTE(24/04/18): largest=nsIDocShell (193)
  uint8_t mNumConsts; // NOTE(24/04/18): largest=nsIAccessibleRole (175)
};

// The fields in nsXPTInterfaceInfo were carefully ordered to minimize size.
static_assert(sizeof(nsXPTInterfaceInfo) == 28, "wrong size?");


/*
 * The following enum represents contains the different tag types which
 * can be found in nsXPTTypeInfo::mTag.
 *
 * WARNING: mTag is 5 bits wide, supporting at most 32 tags.
 */
enum nsXPTTypeTag : uint8_t
{
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
  TD_JSVAL             = 26,
  TD_DOMOBJECT         = 27
};


/*
 * A nsXPTType is a union used to identify the type of a method argument or
 * return value. The internal data is stored as an 5-bit tag, and two 8-bit
 * integers, to keep alignment requirements low.
 *
 * nsXPTType contains 3 extra bits, reserved for use by nsXPTParamInfo.
 */
struct nsXPTType
{
  nsXPTTypeTag Tag() const { return static_cast<nsXPTTypeTag>(mTag); }

  uint8_t ArgNum() const {
    MOZ_ASSERT(Tag() == TD_INTERFACE_IS_TYPE ||
               Tag() == TD_PSTRING_SIZE_IS ||
               Tag() == TD_PWSTRING_SIZE_IS ||
               Tag() == TD_ARRAY);
    return mData1;
  }

  const nsXPTType& ArrayElementType() const {
    MOZ_ASSERT(Tag() == TD_ARRAY);
    return xpt::detail::GetType(mData2);
  }

private:
  uint16_t Data16() const { return ((uint16_t)mData1 << 8) | mData2; }

public:
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

  // 'Arithmetic' here roughly means that the value is self-contained and
  // doesn't depend on anything else in memory (ie: not a pointer, not an
  // XPCOM object, not a jsval, etc).
  //
  // Supposedly this terminology comes from Harbison/Steele, but it's still
  // a rather crappy name. We'd change it if it wasn't used all over the
  // place in xptcall. :-(
  bool IsArithmetic() const { return Tag() <= TD_WCHAR; }

  // We used to abuse 'pointer' flag bit in typelib format quite extensively.
  // We've gotten rid of most of the cases, but there's still a fair amount
  // of refactoring to be done in XPCWrappedJSClass before we can safely stop
  // asking about this. In the mean time, we've got a temporary version of
  // IsPointer() that should do the right thing.
  bool deprecated_IsPointer() const {
    return !IsArithmetic() && Tag() != TD_JSVAL;
  }

  bool IsInterfacePointer() const {
    return Tag() == TD_INTERFACE_TYPE || Tag() == TD_INTERFACE_IS_TYPE;
  }

  bool IsArray() const { return Tag() == TD_ARRAY; }

  bool IsDependent() const {
    return Tag() == TD_INTERFACE_IS_TYPE || Tag() == TD_ARRAY ||
           Tag() == TD_PSTRING_SIZE_IS || Tag() == TD_PWSTRING_SIZE_IS;
  }

  bool IsStringClass() const {
    return Tag() == TD_DOMSTRING || Tag() == TD_ASTRING ||
           Tag() == TD_CSTRING || Tag() == TD_UTF8STRING;
  }

  ///////////////////////////////////////
  // nsXPTType backwards compatibility //
  ///////////////////////////////////////

  nsXPTType& operator=(uint8_t aPrefix) { mTag = aPrefix; return *this; }
  operator uint8_t() const { return TagPart(); };
  uint8_t TagPart() const { return mTag; };

  enum // Re-export TD_ interfaces from nsXPTType
  {
    T_I8                = TD_INT8             ,
    T_I16               = TD_INT16            ,
    T_I32               = TD_INT32            ,
    T_I64               = TD_INT64            ,
    T_U8                = TD_UINT8            ,
    T_U16               = TD_UINT16           ,
    T_U32               = TD_UINT32           ,
    T_U64               = TD_UINT64           ,
    T_FLOAT             = TD_FLOAT            ,
    T_DOUBLE            = TD_DOUBLE           ,
    T_BOOL              = TD_BOOL             ,
    T_CHAR              = TD_CHAR             ,
    T_WCHAR             = TD_WCHAR            ,
    T_VOID              = TD_VOID             ,
    T_IID               = TD_PNSIID           ,
    T_DOMSTRING         = TD_DOMSTRING        ,
    T_CHAR_STR          = TD_PSTRING          ,
    T_WCHAR_STR         = TD_PWSTRING         ,
    T_INTERFACE         = TD_INTERFACE_TYPE   ,
    T_INTERFACE_IS      = TD_INTERFACE_IS_TYPE,
    T_ARRAY             = TD_ARRAY            ,
    T_PSTRING_SIZE_IS   = TD_PSTRING_SIZE_IS  ,
    T_PWSTRING_SIZE_IS  = TD_PWSTRING_SIZE_IS ,
    T_UTF8STRING        = TD_UTF8STRING       ,
    T_CSTRING           = TD_CSTRING          ,
    T_ASTRING           = TD_ASTRING          ,
    T_JSVAL             = TD_JSVAL            ,
    T_DOMOBJECT         = TD_DOMOBJECT
  };

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
struct nsXPTParamInfo
{
  bool IsIn() const { return mType.mInParam; }
  bool IsOut() const { return mType.mOutParam && !IsDipper(); }
  bool IsOptional() const { return mType.mOptionalParam; }
  bool IsShared() const { return false; } // XXX remove (backcompat)

  // Get the type of this parameter.
  const nsXPTType& Type() const { return mType; }
  const nsXPTType& GetType() const { return Type(); } // XXX remove (backcompat)

  // Dipper types are one of the more inscrutable aspects of xpidl. In a
  // nutshell, dippers are empty container objects, created and passed by the
  // caller, and filled by the callee. The callee receives a fully- formed
  // object, and thus does not have to construct anything. But the object is
  // functionally empty, and the callee is responsible for putting something
  // useful inside of it.
  //
  // Dipper types are treated as `in` parameters when declared as an `out`
  // parameter. For this reason, dipper types are sometimes referred to as 'out
  // parameters masquerading as in'. The burden of maintaining this illusion
  // falls mostly on XPConnect, which creates the empty containers, and harvest
  // the results after the call.
  //
  // Currently, the only dipper types are the string classes.
  //
  // XXX: Dipper types may be able to go away? (bug 677784)
  bool IsDipper() const { return mType.mOutParam && IsStringClass(); }

  // Whether this parameter is passed indirectly on the stack. This mainly
  // applies to out/inout params, but we use it unconditionally for certain
  // types.
  bool IsIndirect() const { return IsOut() || mType.Tag() == TD_JSVAL; }

  bool IsStringClass() const { return mType.IsStringClass(); }

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
struct nsXPTMethodInfo
{
  bool IsGetter() const { return mGetter; }
  bool IsSetter() const { return mSetter; }
  bool IsNotXPCOM() const { return mNotXPCOM; }
  bool IsHidden() const { return mHidden; }
  bool WantsOptArgc() const { return mOptArgc; }
  bool WantsContext() const { return mContext; }
  uint8_t ParamCount() const { return mNumParams; }

  const char* Name() const {
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

  /////////////////////////////////////////////
  // nsXPTMethodInfo backwards compatibility //
  /////////////////////////////////////////////

  const char* GetName() const { return Name(); }
  uint8_t GetParamCount() const { return ParamCount(); }
  const nsXPTParamInfo& GetParam(uint8_t aIndex) const {
    return Param(aIndex);
  }

  ////////////////////////////////////////////////////////////////
  // Ensure these fields are in the same order as xptcodegen.py //
  ////////////////////////////////////////////////////////////////

  uint32_t mName; // Index into xpt::detail::sStrings.
  uint16_t mParams; // Index into xpt::detail::sParams.
  uint8_t mNumParams;

  uint8_t mGetter : 1;
  uint8_t mSetter : 1;
  uint8_t mNotXPCOM : 1;
  uint8_t mHidden : 1;
  uint8_t mOptArgc : 1;
  uint8_t mContext : 1;
  uint8_t mHasRetval : 1;
  // uint8_t unused : 1;
};

// The fields in nsXPTMethodInfo were carefully ordered to minimize size.
static_assert(sizeof(nsXPTMethodInfo) == 8, "wrong size");

/**
 * Object representing the information required to wrap and unwrap DOMObjects.
 *
 * This object will not live in rodata as it contains relocations.
 */
struct nsXPTDOMObjectInfo
{
  nsresult Unwrap(JS::HandleValue aHandle, void** aObj) const {
    return mUnwrap(aHandle, aObj);
  }

  bool Wrap(JSContext* aCx, void* aObj, JS::MutableHandleValue aHandle) const {
    return mWrap(aCx, aObj, aHandle);
  }

  void Cleanup(void* aObj) const {
    return mCleanup(aObj);
  }

  ////////////////////////////////////////////////////////////////
  // Ensure these fields are in the same order as xptcodegen.py //
  ////////////////////////////////////////////////////////////////

  nsresult (*mUnwrap) (JS::HandleValue aHandle, void** aObj);
  bool (*mWrap) (JSContext* aCx, void* aObj, JS::MutableHandleValue aHandle);
  void (*mCleanup) (void* aObj);
};


namespace xpt {
namespace detail {

/**
 * The compressed representation of constants from XPT. Not part of the public
 * interface, as we also need to support Shim interfaces.
 */
struct ConstInfo
{
  ////////////////////////////////////////////////////////////////
  // Ensure these fields are in the same order as xptcodegen.py //
  ////////////////////////////////////////////////////////////////

  uint32_t mName : 31; // Index into xpt::detail::mStrings.

  // Whether the value should be interpreted as a int32_t or uint32_t.
  uint32_t mSigned: 1;
  uint32_t mValue; // The value stored as a u32
};

// The fields in ConstInfo were carefully ordered to minimize size.
static_assert(sizeof(ConstInfo) == 8, "wrong size");


//////////////////////////////////////////////
// Raw typelib data stored in const statics //
//////////////////////////////////////////////

// XPIDL information
extern const nsXPTInterfaceInfo sInterfaces[];
extern const nsXPTType sTypes[];
extern const nsXPTParamInfo sParams[];
extern const nsXPTMethodInfo sMethods[];
extern const nsXPTDOMObjectInfo sDOMObjects[];

extern const char sStrings[];
extern const ConstInfo sConsts[];

// shim constant information
extern const mozilla::dom::NativePropertyHooks* sPropHooks[];

// Perfect Hash Function backing data
static const uint16_t kPHFSize = 512;
extern const uint32_t sPHF_IIDs[]; // Length == kPHFSize
extern const uint32_t sPHF_Names[]; // Length == kPHFSize
extern const uint16_t sPHF_NamesIdxs[]; // Length == sInterfacesSize


//////////////////////////////////////
// Helper Methods for fetching data //
//////////////////////////////////////

inline const nsXPTInterfaceInfo*
GetInterface(uint16_t aIndex)
{
  if (aIndex > 0 && aIndex <= sInterfacesSize) {
    return &sInterfaces[aIndex - 1]; // 1-based as 0 is a marker.
  }
  return nullptr;
}

inline const nsXPTType&
GetType(uint16_t aIndex)
{
  return sTypes[aIndex];
}

inline const nsXPTParamInfo&
GetParam(uint16_t aIndex)
{
  return sParams[aIndex];
}

inline const nsXPTMethodInfo&
GetMethod(uint16_t aIndex)
{
  return sMethods[aIndex];
}

inline const nsXPTDOMObjectInfo&
GetDOMObjectInfo(uint16_t aIndex)
{
  return sDOMObjects[aIndex];
}

inline const char*
GetString(uint32_t aIndex)
{
  return &sStrings[aIndex];
}

} // namespace detail
} // namespace xpt

#endif /* xptinfo_h */
