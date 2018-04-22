/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "xptinfo.h"
#include "nsISupports.h"
#include "mozilla/dom/DOMJSClass.h"
#include "mozilla/ArrayUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace xpt::detail;

///////////////////////////////////////
// C++ Perfect Hash Helper Functions //
///////////////////////////////////////

// WARNING: This must match phf.py's implementation of the hash functions etc.
static const uint32_t FNV_OFFSET_BASIS = 0x811C9DC5;
static const uint32_t FNV_PRIME = 16777619;
static const uint32_t U32_HIGH_BIT = 0x80000000;

static uint32_t
Phf_DoHash(const void* bytes, uint32_t len, uint32_t h=FNV_OFFSET_BASIS)
{
  for (uint32_t i = 0; i < len; ++i) {
    h ^= reinterpret_cast<const uint8_t*>(bytes)[i];
    h *= FNV_PRIME;
  }
  return h;
}

static uint16_t
Phf_DoLookup(const void* aBytes, uint32_t aLen, const uint32_t* aIntr)
{
  uint32_t mid = aIntr[Phf_DoHash(aBytes, aLen) % kPHFSize];
  if (mid & U32_HIGH_BIT) {
    return mid & ~U32_HIGH_BIT;
  }
  return Phf_DoHash(aBytes, aLen, mid) % sInterfacesSize;
}
static_assert(kPHFSize == 512, "wrong phf size?");


////////////////////////////////////////
// PHF-based interface lookup methods //
////////////////////////////////////////

/* static */ const nsXPTInterfaceInfo*
nsXPTInterfaceInfo::ByIID(const nsIID& aIID)
{
  // Make sure the bytes in the IID are all little-endian, as xptcodegen.py
  // generates code assuming that nsIID is encoded in little-endian.
  nsIID iid = aIID;
  iid.m0 = NativeEndian::swapToLittleEndian(aIID.m0);
  iid.m1 = NativeEndian::swapToLittleEndian(aIID.m1);
  iid.m2 = NativeEndian::swapToLittleEndian(aIID.m2);

  uint16_t idx = Phf_DoLookup(&iid, sizeof(nsIID), sPHF_IIDs);
  MOZ_ASSERT(idx < sInterfacesSize, "index out of range");

  const nsXPTInterfaceInfo* found = &sInterfaces[idx];
  return found->IID() == aIID ? found : nullptr;
}
static_assert(sizeof(nsIID) == 16, "IIDs have the wrong size?");

/* static */ const nsXPTInterfaceInfo*
nsXPTInterfaceInfo::ByName(const char* aName)
{
  uint16_t idx = Phf_DoLookup(aName, strlen(aName), sPHF_Names);
  MOZ_ASSERT(idx < sInterfacesSize, "index out of range");

  idx = sPHF_NamesIdxs[idx];
  MOZ_ASSERT(idx < sInterfacesSize, "index out of range");

  const nsXPTInterfaceInfo* found = &sInterfaces[idx];
  return strcmp(found->Name(), aName) ? nullptr : found;
}


////////////////////////////////////
// Constant Lookup Helper Methods //
////////////////////////////////////

// XXX: Remove when shims are gone.
// This method either looks for the ConstantSpec at aIndex, or counts the
// number of constants for a given shim.
// NOTE: Only one of the aSpec and aCount outparameters should be provided.
// NOTE: If aSpec is not passed, aIndex is ignored.
// NOTE: aIndex must be in range if aSpec is passed.
static void
GetWebIDLConst(uint16_t aHookIdx, uint16_t aIndex,
               const ConstantSpec** aSpec, uint16_t* aCount)
{
  MOZ_ASSERT((aSpec && !aCount) || (aCount && !aSpec),
             "Only one of aSpec and aCount should be provided");

  const NativePropertyHooks* propHooks = sPropHooks[aHookIdx];

  uint16_t idx = 0;
  do {
    const NativeProperties* props[] = {
      propHooks->mNativeProperties.regular,
      propHooks->mNativeProperties.chromeOnly
    };
    for (size_t i = 0; i < ArrayLength(props); ++i) {
      auto prop = props[i];
      if (prop && prop->HasConstants()) {
        for (auto cs = prop->Constants()->specs; cs->name; ++cs) {
          // We have found one constant here.  We explicitly do not bother
          // calling isEnabled() here because it's OK to define potentially
          // extra constants on these shim interfaces.
          if (aSpec && idx == aIndex) {
            *aSpec = cs;
            return;
          }
          ++idx;
        }
      }
    }
  } while ((propHooks = propHooks->mProtoHooks));

  MOZ_ASSERT(aCount, "aIndex is out of bounds!");
  *aCount = idx;
}

