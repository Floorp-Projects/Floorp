/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenGtk_h___
#define nsScreenGtk_h___

#include "nsBaseScreen.h"
#include "nsRect.h"
#include "gdk/gdk.h"
#ifdef MOZ_X11
#include <X11/Xlib.h>

// from Xinerama.h
typedef struct {
   int   screen_number;
   short x_org;
   short y_org;
   short width;
   short height;
} XineramaScreenInfo;
#endif /* MOZ_X11 */

//------------------------------------------------------------------------

class nsScreenGtk : public nsBaseScreen
{
public:
  nsScreenGtk();
  ~nsScreenGtk();

  NS_IMETHOD GetRect(PRInt32* aLeft, PRInt32* aTop, PRInt32* aWidth, PRInt32* aHeight);
  NS_IMETHOD GetAvailRect(PRInt32* aLeft, PRInt32* aTop, PRInt32* aWidth, PRInt32* aHeight);
  NS_IMETHOD GetPixelDepth(PRInt32* aPixelDepth);
  NS_IMETHOD GetColorDepth(PRInt32* aColorDepth);

  void Init(GdkWindow *aRootWindow);
#ifdef MOZ_X11
  void Init(XineramaScreenInfo *aScreenInfo);
#endif /* MOZ_X11 */

private:
  PRUint32 mScreenNum;
  nsIntRect mRect;
  nsIntRect mAvailRect;
};

#endif  // nsScreenGtk_h___ 
