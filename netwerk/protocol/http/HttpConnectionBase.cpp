/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

// Log on level :5, instead of default :4.
#undef LOG
#define LOG(args) LOG5(args)
#undef LOG_ENABLED
#define LOG_ENABLED() LOG5_ENABLED()

#define TLS_EARLY_DATA_NOT_AVAILABLE 0
#define TLS_EARLY_DATA_AVAILABLE_BUT_NOT_USED 1
#define TLS_EARLY_DATA_AVAILABLE_AND_USED 2

#include "mozilla/Telemetry.h"
#include "HttpConnectionBase.h"
#include "nsHttpHandler.h"
#include "nsIClassOfService.h"
#include "nsIOService.h"
#include "nsISocketTransport.h"

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// nsHttpConnection <public>
//-----------------------------------------------------------------------------

HttpConnectionBase::HttpConnectionBase() {
  LOG(("Creating HttpConnectionBase @%p\n", this));
}

void HttpConnectionBase::BootstrapTimings(TimingStruct times) {
  mBootstrappedTimingsSet = true;
  mBootstrappedTimings = times;
}

void HttpConnectionBase::SetSecurityCallbacks(
    nsIInterfaceRequestor* aCallbacks) {
  MutexAutoLock lock(mCallbacksLock);
  // This is called both on and off the main thread. For JS-implemented
  // callbacks, we requires that the call happen on the main thread, but
  // for C++-implemented callbacks we don't care. Use a pointer holder with
  // strict checking disabled.
  mCallbacks = new nsMainThreadPtrHolder<nsIInterfaceRequestor>(
      "nsHttpConnection::mCallbacks", aCallbacks, false);
}

void HttpConnectionBase::SetTrafficCategory(HttpTrafficCategory aCategory) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  if (aCategory == HttpTrafficCategory::eInvalid ||
      mTrafficCategory.Contains(aCategory)) {
    return;
  }
  Unused << mTrafficCategory.AppendElement(aCategory);
}

}  // namespace net
}  // namespace mozilla
