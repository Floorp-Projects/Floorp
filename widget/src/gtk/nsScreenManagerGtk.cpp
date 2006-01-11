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

#include "nsScreenManagerGtk.h"
#include "nsScreenGtk.h"
#include "nsIComponentManager.h"
#include "nsRect.h"

#include <gdk/gdkx.h>

#ifdef MOZ_ENABLE_XINERAMA
// this header rocks!
extern "C"
{
#include <X11/extensions/Xinerama.h>
}
#endif /* MOZ_ENABLE_XINERAMA */

nsScreenManagerGtk :: nsScreenManagerGtk ( )
{
  NS_INIT_REFCNT();

  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
  mNumScreens = 0;
}


nsScreenManagerGtk :: ~nsScreenManagerGtk()
{
  // nothing to see here.
}


// addref, release, QI
NS_IMPL_ISUPPORTS1(nsScreenManagerGtk, nsIScreenManager)


// this function will make sure that everything has been initialized.
nsresult
nsScreenManagerGtk :: EnsureInit(void)
{
  if (!mCachedScreenArray) {
    mCachedScreenArray = do_CreateInstance("@mozilla.org/supports-array;1");
    if (!mCachedScreenArray) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
#ifdef MOZ_ENABLE_XINERAMA
    // get the number of screens via xinerama
    XineramaScreenInfo *screenInfo;
    if (XineramaIsActive(GDK_DISPLAY())) {
      screenInfo = XineramaQueryScreens(GDK_DISPLAY(), &mNumScreens);
    }
    else {
      screenInfo = NULL;
      mNumScreens = 1;
    }
#else
    mNumScreens = 1;
#endif
    // there will be < 2 screens if we are either not building with
    // xinerama support or xinerama isn't running on the current
    // display.
    if (mNumScreens < 2) {
      mNumScreens = 1;
      nsCOMPtr<nsISupports> screen = new nsScreenGtk();
      if (!screen)
        return NS_ERROR_OUT_OF_MEMORY;
      mCachedScreenArray->AppendElement(screen);
    }
    // If Xinerama is enabled and there's more than one screen, fill
    // in the info for all of the screens.  If that's not the case
    // then nsScreenGTK() defaults to the screen width + height
#ifdef MOZ_ENABLE_XINERAMA
    else {
#ifdef DEBUG
      printf("Xinerama superpowers activated for %d screens!\n", mNumScreens);
#endif
      int i;
      for (i=0; i < mNumScreens; i++) {
        nsScreenGtk *screen = new nsScreenGtk();
        if (!screen) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        // initialize this screen object
        screen->mXOrg = screenInfo[i].x_org;
        screen->mYOrg = screenInfo[i].y_org;
        screen->mWidth = screenInfo[i].width;
        screen->mHeight = screenInfo[i].height;
        screen->mScreenNum = screenInfo[i].screen_number;
        nsCOMPtr<nsISupports> screenSupports = screen;
        mCachedScreenArray->AppendElement(screenSupports);
      }
    }
#endif /* MOZ_ENABLE_XINERAMA */
  }
  return NS_OK;;
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
nsScreenManagerGtk :: ScreenForRect ( PRInt32 aX, PRInt32 aY,
                                      PRInt32 aWidth, PRInt32 aHeight,
                                      nsIScreen **aOutScreen )
{
  nsresult rv;
  rv = EnsureInit();
  if (NS_FAILED(rv)) {
    NS_ERROR("nsScreenManagerGtk::EnsureInit() failed from ScreenForRect\n");
    return rv;
  }
  // which screen ( index from zero ) should we return?
  PRUint32 which = 0;
  // Optimize for the common case.  If the number of screens is only
  // one then this will fall through with which == 0 and will get the
  // primary screen.
  if (mNumScreens > 1) {
    // walk the list of screens and find the one that has the most
    // surface area.
    PRUint32 count;
    mCachedScreenArray->Count(&count);
    PRUint32 i;
    PRUint32 area = 0;
    nsRect   windowRect(aX, aY, aWidth, aHeight);
    for (i=0; i < count; i++) {
      PRInt32  x, y, width, height;
      x = y = width = height = 0;
      nsCOMPtr<nsIScreen> screen;
      mCachedScreenArray->GetElementAt(i, getter_AddRefs(screen));
      screen->GetRect(&x, &y, &width, &height);
      // calculate the surface area
      nsRect screenRect(x, y, width, height);
      screenRect.IntersectRect(screenRect, windowRect);
      PRUint32 tempArea = screenRect.width * screenRect.height;
      if (tempArea >= area) {
        which = i;
        area = tempArea;
      }
    }
  }
  nsCOMPtr<nsIScreen> outScreen;
  mCachedScreenArray->GetElementAt(which, getter_AddRefs(outScreen));
  *aOutScreen = outScreen.get();
  NS_IF_ADDREF(*aOutScreen);
  return NS_OK;
    
} // ScreenForRect


//
// GetPrimaryScreen
//
// The screen with the menubar/taskbar. This shouldn't be needed very
// often.
//
NS_IMETHODIMP 
nsScreenManagerGtk :: GetPrimaryScreen(nsIScreen * *aPrimaryScreen) 
{
  nsresult rv;
  rv =  EnsureInit();
  if (NS_FAILED(rv)) {
    NS_ERROR("nsScreenManagerGtk::EnsureInit() failed from GetPrimaryScreen\n");
    return rv;
  }
  nsCOMPtr <nsIScreen> screen;
  mCachedScreenArray->GetElementAt(0, getter_AddRefs(screen));
  *aPrimaryScreen = screen.get();
  NS_IF_ADDREF(*aPrimaryScreen);
  return NS_OK;
  
} // GetPrimaryScreen


//
// GetNumberOfScreens
//
// Returns how many physical screens are available.
//
NS_IMETHODIMP
nsScreenManagerGtk :: GetNumberOfScreens(PRUint32 *aNumberOfScreens)
{
  nsresult rv;
  rv = EnsureInit();
  if (NS_FAILED(rv)) {
    NS_ERROR("nsScreenManagerGtk::EnsureInit() failed from GetNumberOfScreens\n");
    return rv;
  }
  *aNumberOfScreens = mNumScreens;
  return NS_OK;
  
} // GetNumberOfScreens

