/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/AltDataOutputStreamParent.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS0(AltDataOutputStreamParent)

AltDataOutputStreamParent::AltDataOutputStreamParent(nsIOutputStream* aStream)
  : mOutputStream(aStream)
  , mStatus(NS_OK)
{
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");
}

AltDataOutputStreamParent::~AltDataOutputStreamParent()
{
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");
}

bool
AltDataOutputStreamParent::RecvWriteData(const nsCString& data)
{
  if (NS_FAILED(mStatus)) {
    Unused << SendError(mStatus);
    return true;
  }
  nsresult rv;
  uint32_t n;
  if (mOutputStream) {
    rv = mOutputStream->Write(data.BeginReading(), data.Length(), &n);
    MOZ_ASSERT(n == data.Length());
    if (NS_FAILED(rv)) {
      Unused << SendError(rv);
    }
  }
  return true;
}

bool
AltDataOutputStreamParent::RecvClose()
{
  if (NS_FAILED(mStatus)) {
    Unused << SendError(mStatus);
    return true;
  }
  nsresult rv;
  if (mOutputStream) {
    rv = mOutputStream->Close();
    if (NS_FAILED(rv)) {
      Unused << SendError(rv);
    }
    mOutputStream = nullptr;
  }
  return true;
}

void
AltDataOutputStreamParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

} // namespace net
} // namespace mozilla
