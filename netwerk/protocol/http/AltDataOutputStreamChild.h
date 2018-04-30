/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_AltDataOutputStreamChild_h
#define mozilla_net_AltDataOutputStreamChild_h

#include "mozilla/net/PAltDataOutputStreamChild.h"
#include "nsIOutputStream.h"

namespace mozilla {
namespace net {

class AltDataOutputStreamChild
  : public PAltDataOutputStreamChild
  , public nsIOutputStream
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
  explicit AltDataOutputStreamChild();

  void AddIPDLReference();
  void ReleaseIPDLReference();
  // Saves an error code which will be reported to the writer on the next call.
  virtual mozilla::ipc::IPCResult RecvError(const nsresult& err) override;
  virtual mozilla::ipc::IPCResult RecvDeleteSelf() override;

private:
  virtual ~AltDataOutputStreamChild() = default;
  // Sends data to the parent process in 256k chunks.
  bool WriteDataInChunks(const nsCString& data);

  bool mIPCOpen;
  // If there was an error opening the output stream or writing to it on the
  // parent side, this will be set to the error code. We check it before we
  // write so we can report an error to the consumer.
  nsresult mError;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_AltDataOutputStreamChild_h
