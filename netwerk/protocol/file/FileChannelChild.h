/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla__net__FileChannelChild_h
#define mozilla__net__FileChannelChild_h

#include "nsFileChannel.h"
#include "nsIChildChannel.h"
#include "nsISupportsImpl.h"

#include "mozilla/net/PFileChannelChild.h"

namespace mozilla {
namespace net {

class FileChannelChild : public nsFileChannel
                       , public nsIChildChannel
                       , public PFileChannelChild
{
public:
  explicit FileChannelChild(nsIURI *uri);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICHILDCHANNEL

protected:
  virtual void ActorDestroy(ActorDestroyReason why) override;

private:
  ~FileChannelChild() { };

  void AddIPDLReference();

  bool mIPCOpen;
};

} // namespace net
} // namespace mozilla

#endif /* mozilla__net__FileChannelChild_h */
