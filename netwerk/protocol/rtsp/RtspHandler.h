/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RtspHandler_h
#define RtspHandler_h

#include "nsIProtocolHandler.h"
#include "nsCOMPtr.h"
#include "nsString.h"

namespace mozilla {
namespace net {


class RtspHandler : public nsIProtocolHandler
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER

  RtspHandler() { }
  ~RtspHandler() { }
  const static int32_t kDefaultRtspPort = 554;
};

} // namespace net
} // namespace mozilla

#endif // RtspHandler_h
