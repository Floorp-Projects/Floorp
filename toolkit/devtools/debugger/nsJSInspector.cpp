/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsJSInspector.h"
#include "nsIXPConnect.h"
#include "nsIJSContextStack.h"
#include "nsThreadUtils.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "jsdbgapi.h"
#include "mozilla/ModuleUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsMemory.h"
#include "nsArray.h"
#include "nsTArray.h"

#define JSINSPECTOR_CONTRACTID \
  "@mozilla.org/jsinspector;1"

#define JSINSPECTOR_CID \
{ 0xec5aa99c, 0x7abb, 0x4142, { 0xac, 0x5f, 0xaa, 0xb2, 0x41, 0x9e, 0x38, 0xe2 } }

namespace mozilla {
namespace jsinspector {

NS_GENERIC_FACTORY_CONSTRUCTOR(nsJSInspector)

NS_IMPL_ISUPPORTS1(nsJSInspector, nsIJSInspector)

nsJSInspector::nsJSInspector() : mNestedLoopLevel(0), mRequestors(1), mLastRequestor(JSVAL_NULL)
{
  nsTArray<JS::Value> mRequestors;
}

nsJSInspector::~nsJSInspector()
{
  mRequestors.Clear();
}

NS_IMETHODIMP
nsJSInspector::EnterNestedEventLoop(const JS::Value& requestor, uint32_t *out)
{
  nsresult rv;
  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mLastRequestor = requestor;
  mRequestors.AppendElement(requestor);

  uint32_t nestLevel = ++mNestedLoopLevel;
  if (NS_SUCCEEDED(stack->Push(nullptr))) {
    while (NS_SUCCEEDED(rv) && mNestedLoopLevel >= nestLevel) {
      if (!NS_ProcessNextEvent())
        rv = NS_ERROR_UNEXPECTED;
    }

    JSContext *cx;
    stack->Pop(&cx);
    NS_ASSERTION(cx == nullptr, "JSContextStack mismatch");
  } else {
    rv = NS_ERROR_FAILURE;
  }

  NS_ASSERTION(mNestedLoopLevel <= nestLevel,
               "nested event didn't unwind properly");

  if (mNestedLoopLevel == nestLevel) {
    mLastRequestor = mRequestors.ElementAt(--mNestedLoopLevel);
  }

  *out = mNestedLoopLevel;
  return rv;
}

NS_IMETHODIMP
nsJSInspector::ExitNestedEventLoop(uint32_t *out)
{
  if (mNestedLoopLevel > 0) {
    mRequestors.RemoveElementAt(--mNestedLoopLevel);
    if (mNestedLoopLevel > 0)
      mLastRequestor = mRequestors.ElementAt(mNestedLoopLevel - 1);
    else
      mLastRequestor = JSVAL_NULL;
  } else {
    return NS_ERROR_FAILURE;
  }

  *out = mNestedLoopLevel;

  return NS_OK;
}

NS_IMETHODIMP
nsJSInspector::GetEventLoopNestLevel(uint32_t *out)
{
  *out = mNestedLoopLevel;
  return NS_OK;
}

NS_IMETHODIMP
nsJSInspector::GetLastNestRequestor(JS::Value *out)
{
  *out = mLastRequestor;
  return NS_OK;
}

}
}

NS_DEFINE_NAMED_CID(JSINSPECTOR_CID);

static const mozilla::Module::CIDEntry kJSInspectorCIDs[] = {
  { &kJSINSPECTOR_CID, false, NULL, mozilla::jsinspector::nsJSInspectorConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kJSInspectorContracts[] = {
  { JSINSPECTOR_CONTRACTID, &kJSINSPECTOR_CID },
  { NULL }
};

static const mozilla::Module kJSInspectorModule = {
  mozilla::Module::kVersion,
  kJSInspectorCIDs,
  kJSInspectorContracts
};

NSMODULE_DEFN(jsinspector) = &kJSInspectorModule;
