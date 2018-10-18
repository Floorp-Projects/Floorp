/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCodeCoverage.h"
#include "mozilla/CodeCoverageHandler.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

NS_IMPL_ISUPPORTS(nsCodeCoverage,
                  nsICodeCoverage)

nsCodeCoverage::nsCodeCoverage()
{
}

nsCodeCoverage::~nsCodeCoverage()
{
}

enum RequestType { Flush };

class ProcessCount final {
  NS_INLINE_DECL_REFCOUNTING(ProcessCount);

public:
  explicit ProcessCount(uint32_t c) : mCount(c) {}
  operator uint32_t() const { return mCount; }
  ProcessCount& operator--() { mCount--; return *this; }

private:
  ~ProcessCount() {}
  uint32_t mCount;
};

nsresult Request(JSContext* cx, Promise** aPromise, RequestType requestType)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  nsIGlobalObject* global = xpc::CurrentNativeGlobal(cx);
  if (NS_WARN_IF(!global)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(global, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  uint32_t processCount = 0;
  for (auto* cp : ContentParent::AllProcesses(ContentParent::eLive)) {
    Unused << cp;
    ++processCount;
  }

  if (requestType == RequestType::Flush) {
    CodeCoverageHandler::FlushCounters();
  }

  if (processCount == 0) {
    promise->MaybeResolveWithUndefined();
  } else {
    RefPtr<ProcessCount> processCountHolder(new ProcessCount(processCount));

    auto resolve = [processCountHolder, promise](bool unused) {
      if (--(*processCountHolder) == 0) {
        promise->MaybeResolveWithUndefined();
      }
    };

    auto reject = [promise](ResponseRejectReason aReason) {
      promise->MaybeReject(NS_ERROR_FAILURE);
    };

    for (auto* cp : ContentParent::AllProcesses(ContentParent::eLive)) {
      if (requestType == RequestType::Flush) {
        cp->SendFlushCodeCoverageCounters(resolve, reject);
      }
    }
  }

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP nsCodeCoverage::FlushCounters(JSContext *cx, Promise** aPromise)
{
  return Request(cx, aPromise, RequestType::Flush);
}
