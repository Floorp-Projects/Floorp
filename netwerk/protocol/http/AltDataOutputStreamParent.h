/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_AltDataOutputStreamParent_h
#define mozilla_net_AltDataOutputStreamParent_h

#include "mozilla/net/PAltDataOutputStreamParent.h"
#include "nsIOutputStream.h"

namespace mozilla {
namespace net {

// Forwards data received from the content process to an output stream.
class AltDataOutputStreamParent
  : public PAltDataOutputStreamParent
  , public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  // Called from NeckoParent::AllocPAltDataOutputStreamParent which also opens
  // the output stream.
  // aStream may be null
  explicit AltDataOutputStreamParent(nsIOutputStream* aStream);

  // Called when data is received from the content process.
  // We proceed to write that data to the output stream.
  virtual bool RecvWriteData(const nsCString& data) override;
  // Called when AltDataOutputStreamChild::Close() is
  // Closes and nulls the output stream.
  virtual bool RecvClose() override;
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  // Sets an error that will be reported to the content process.
  void SetError(nsresult status) { mStatus = status; }

private:
  virtual ~AltDataOutputStreamParent();
  nsCOMPtr<nsIOutputStream> mOutputStream;
  // In case any error occurs mStatus will be != NS_OK, and this status code will
  // be sent to the content process asynchronously.
  nsresult mStatus;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_AltDataOutputStreamParent_h
