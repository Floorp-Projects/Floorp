/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenManagerWin_h___
#define nsScreenManagerWin_h___

#include "nsIScreenManager.h"

#include <windows.h>
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"

class nsIScreen;

//------------------------------------------------------------------------

class ScreenListItem
{
public:
  ScreenListItem ( HMONITOR inMon, nsIScreen* inScreen )
    : mMon(inMon), mScreen(inScreen) { } ;
  
  HMONITOR mMon;
  nsCOMPtr<nsIScreen> mScreen;
};

class nsScreenManagerWin MOZ_FINAL : public nsIScreenManager
{
public:
  nsScreenManagerWin ( );

  NS_DECL_ISUPPORTS
  NS_DECL_NSISCREENMANAGER

private:
  ~nsScreenManagerWin();

  nsIScreen* CreateNewScreenObject ( HMONITOR inScreen ) ;

  uint32_t mNumberOfScreens;

    // cache the screens to avoid the memory allocations
  nsAutoTArray<ScreenListItem, 8> mScreenList;

};

#endif  // nsScreenManagerWin_h___ 
