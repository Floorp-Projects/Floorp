/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIMacDockSupport.h"
#include "nsIStandaloneNativeMenu.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsMacDockSupport : public nsIMacDockSupport
{
public:
  nsMacDockSupport() {}
  virtual ~nsMacDockSupport() {}
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMACDOCKSUPPORT

protected:
  nsCOMPtr<nsIStandaloneNativeMenu> mDockMenu;
  nsString mBadgeText;
};