bool
nsXPTInterfaceInfo::HasAncestor(const nsIID& aIID) const
{
  for (const auto* info = this; info; info = info->GetParent()) {
    if (info->IID() == aIID) {
      return true;
    }
  }
  return false;
}

uint16_t
nsXPTInterfaceInfo::ConstantCount() const
{
  if (!mIsShim) {
    return mNumConsts;
  }

  // Get the number of WebIDL constants.
  uint16_t num = 0;
  GetWebIDLConst(mConsts, 0, nullptr, &num);
  return num;
}

const char*
nsXPTInterfaceInfo::Constant(uint16_t aIndex, JS::MutableHandleValue aValue) const
{
  if (!mIsShim) {
    MOZ_ASSERT(aIndex < mNumConsts);

    if (const nsXPTInterfaceInfo* pi = GetParent()) {
      MOZ_ASSERT(!pi->mIsShim);
      if (aIndex < pi->mNumConsts) {
        return pi->Constant(aIndex, aValue);
      }
      aIndex -= pi->mNumConsts;
    }

    // Extract the value and name from the Constant Info.
    const ConstInfo& info = sConsts[mConsts + aIndex];
    if (info.mSigned || info.mValue <= (uint32_t)INT32_MAX) {
      aValue.set(JS::Int32Value((int32_t)info.mValue));
    } else {
      aValue.set(JS::DoubleValue(info.mValue));
    }
    return GetString(info.mName);
  }

  // Get a single WebIDL constant.
  const ConstantSpec* spec;
  GetWebIDLConst(mConsts, aIndex, &spec, nullptr);
  aValue.set(spec->value);
  return spec->name;
}

const nsXPTMethodInfo&
nsXPTInterfaceInfo::Method(uint16_t aIndex) const
{
  MOZ_ASSERT(aIndex < MethodCount());

  if (const nsXPTInterfaceInfo* pi = GetParent()) {
    if (aIndex < pi->MethodCount()) {
      return pi->Method(aIndex);
    }
    aIndex -= pi->MethodCount();
  }

  return xpt::detail::GetMethod(mMethods + aIndex);
}


////////////////////////////////////////////////
// nsIInterfaceInfo backcompat implementation //
////////////////////////////////////////////////

nsresult
nsXPTInterfaceInfo::GetName(char** aName) const
{
  *aName = moz_xstrdup(Name());
  return NS_OK;
}

nsresult
nsXPTInterfaceInfo::IsScriptable(bool* aRes) const
{
  *aRes = IsScriptable();
  return NS_OK;
}

nsresult
nsXPTInterfaceInfo::IsBuiltinClass(bool* aRes) const
{
  *aRes = IsBuiltinClass();
  return NS_OK;
}

nsresult
nsXPTInterfaceInfo::GetParent(const nsXPTInterfaceInfo** aParent) const
{
  *aParent = GetParent();
  return NS_OK;
}

nsresult
nsXPTInterfaceInfo::GetMethodCount(uint16_t* aMethodCount) const
{
  *aMethodCount = MethodCount();
  return NS_OK;
}

nsresult
nsXPTInterfaceInfo::GetConstantCount(uint16_t* aConstantCount) const
{
  *aConstantCount = ConstantCount();
  return NS_OK;
}

nsresult
nsXPTInterfaceInfo::GetMethodInfo(uint16_t aIndex, const nsXPTMethodInfo** aInfo) const
{
  *aInfo = aIndex < MethodCount() ? &Method(aIndex) : nullptr;
  return *aInfo ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsXPTInterfaceInfo::GetConstant(uint16_t aIndex,
                                JS::MutableHandleValue aConstant,
                                char** aName) const
{
  *aName = aIndex < ConstantCount()
    ? moz_xstrdup(Constant(aIndex, aConstant))
    : nullptr;
  return *aName ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsXPTInterfaceInfo::IsIID(const nsIID* aIID, bool* aIs) const
{
  *aIs = mIID == *aIID;
  return NS_OK;
}

nsresult
nsXPTInterfaceInfo::GetNameShared(const char** aName) const
{
  *aName = Name();
  return NS_OK;
}

nsresult
nsXPTInterfaceInfo::GetIIDShared(const nsIID** aIID) const
{
  *aIID = &IID();
  return NS_OK;
}

nsresult
nsXPTInterfaceInfo::IsFunction(bool* aRetval) const
{
  *aRetval = IsFunction();
  return NS_OK;
}

nsresult
nsXPTInterfaceInfo::HasAncestor(const nsIID* aIID, bool* aRetval) const
{
  *aRetval = HasAncestor(*aIID);
  return NS_OK;
}

nsresult
nsXPTInterfaceInfo::IsMainProcessScriptableOnly(bool* aRetval) const
{
  *aRetval = IsMainProcessScriptableOnly();
  return NS_OK;
}
