/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSLOCALHANDLERAPPWIN_H_
#define NSLOCALHANDLERAPPWIN_H_

#include "nsLocalHandlerApp.h"
#include "nsString.h"

class nsLocalHandlerAppWin : public nsLocalHandlerApp {
 public:
  nsLocalHandlerAppWin() {}

  nsLocalHandlerAppWin(const char16_t* aName, nsIFile* aExecutable)
      : nsLocalHandlerApp(aName, aExecutable) {}

  nsLocalHandlerAppWin(const nsAString& aName, nsIFile* aExecutable)
      : nsLocalHandlerApp(aName, aExecutable) {}
  virtual ~nsLocalHandlerAppWin() {}

  void SetAppIdOrName(const nsString& appIdOrName) {
    mAppIdOrName = appIdOrName;
  }

 protected:
  std::function<nsresult(nsString&)> GetPrettyNameOnNonMainThreadCallback()
      override;

 private:
  nsString mAppIdOrName;
};

#endif /*NSLOCALHANDLERAPPMAC_H_*/
