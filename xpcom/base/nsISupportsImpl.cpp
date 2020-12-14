/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupportsImpl.h"

#include "mozilla/Assertions.h"
#ifndef XPCOM_GLUE_AVOID_NSPR
#  include "nsPrintfCString.h"
#  include "nsThreadUtils.h"
#endif

using namespace mozilla;

nsresult NS_FASTCALL NS_TableDrivenQI(void* aThis, REFNSIID aIID,
                                      void** aInstancePtr,
                                      const QITableEntry* aEntries) {
  do {
    if (aIID.Equals(*aEntries->iid)) {
      nsISupports* r = reinterpret_cast<nsISupports*>(
          reinterpret_cast<char*>(aThis) + aEntries->offset);
      NS_ADDREF(r);
      *aInstancePtr = r;
      return NS_OK;
    }

    ++aEntries;
  } while (aEntries->iid);

  *aInstancePtr = nullptr;
  return NS_ERROR_NO_INTERFACE;
}

#ifndef XPCOM_GLUE_AVOID_NSPR
#  ifdef MOZ_THREAD_SAFETY_OWNERSHIP_CHECKS_SUPPORTED
nsAutoOwningThread::nsAutoOwningThread() : mThread(PR_GetCurrentThread()) {}

void nsAutoOwningThread::AssertCurrentThreadOwnsMe(const char* msg) const {
  if (MOZ_UNLIKELY(!IsCurrentThread())) {
    // `msg` is a string literal by construction.
    MOZ_CRASH_UNSAFE(msg);
  }
}

bool nsAutoOwningThread::IsCurrentThread() const {
  return mThread == PR_GetCurrentThread();
}

nsAutoOwningEventTarget::nsAutoOwningEventTarget()
    : mTarget(GetCurrentSerialEventTarget()) {
  mTarget->AddRef();
}

nsAutoOwningEventTarget::~nsAutoOwningEventTarget() {
  nsCOMPtr<nsISerialEventTarget> target = dont_AddRef(mTarget);
}

void nsAutoOwningEventTarget ::AssertCurrentThreadOwnsMe(
    const char* msg) const {
  if (MOZ_UNLIKELY(!IsCurrentThread())) {
    // `msg` is a string literal by construction.
    MOZ_CRASH_UNSAFE(msg);
  }
}

bool nsAutoOwningEventTarget::IsCurrentThread() const {
  return mTarget->IsOnCurrentThread();
}

#  endif

namespace mozilla::detail {
class ProxyDeleteVoidRunnable final : public CancelableRunnable {
 public:
  ProxyDeleteVoidRunnable(const char* aName, void* aPtr,
                          DeleteVoidFunction* aDeleteFunc)
      : CancelableRunnable(aName), mPtr(aPtr), mDeleteFunc(aDeleteFunc) {}

  NS_IMETHOD Run() override {
    if (mPtr) {
      mDeleteFunc(mPtr);
      mPtr = nullptr;
    }
    return NS_OK;
  }

  // Mimics the behaviour in `ProxyRelease`, freeing the resource when
  // cancelled.
  nsresult Cancel() override { return Run(); }

#  ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
  NS_IMETHOD GetName(nsACString& aName) override {
    if (mName) {
      aName.Append(mName);
    } else {
      aName.AssignLiteral("ProxyDeleteVoidRunnable");
    }
    return NS_OK;
  }
#  endif

 private:
  ~ProxyDeleteVoidRunnable() {
    if (mPtr) {
#  ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
      NS_WARNING(
          nsPrintfCString(
              "ProxyDeleteVoidRunnable for '%s' never run, leaking!", mName)
              .get());
#  else
      NS_WARNING("ProxyDeleteVoidRunnable never run, leaking!");
#  endif
    }
  }

  void* mPtr;
  DeleteVoidFunction* mDeleteFunc;
};

void ProxyDeleteVoid(const char* aName, nsISerialEventTarget* aTarget,
                     void* aPtr, DeleteVoidFunction* aDeleteFunc) {
  MOZ_ASSERT(aName);
  MOZ_ASSERT(aPtr);
  MOZ_ASSERT(aDeleteFunc);
  if (!aTarget) {
    NS_WARNING(nsPrintfCString("no target for '%s', leaking!", aName).get());
    return;
  }

  if (aTarget->IsOnCurrentThread()) {
    aDeleteFunc(aPtr);
    return;
  }
  nsresult rv = aTarget->Dispatch(
      MakeAndAddRef<ProxyDeleteVoidRunnable>(aName, aPtr, aDeleteFunc),
      NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING(nsPrintfCString("failed to post '%s', leaking!", aName).get());
  }
}
}  // namespace mozilla::detail
#endif
