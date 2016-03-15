/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _adivertablechannelparent_h_
#define _adivertablechannelparent_h_

#include "nsISupports.h"

class nsIStreamListener;

namespace mozilla {
namespace net {

// To be implemented by a channel's parent actors, e.g. HttpChannelParent
// and FTPChannelParent. Used by ChannelDiverterParent to divert
// nsIStreamListener callbacks from the child process to a new
// listener in the parent process.
class ADivertableParentChannel : public nsISupports
{
public:
  // Called by ChannelDiverterParent::DivertTo(nsIStreamListener*).
  // The listener should now be used to received nsIStreamListener callbacks,
  // i.e. OnStartRequest, OnDataAvailable and OnStopRequest, as if it had been
  // passed to AsyncOpen for the channel. A reference to the listener will be
  // added and kept until OnStopRequest has completed.
  virtual void DivertTo(nsIStreamListener *aListener) = 0;

  // Called to suspend parent channel in ChannelDiverterParent constructor.
  virtual nsresult SuspendForDiversion() = 0;

  // While messages are diverted back from the child to the parent calls to
  // suspend/resume the channel must also suspend/resume the message diversion.
  // These two functions will be called by nsHttpChannel and nsFtpChannel
  // Suspend()/Resume() functions.
  virtual nsresult SuspendMessageDiversion() = 0;
  virtual nsresult ResumeMessageDiversion() = 0;
};

} // namespace net
} // namespace mozilla

#endif
