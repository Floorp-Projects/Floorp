/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenManagerOS2_h___
#define nsScreenManagerOS2_h___

#include "nsIScreenManager.h"
#include "nsIScreen.h"
#include "nsCOMPtr.h"

//------------------------------------------------------------------------

class nsScreenManagerOS2 : public nsIScreenManager
{
public:
  nsScreenManagerOS2 ( );
  virtual ~nsScreenManagerOS2();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISCREENMANAGER

private:

  nsIScreen* CreateNewScreenObject ( ) ;

    // cache the primary screen object to avoid memory allocation every time
  nsCOMPtr<nsIScreen> mCachedMainScreen;

};

#endif  // nsScreenManagerOS2_h___ 
