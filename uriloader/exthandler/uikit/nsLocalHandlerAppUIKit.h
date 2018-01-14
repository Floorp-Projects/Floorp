/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:cin:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nslocalhandlerappuikit_h_
#define nslocalhandlerappuikit_h_

#include "nsLocalHandlerApp.h"

class nsLocalHandlerAppUIKit final : public nsLocalHandlerApp
{
public:
  nsLocalHandlerAppUIKit()
  {}
  ~nsLocalHandlerAppUIKit()
  {}

  nsLocalHandlerAppUIKit(const char16_t* aName, nsIFile* aExecutable)
    : nsLocalHandlerApp(aName, aExecutable)
  {}

  nsLocalHandlerAppUIKit(const nsAString& aName, nsIFile* aExecutable)
    : nsLocalHandlerApp(aName, aExecutable)
  {}


  NS_IMETHOD LaunchWithURI(nsIURI* aURI, nsIInterfaceRequestor* aWindowContext) override;
};

#endif /* nslocalhandlerappuikit_h_ */
