/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsScreenManagerOS2.h"
#include "nsScreenOS2.h"


nsScreenManagerOS2 :: nsScreenManagerOS2 ( )
{
  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenManagerOS2 :: ~nsScreenManagerOS2()
{
  // nothing to see here.
}


// addref, release, QI
NS_IMPL_ISUPPORTS1(nsScreenManagerOS2, nsIScreenManager)


//
// CreateNewScreenObject
//
// Utility routine. Creates a new screen object from the given device handle
//
// NOTE: For this "single-monitor" impl, we just always return the cached primary
//        screen. This should change when a multi-monitor impl is done.
//
nsIScreen* 
nsScreenManagerOS2 :: CreateNewScreenObject (  )
{
  nsIScreen* retval = nullptr;
  if ( !mCachedMainScreen )
    mCachedMainScreen = new nsScreenOS2 ( );
  NS_IF_ADDREF(retval = mCachedMainScreen.get());
  
  return retval;
}


//
// ScreenForRect 
//
// Returns the screen that contains the rectangle. If the rect overlaps
// multiple screens, it picks the screen with the greatest area of intersection.
//
// The coordinates are in pixels (not twips) and in screen coordinates.
//
NS_IMETHODIMP
nsScreenManagerOS2 :: ScreenForRect ( int32_t /*inLeft*/, int32_t /*inTop*/, int32_t /*inWidth*/,
                                       int32_t /*inHeight*/, nsIScreen **outScreen )
{
  GetPrimaryScreen ( outScreen );
  return NS_OK;
    
} // ScreenForRect


//
// GetPrimaryScreen
//
// The screen with the menubar/taskbar. This shouldn't be needed very
// often.
//
NS_IMETHODIMP 
nsScreenManagerOS2 :: GetPrimaryScreen(nsIScreen * *aPrimaryScreen) 
{
  *aPrimaryScreen = CreateNewScreenObject();    // addrefs  
  return NS_OK;
  
} // GetPrimaryScreen


//
// GetNumberOfScreens
//
// Returns how many physical screens are available.
//
NS_IMETHODIMP
nsScreenManagerOS2 :: GetNumberOfScreens(uint32_t *aNumberOfScreens)
{
  *aNumberOfScreens = 1;
  return NS_OK;
  
} // GetNumberOfScreens

NS_IMETHODIMP
nsScreenManagerOS2::GetSystemDefaultScale(float *aDefaultScale)
{
  *aDefaultScale = 1.0f;
  return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerOS2 :: ScreenForNativeWidget(void *nativeWidget, nsIScreen **aScreen)
{
  *aScreen = CreateNewScreenObject();    // addrefs
  return NS_OK;
}
