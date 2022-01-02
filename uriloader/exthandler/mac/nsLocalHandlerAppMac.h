/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSLOCALHANDLERAPPMAC_H_
#define NSLOCALHANDLERAPPMAC_H_

#include "nsLocalHandlerApp.h"

class nsLocalHandlerAppMac : public nsLocalHandlerApp {
 public:
  nsLocalHandlerAppMac() {}

  nsLocalHandlerAppMac(const char16_t* aName, nsIFile* aExecutable)
      : nsLocalHandlerApp(aName, aExecutable) {}

  nsLocalHandlerAppMac(const nsAString& aName, nsIFile* aExecutable)
      : nsLocalHandlerApp(aName, aExecutable) {}
  virtual ~nsLocalHandlerAppMac() {}

  NS_IMETHOD LaunchWithURI(
      nsIURI* aURI, mozilla::dom::BrowsingContext* aBrowsingContext) override;
  NS_IMETHOD GetName(nsAString& aName) override;
};

#endif /*NSLOCALHANDLERAPPMAC_H_*/
