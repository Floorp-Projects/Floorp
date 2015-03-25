/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSecurityConsoleMessage_h__
#define nsSecurityConsoleMessage_h__
#include "nsISecurityConsoleMessage.h"
#include "nsString.h"

class nsSecurityConsoleMessage final : public nsISecurityConsoleMessage
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISECURITYCONSOLEMESSAGE

  nsSecurityConsoleMessage();

private:
  ~nsSecurityConsoleMessage();

protected:
  nsString mTag;
  nsString mCategory;
};

#define NS_SECURITY_CONSOLE_MESSAGE_CID \
  {0x43ebf210, 0x8a7b, 0x4ddb, {0xa8, 0x3d, 0xb8, 0x7c, 0x51, 0xa0, 0x58, 0xdb}}
#endif //nsSecurityConsoleMessage_h__
