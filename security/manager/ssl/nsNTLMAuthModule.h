/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNTLMAuthModule_h__
#define nsNTLMAuthModule_h__

#include "nsIAuthModule.h"
#include "nsString.h"

class nsNTLMAuthModule : public nsIAuthModule {
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

#endif  // nsNTLMAuthModule_h__
