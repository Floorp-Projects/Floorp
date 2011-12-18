/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Sylvain Pasche <sylvain.pasche@gmail.com>
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsScreenManagerGtk.h"
#include "nsScreenGtk.h"
#include "nsIComponentManager.h"
#include "nsRect.h"
#include "nsAutoPtr.h"

#define SCREEN_MANAGER_LIBRARY_LOAD_FAILED ((PRLibrary*)1)

#ifdef MOZ_X11
#include <gdk/gdkx.h>
// prototypes from Xinerama.h
typedef Bool (*_XnrmIsActive_fn)(Display *dpy);
typedef XineramaScreenInfo* (*_XnrmQueryScreens_fn)(Display *dpy, int *number);
#endif

#include <gtk/gtk.h>


static GdkFilterReturn
root_window_event_filter(GdkXEvent *aGdkXEvent, GdkEvent *aGdkEvent,
                         gpointer aClosure)
{
  nsScreenManagerGtk *manager = static_cast<nsScreenManagerGtk*>(aClosure);
#ifdef MOZ_X11
  XEvent *xevent = static_cast<XEvent*>(aGdkXEvent);

  // See comments in nsScreenGtk::Init below.
  switch (xevent->type) {
    case ConfigureNotify:
      manager->Init();
      break;
    case PropertyNotify:
      {
        XPropertyEvent *propertyEvent = &xevent->xproperty;
        if (propertyEvent->atom == manager->NetWorkareaAtom()) {
          manager->Init();
        }
      }
      break;
    default:
      break;
  }
#endif

  return GDK_FILTER_CONTINUE;
}

