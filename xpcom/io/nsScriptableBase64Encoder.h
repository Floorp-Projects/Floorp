/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScriptableBase64Encoder_h__
#define nsScriptableBase64Encoder_h__

#include "nsIScriptableBase64Encoder.h"
#include "mozilla/Attributes.h"

#define NS_SCRIPTABLEBASE64ENCODER_CID                 \
  {0xaaf68860, 0xf849, 0x40ee,                         \
   {0xbb, 0x7a, 0xb2, 0x29, 0xbc, 0xe0, 0x36, 0xa3} }
#define NS_SCRIPTABLEBASE64ENCODER_CONTRACTID \
  "@mozilla.org/scriptablebase64encoder;1"

class nsScriptableBase64Encoder MOZ_FINAL : public nsIScriptableBase64Encoder
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISCRIPTABLEBASE64ENCODER
private:
  ~nsScriptableBase64Encoder() {}
};

#endif
