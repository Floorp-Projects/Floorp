/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNTLMAuthModule_h__
#define nsNTLMAuthModule_h__

#include "nsIAuthModule.h"
#include "nsString.h"

class nsNTLMAuthModule : public nsIAuthModule
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTHMODULE

  nsNTLMAuthModule() : mNTLMNegotiateSent(false) {}

  nsresult InitTest();

  static void SetSendLM(bool sendLM);

protected:
  virtual ~nsNTLMAuthModule();

private:
  nsString mDomain;
  nsString mUsername;
  nsString mPassword;
  bool mNTLMNegotiateSent;
};

#define NS_NTLMAUTHMODULE_CONTRACTID \
  NS_AUTH_MODULE_CONTRACTID_PREFIX "ntlm"
#define NS_NTLMAUTHMODULE_CID \
{ /* a4e5888f-4fe4-4632-8e7e-745196ea7c70 */       \
  0xa4e5888f,                                      \
  0x4fe4,                                          \
  0x4632,                                          \
  {0x8e, 0x7e, 0x74, 0x51, 0x96, 0xea, 0x7c, 0x70} \
}

#endif // nsNTLMAuthModule_h__
