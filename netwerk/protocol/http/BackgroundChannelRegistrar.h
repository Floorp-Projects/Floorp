/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_BackgroundChannelRegistrar_h__
#define mozilla_net_BackgroundChannelRegistrar_h__

#include "nsIBackgroundChannelRegistrar.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {
namespace net {

class HttpBackgroundChannelParent;
class HttpChannelParent;

class BackgroundChannelRegistrar final : public nsIBackgroundChannelRegistrar
{
  typedef nsRefPtrHashtable<nsUint64HashKey, HttpChannelParent>
          ChannelHashtable;
  typedef nsRefPtrHashtable<nsUint64HashKey, HttpBackgroundChannelParent>
          BackgroundChannelHashtable;
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBACKGROUNDCHANNELREGISTRAR

  explicit BackgroundChannelRegistrar();

private:
  virtual ~BackgroundChannelRegistrar();

  // A helper function for BackgroundChannelRegistrar itself to callback
  // HttpChannelParent and HttpBackgroundChannelParent when both objects are
  // ready. aChannelParent and aBgParent is the pair of HttpChannelParent and
  // HttpBackgroundChannelParent that should be linked together.
  void NotifyChannelLinked(HttpChannelParent* aChannelParent,
                           HttpBackgroundChannelParent* aBgParent);

  // Store unlinked HttpChannelParent objects.
  ChannelHashtable mChannels;

  // Store unlinked HttpBackgroundChannelParent objects.
  BackgroundChannelHashtable mBgChannels;

};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_BackgroundChannelRegistrar_h__
