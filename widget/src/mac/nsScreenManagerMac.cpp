/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsScreenManagerMac.h"
#include "nsScreenMac.h"
#include "nsCOMPtr.h"


class ScreenListItem
{
public:
  ScreenListItem ( GDHandle inGD, nsIScreen* inScreen )
    : mGD(inGD), mScreen(inScreen) { } ;
  
  GDHandle mGD;
  nsCOMPtr<nsIScreen> mScreen;
};


nsScreenManagerMac :: nsScreenManagerMac ( )
{
  NS_INIT_REFCNT();

  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenManagerMac :: ~nsScreenManagerMac()
{
  // walk our list of cached screens and delete them.
  for ( int i = 0; i < mScreenList.Count(); ++i ) {
    ScreenListItem* item = NS_REINTERPRET_CAST(ScreenListItem*, mScreenList[i]);
    delete item;
  }
}


// addref, release, QI
NS_IMPL_ISUPPORTS1(nsScreenManagerMac, nsIScreenManager)


//
// CreateNewScreenObject
//
// Utility routine. Creates a new screen object from the given device handle
//
nsIScreen* 
nsScreenManagerMac :: CreateNewScreenObject ( GDHandle inDevice )
{
  nsIScreen* retScreen = nsnull;
  
  // look through our screen list, hoping to find it. If it's not there,
  // add it and return the new one.
  for ( int i = 0; i < mScreenList.Count(); ++i ) {
    ScreenListItem* curr = NS_REINTERPRET_CAST(ScreenListItem*, mScreenList[i]);
    if ( inDevice == curr->mGD ) {
      NS_IF_ADDREF(retScreen = curr->mScreen.get());
      return retScreen;
    }
  } // for each screen.
  
  // didn't find it in the list, so add it and return that item.
  retScreen = new nsScreenMac ( inDevice );
  ScreenListItem* listItem = new ScreenListItem ( inDevice, retScreen );
  mScreenList.AppendElement ( listItem );
  
  NS_IF_ADDREF(retScreen);
  return retScreen;
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
nsScreenManagerMac :: ScreenForRect ( PRInt32 inLeft, PRInt32 inTop, PRInt32 inWidth, PRInt32 inHeight,
                                        nsIScreen **outScreen )
{
  if ( !(inWidth || inHeight) ) {
    NS_WARNING ( "trying to find screen for sizeless window, using primary monitor" );
    *outScreen = CreateNewScreenObject ( ::GetMainDevice() );    // addrefs
    return NS_OK;
  }

  Rect globalWindowBounds = { inTop, inLeft, inTop + inHeight, inLeft + inWidth };

  GDHandle currDevice = ::GetDeviceList();
  GDHandle deviceWindowIsOn = ::GetMainDevice();
  PRInt32 greatestArea = 0;
  while ( currDevice ) {
    if ( ::TestDeviceAttribute(currDevice, screenDevice) && ::TestDeviceAttribute(currDevice, screenActive) ) {
      // calc the intersection.
      Rect intersection;
      Rect devRect = (**currDevice).gdRect;
      ::SectRect ( &globalWindowBounds, &devRect, &intersection );
      PRInt32 intersectArea = (intersection.right - intersection.left) * 
                                  (intersection.bottom - intersection.top);
      if ( intersectArea > greatestArea ) {
        greatestArea = intersectArea;
        deviceWindowIsOn = currDevice;
     }      
    } // if device is a screen and visible
    currDevice = ::GetNextDevice(currDevice);
  } // foreach device in list

  *outScreen = CreateNewScreenObject ( deviceWindowIsOn );    // addrefs
  return NS_OK;
    
} // ScreenForRect


//
// GetPrimaryScreen
//
// The screen with the menubar/taskbar. This shouldn't be needed very
// often.
//
NS_IMETHODIMP 
nsScreenManagerMac :: GetPrimaryScreen(nsIScreen * *aPrimaryScreen) 
{
  *aPrimaryScreen = CreateNewScreenObject ( ::GetMainDevice() );    // addrefs  
  return NS_OK;
  
} // GetPrimaryScreen


//
// GetNumberOfScreens
//
// Returns how many physical screens are available.
//
NS_IMETHODIMP
nsScreenManagerMac :: GetNumberOfScreens(PRUint32 *aNumberOfScreens)
{
  *aNumberOfScreens = 0;
  GDHandle currDevice = ::GetDeviceList();
  while ( currDevice ) {
    if ( ::TestDeviceAttribute(currDevice, screenDevice) && ::TestDeviceAttribute(currDevice, screenActive) )
      ++(*aNumberOfScreens);
    currDevice = ::GetNextDevice(currDevice);
  }
  return NS_OK;
  
} // GetNumberOfScreens

