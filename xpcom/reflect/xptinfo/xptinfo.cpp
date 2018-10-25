/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "xptinfo.h"
#include "nsISupports.h"
#include "mozilla/dom/DOMJSClass.h"
#include "mozilla/ArrayUtils.h"

#include "jsfriendapi.h"

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

////////////////////////////////////
// nsXPTMethodInfo symbol helpers //
////////////////////////////////////

void
nsXPTMethodInfo::GetSymbolDescription(JSContext* aCx, nsACString& aID) const
{
  JS::RootedSymbol symbol(aCx, GetSymbol(aCx));
  JSString* desc = JS::GetSymbolDescription(symbol);
  MOZ_ASSERT(JS_StringHasLatin1Chars(desc));

  JS::AutoAssertNoGC nogc(aCx);
  size_t length;
  const JS::Latin1Char* chars = JS_GetLatin1StringCharsAndLength(
    aCx, nogc, desc, &length);

  aID.AssignASCII(reinterpret_cast<const char*>(chars), length);
}
