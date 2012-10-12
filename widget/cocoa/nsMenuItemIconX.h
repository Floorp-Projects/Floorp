/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Retrieves and displays icons in native menu items on Mac OS X.
 */

#ifndef nsMenuItemIconX_h_
#define nsMenuItemIconX_h_

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "imgINotificationObserver.h"

class nsIURI;
class nsIContent;
class imgRequestProxy;
class nsMenuObjectX;

#import <Cocoa/Cocoa.h>

class nsMenuItemIconX : public imgINotificationObserver
{
public:
  nsMenuItemIconX(nsMenuObjectX* aMenuItem,
                  nsIContent*    aContent,
                  NSMenuItem*    aNativeMenuItem);
private:
  virtual ~nsMenuItemIconX();

public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER

  // SetupIcon succeeds if it was able to set up the icon, or if there should
  // be no icon, in which case it clears any existing icon but still succeeds.
  nsresult SetupIcon();

  // GetIconURI fails if the item should not have any icon.
  nsresult GetIconURI(nsIURI** aIconURI);

  // LoadIcon will set a placeholder image and start a load request for the
  // icon.  The request may not complete until after LoadIcon returns.
  nsresult LoadIcon(nsIURI* aIconURI);

  // Unless we take precautions, we may outlive the object that created us
  // (mMenuObject, which owns our native menu item (mNativeMenuItem)).
  // Destroy() should be called from mMenuObject's destructor to prevent
  // this from happening.  See bug 499600.
  void Destroy();

protected:
  nsresult OnStopFrame(imgIRequest* aRequest);

  nsCOMPtr<nsIContent>      mContent;
  nsRefPtr<imgRequestProxy> mIconRequest;
  nsMenuObjectX*            mMenuObject; // [weak]
  nsIntRect                 mImageRegionRect;
  bool                      mLoadedIcon;
  bool                      mSetIcon;
  NSMenuItem*               mNativeMenuItem; // [weak]
};

#endif // nsMenuItemIconX_h_
