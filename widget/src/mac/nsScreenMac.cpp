/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsScreenMac.h"
#if TARGET_CARBON
#include <MacWindows.h>
#else
#include <Menus.h>
#endif

nsScreenMac :: nsScreenMac ( GDHandle inScreen )
  : mScreen(inScreen)
{
  NS_INIT_REFCNT();

  NS_ASSERTION ( inScreen, "Passing null device to nsScreenMac" );
  
  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenMac :: ~nsScreenMac()
{
  // nothing to see here.
}


// addref, release, QI
NS_IMPL_ISUPPORTS1(nsScreenMac, nsIScreen)


NS_IMETHODIMP
nsScreenMac :: GetRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  *outLeft = (**mScreen).gdRect.left;
  *outTop = (**mScreen).gdRect.top;  
  *outWidth = (**mScreen).gdRect.right - (**mScreen).gdRect.left;
  *outHeight = (**mScreen).gdRect.bottom - (**mScreen).gdRect.top;

  return NS_OK;

} // GetRect


NS_IMETHODIMP 
nsScreenMac :: GetAvailRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  Rect adjustedRect;
  
#if TARGET_CARBON
  ::GetAvailableWindowPositioningBounds ( mScreen, &adjustedRect );
#else
  SubtractMenuBar ( (**mScreen).gdRect, &adjustedRect );
#endif

  *outLeft = adjustedRect.left;
  *outTop = adjustedRect.top;  
  *outWidth = adjustedRect.right - adjustedRect.left;
  *outHeight = adjustedRect.bottom - adjustedRect.top;

  return NS_OK;
  
} // GetRect



NS_IMETHODIMP 
nsScreenMac :: GetPixelDepth(PRInt32 *aPixelDepth)
{
  *aPixelDepth = (**(**mScreen).gdPMap).pixelSize;
  return NS_OK;

} // GetPixelDepth


NS_IMETHODIMP 
nsScreenMac :: GetColorDepth(PRInt32 *aColorDepth)
{
  *aColorDepth = (**(**mScreen).gdPMap).pixelSize;
  return NS_OK;

} // GetColorDepth


#if !TARGET_CARBON

//
// SubtractMenuBar
//
// Take out the menu bar from the reported size of this screen, but only
// if it is the primary screen
//
void
nsScreenMac :: SubtractMenuBar ( const Rect & inScreenRect, Rect* outAdjustedRect )
{
  // if the screen we're being asked about is the main screen (ie, has the menu
  // bar), then subract out the menubar from the rect that we're returning.
  *outAdjustedRect = inScreenRect;  
  if ( IsPrimaryScreen() )
    outAdjustedRect->top += ::GetMBarHeight();
}

#endif
