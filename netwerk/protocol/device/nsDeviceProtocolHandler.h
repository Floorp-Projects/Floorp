/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceProtocolHandler_h_
#define nsDeviceProtocolHandler_h_

#include "nsIProtocolHandler.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

// {6b0ffe9e-d114-486b-aeb7-da62e7273ed5}
#define NS_DEVICEPROTOCOLHANDLER_CID                      \
{ 0x60ffe9e, 0xd114, 0x486b,                              \
    {0xae, 0xb7, 0xda, 0x62, 0xe7, 0x27, 0x3e, 0xd5} }

class nsDeviceProtocolHandler MOZ_FINAL : public nsIProtocolHandler {
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER

  nsDeviceProtocolHandler() {}
  ~nsDeviceProtocolHandler() {}

  nsresult Init();
};

#endif
