/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 * syd@netscape.com 2/12/00. 
 * pavlov@netscape.com 2/13/00.
 * bryner@brianryner.com 11/20/01.
 */

#include "nsNativeAppSupportBase.h"
#include "gdk/gdk.h"
#include "prenv.h"
#include "nsString.h"
#ifdef MOZ_XUL_APP
extern char* splash_xpm[];
#else
#include SPLASH_XPM
#endif

class nsSplashScreenGtk : public nsISplashScreen {
public:
  nsSplashScreenGtk();
  virtual ~nsSplashScreenGtk();

  NS_IMETHOD Show();
  NS_IMETHOD Hide();

  NS_DECL_ISUPPORTS

private:
  GdkWindow *mDialog;
}; // class nsSplashScreenGtk

class nsNativeAppSupportGtk : public nsNativeAppSupportBase {
  // We don't have any methods to override.
};

NS_IMPL_ISUPPORTS1(nsSplashScreenGtk, nsISplashScreen)

nsSplashScreenGtk::nsSplashScreenGtk()
{
}

nsSplashScreenGtk::~nsSplashScreenGtk()
{
  Hide();
}

NS_IMETHODIMP nsSplashScreenGtk::Show()
{
#ifdef MOZ_XUL_APP
  if (!splash_xpm[0])
    return NS_OK;
#endif

  nsCAutoString path(PR_GetEnv("MOZILLA_FIVE_HOME"));

  if (path.IsEmpty()) {
    path.Assign("splash.xpm");
  } else {
    path.Append("/splash.xpm");
  }

  /* See if the user has a custom splash screen */
  GdkPixmap* pmap = gdk_pixmap_colormap_create_from_xpm(NULL,
                                                    gdk_colormap_get_system(),
                                                    NULL, NULL, path.get());

  if (!pmap) {
    /* create a pixmap based on xpm data */
    pmap = gdk_pixmap_colormap_create_from_xpm_d(NULL,
                                                    gdk_colormap_get_system(),
                                                    NULL, NULL, splash_xpm);
  }

  if (!pmap) {
    gdk_window_destroy(mDialog);
    mDialog = nsnull;
    return NS_ERROR_FAILURE;
  }

  gint width, height;
  gdk_window_get_size(pmap, &width, &height);

  GdkWindowAttr attr;
  attr.window_type = GDK_WINDOW_TEMP;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.x = (gdk_screen_width() >> 1) - (width >> 1);
  attr.y = (gdk_screen_height() >> 1) - (height >> 1);
  attr.width = width;
  attr.height = height;
  attr.event_mask = GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK;
  mDialog = gdk_window_new(NULL, &attr, GDK_WA_X | GDK_WA_Y);

  gdk_window_set_back_pixmap(mDialog, pmap, FALSE);
  gdk_pixmap_unref(pmap);

  gdk_window_show(mDialog);

  return NS_OK;
}

NS_IMETHODIMP nsSplashScreenGtk::Hide()
{
  if (mDialog) {
    gdk_window_destroy(mDialog);
    mDialog = nsnull;
  }
  return NS_OK;
}

nsresult NS_CreateNativeAppSupport(nsINativeAppSupport** aNativeApp) {
  *aNativeApp = new nsNativeAppSupportGtk;
  NS_ADDREF(*aNativeApp);
  return NS_OK;
}

nsresult NS_CreateSplashScreen(nsISplashScreen** aSplash) {
  *aSplash = new nsSplashScreenGtk;
  NS_ADDREF(*aSplash);
  return NS_OK;
}

PRBool NS_CanRun()
{
  return PR_TRUE;
}
