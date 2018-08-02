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


////////////////////////////////////
// Constant Lookup Helper Methods //
////////////////////////////////////


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

const nsXPTConstantInfo&
nsXPTInterfaceInfo::Constant(uint16_t aIndex) const
{
  MOZ_ASSERT(aIndex < ConstantCount());

  if (const nsXPTInterfaceInfo* pi = GetParent()) {
    if (aIndex < pi->ConstantCount()) {
      return pi->Constant(aIndex);
    }
    aIndex -= pi->ConstantCount();
  }

  return xpt::detail::GetConstant(mConsts + aIndex);
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
  if (aIndex < ConstantCount()) {
    aConstant.set(Constant(aIndex).JSValue());
    *aName = moz_xstrdup(Constant(aIndex).Name());
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
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
