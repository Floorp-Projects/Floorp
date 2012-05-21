/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsB2GProtocolHandler_h_
#define nsB2GProtocolHandler_h_

#include "nsIProtocolHandler.h"
#include "nsString.h"

// {e50d101a-9db2-466f-977c-ae6af19e3b2f}
#define NS_B2GPROTOCOLHANDLER_CID                      \
{ 0x50d101a, 0x9db2, 0x466f,                              \
    {0x97, 0x7c, 0xae, 0x6a, 0xf1, 0x9e, 0x3b, 0x2f} }

class nsB2GProtocolHandler : public nsIProtocolHandler {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER

  nsB2GProtocolHandler() {}
  ~nsB2GProtocolHandler() {}

  nsresult Init();
};

#endif
