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

#include "nsScreenManagerPh.h"
#include "nsScreenPh.h"

#include "nsPhGfxLog.h"

nsScreenManagerPh :: nsScreenManagerPh( ) {
  NS_INIT_REFCNT();
  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
	}


nsScreenManagerPh :: ~nsScreenManagerPh( ) {
	}


// addref, release, QI
NS_IMPL_ISUPPORTS1(nsScreenManagerPh, nsIScreenManager)


//
// CreateNewScreenObject
//
// Utility routine. Creates a new screen object from the given device handle
//
// NOTE: For this "single-monitor" impl, we just always return the cached primary
//        screen. This should change when a multi-monitor impl is done.
//
nsIScreen* nsScreenManagerPh :: CreateNewScreenObject( ) {
  nsIScreen* retval = nsnull;
  if( !mCachedMainScreen ) mCachedMainScreen = new nsScreenPh( );
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
NS_IMETHODIMP nsScreenManagerPh :: ScreenForRect ( PRInt32 /*inLeft*/, PRInt32 /*inTop*/, PRInt32 /*inWidth*/, PRInt32 /*inHeight*/, nsIScreen **outScreen ) {
  GetPrimaryScreen( outScreen );
  return NS_OK;
	}

//
// GetPrimaryScreen
//
// The screen with the menubar/taskbar. This shouldn't be needed very
// often.
//
NS_IMETHODIMP nsScreenManagerPh :: GetPrimaryScreen( nsIScreen * *aPrimaryScreen ) {
  *aPrimaryScreen = CreateNewScreenObject();    // addrefs  
  return NS_OK;
	}

//
// GetNumberOfScreens
//
// Returns how many physical screens are available.
//
NS_IMETHODIMP nsScreenManagerPh :: GetNumberOfScreens( PRUint32 *aNumberOfScreens ) {
  *aNumberOfScreens = 1;
  return NS_OK;
	}