nsScreenManagerGtk :: nsScreenManagerGtk ( )
  : mXineramalib(nsnull)
  , mRootWindow(nsnull)
{
  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenManagerGtk :: ~nsScreenManagerGtk()
{
  if (mRootWindow) {
    gdk_window_remove_filter(mRootWindow, root_window_event_filter, this);
    g_object_unref(mRootWindow);
    mRootWindow = nsnull;
  }

  /* XineramaIsActive() registers a callback function close_display()
   * in X, which is to be called in XCloseDisplay(). This is the case
   * if Xinerama is active, even if only with one screen.
   *
   * We can't unload libXinerama.so.1 here because this will make
   * the address of close_display() registered in X to be invalid and
   * it will crash when XCloseDisplay() is called later. */
}


// addref, release, QI
NS_IMPL_ISUPPORTS1(nsScreenManagerGtk, nsIScreenManager)


// this function will make sure that everything has been initialized.
nsresult
nsScreenManagerGtk :: EnsureInit()
{
  if (mCachedScreenArray.Count() > 0)
    return NS_OK;

#if GTK_CHECK_VERSION(2,2,0)
  mRootWindow = gdk_get_default_root_window();
#else
  mRootWindow = GDK_ROOT_PARENT();
#endif // GTK_CHECK_VERSION(2,2,0)
  g_object_ref(mRootWindow);

  // GDK_STRUCTURE_MASK ==> StructureNotifyMask, for ConfigureNotify
  // GDK_PROPERTY_CHANGE_MASK ==> PropertyChangeMask, for PropertyNotify
  gdk_window_set_events(mRootWindow,
                        GdkEventMask(gdk_window_get_events(mRootWindow) |
                                     GDK_STRUCTURE_MASK |
                                     GDK_PROPERTY_CHANGE_MASK));
  gdk_window_add_filter(mRootWindow, root_window_event_filter, this);
#ifdef MOZ_X11
  mNetWorkareaAtom =
    XInternAtom(GDK_WINDOW_XDISPLAY(mRootWindow), "_NET_WORKAREA", False);
#endif

  return Init();
}

nsresult
nsScreenManagerGtk :: Init()
{
#ifdef MOZ_X11
  XineramaScreenInfo *screenInfo = NULL;
  int numScreens;

  if (!mXineramalib) {
    mXineramalib = PR_LoadLibrary("libXinerama.so.1");
    if (!mXineramalib) {
      mXineramalib = SCREEN_MANAGER_LIBRARY_LOAD_FAILED;
    }
  }
  if (mXineramalib && mXineramalib != SCREEN_MANAGER_LIBRARY_LOAD_FAILED) {
    _XnrmIsActive_fn _XnrmIsActive = (_XnrmIsActive_fn)
        PR_FindFunctionSymbol(mXineramalib, "XineramaIsActive");

    _XnrmQueryScreens_fn _XnrmQueryScreens = (_XnrmQueryScreens_fn)
        PR_FindFunctionSymbol(mXineramalib, "XineramaQueryScreens");
        
    // get the number of screens via xinerama
    if (_XnrmIsActive && _XnrmQueryScreens &&
        _XnrmIsActive(GDK_DISPLAY())) {
      screenInfo = _XnrmQueryScreens(GDK_DISPLAY(), &numScreens);
    }
  }

  // screenInfo == NULL if either Xinerama couldn't be loaded or
  // isn't running on the current display
  if (!screenInfo || numScreens == 1) {
    numScreens = 1;
#endif
    nsRefPtr<nsScreenGtk> screen;

    if (mCachedScreenArray.Count() > 0) {
      screen = static_cast<nsScreenGtk*>(mCachedScreenArray[0]);
    } else {
      screen = new nsScreenGtk();
      if (!screen || !mCachedScreenArray.AppendObject(screen)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    screen->Init(mRootWindow);
#ifdef MOZ_X11
  }
  // If Xinerama is enabled and there's more than one screen, fill
  // in the info for all of the screens.  If that's not the case
  // then nsScreenGTK() defaults to the screen width + height
  else {
#ifdef DEBUG
    printf("Xinerama superpowers activated for %d screens!\n", numScreens);
#endif
    for (int i = 0; i < numScreens; ++i) {
      nsRefPtr<nsScreenGtk> screen;
      if (mCachedScreenArray.Count() > i) {
        screen = static_cast<nsScreenGtk*>(mCachedScreenArray[i]);
      } else {
        screen = new nsScreenGtk();
        if (!screen || !mCachedScreenArray.AppendObject(screen)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }

      // initialize this screen object
      screen->Init(&screenInfo[i]);
    }
  }
  // Remove any screens that are no longer present.
  while (mCachedScreenArray.Count() > numScreens) {
    mCachedScreenArray.RemoveObjectAt(mCachedScreenArray.Count() - 1);
  }

  if (screenInfo) {
    XFree(screenInfo);
  }
#endif

  return NS_OK;
}


//
// ScreenForRect 
//
// Returns the screen that contains the rectangle. If the rect overlaps
// multiple screens, it picks the screen with the greatest area of intersection.
//
// The coordinates are in pixels (not app units) and in screen coordinates.
//
NS_IMETHODIMP
nsScreenManagerGtk :: ScreenForRect ( PRInt32 aX, PRInt32 aY,
                                      PRInt32 aWidth, PRInt32 aHeight,
                                      nsIScreen **aOutScreen )
{
  nsresult rv;
  rv = EnsureInit();
  if (NS_FAILED(rv)) {
    NS_ERROR("nsScreenManagerGtk::EnsureInit() failed from ScreenForRect");
    return rv;
  }
  // which screen ( index from zero ) should we return?
  PRUint32 which = 0;
  // Optimize for the common case.  If the number of screens is only
  // one then this will fall through with which == 0 and will get the
  // primary screen.
  if (mCachedScreenArray.Count() > 1) {
    // walk the list of screens and find the one that has the most
    // surface area.
    PRUint32 area = 0;
    nsIntRect windowRect(aX, aY, aWidth, aHeight);
    for (PRInt32 i = 0, i_end = mCachedScreenArray.Count(); i < i_end; ++i) {
      PRInt32  x, y, width, height;
      x = y = width = height = 0;
      mCachedScreenArray[i]->GetRect(&x, &y, &width, &height);
      // calculate the surface area
      nsIntRect screenRect(x, y, width, height);
      screenRect.IntersectRect(screenRect, windowRect);
      PRUint32 tempArea = screenRect.width * screenRect.height;
      if (tempArea >= area) {
        which = i;
        area = tempArea;
      }
    }
  }
  *aOutScreen = mCachedScreenArray.SafeObjectAt(which);
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
    NS_ERROR("nsScreenManagerGtk::EnsureInit() failed from GetPrimaryScreen");
    return rv;
  }
  *aPrimaryScreen = mCachedScreenArray.SafeObjectAt(0);
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
    NS_ERROR("nsScreenManagerGtk::EnsureInit() failed from GetNumberOfScreens");
    return rv;
  }
  *aNumberOfScreens = mCachedScreenArray.Count();
  return NS_OK;
  
} // GetNumberOfScreens

NS_IMETHODIMP
nsScreenManagerGtk :: ScreenForNativeWidget (void *aWidget, nsIScreen **outScreen)
{
  nsresult rv;
  rv = EnsureInit();
  if (NS_FAILED(rv)) {
    NS_ERROR("nsScreenManagerGtk::EnsureInit() failed from ScreenForNativeWidget");
    return rv;
  }

  if (mCachedScreenArray.Count() > 1) {
    // I don't know how to go from GtkWindow to nsIScreen, especially
    // given xinerama and stuff, so let's just do this
    gint x, y, width, height, depth;
    x = y = width = height = 0;

    gdk_window_get_geometry(GDK_WINDOW(aWidget), &x, &y, &width, &height,
                            &depth);
    gdk_window_get_origin(GDK_WINDOW(aWidget), &x, &y);
    rv = ScreenForRect(x, y, width, height, outScreen);
  } else {
    rv = GetPrimaryScreen(outScreen);
  }

  return rv;
}
