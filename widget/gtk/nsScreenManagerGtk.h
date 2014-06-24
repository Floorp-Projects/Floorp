/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenManagerGtk_h___
#define nsScreenManagerGtk_h___

#include "nsIScreenManager.h"
#include "nsIScreen.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "prlink.h"
#include "gdk/gdk.h"
#ifdef MOZ_X11
#include <X11/Xlib.h>
#endif

//------------------------------------------------------------------------

class nsScreenManagerGtk : public nsIScreenManager
{
public:
  nsScreenManagerGtk ( );

  NS_DECL_ISUPPORTS
  NS_DECL_NSISCREENMANAGER

#ifdef MOZ_X11
  Atom NetWorkareaAtom() { return mNetWorkareaAtom; }
#endif
  
  // For internal use, or reinitialization from change notification.
  nsresult Init();

private:
  virtual ~nsScreenManagerGtk();

  nsresult EnsureInit();

  // Cached screen array.  Its length is the number of screens we have.
  nsCOMArray<nsIScreen> mCachedScreenArray;

  PRLibrary *mXineramalib;

  GdkWindow *mRootWindow;
#ifdef MOZ_X11
  Atom mNetWorkareaAtom;
#endif
};

#endif  // nsScreenManagerGtk_h___ 
