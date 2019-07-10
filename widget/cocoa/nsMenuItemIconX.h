/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Retrieves and displays icons in native menu items on Mac OS X.
 */

#ifndef nsMenuItemIconX_h_
#define nsMenuItemIconX_h_

#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "imgINotificationObserver.h"
#include "nsIContentPolicy.h"
#include "nsIconLoaderService.h"

class nsIconLoaderService;
class nsIURI;
class nsIContent;
class nsIPrincipal;
class imgRequestProxy;
class nsMenuObjectX;

#import <Cocoa/Cocoa.h>

class nsMenuItemIconX : public nsIconLoaderObserver {
 public:
  nsMenuItemIconX(nsMenuObjectX* aMenuItem, nsIContent* aContent,
                  NSMenuItem* aNativeMenuItem);

 private:
  virtual ~nsMenuItemIconX();

 public:
  // SetupIcon succeeds if it was able to set up the icon, or if there should
  // be no icon, in which case it clears any existing icon but still succeeds.
  nsresult SetupIcon();

  // GetIconURI fails if the item should not have any icon.
  nsresult GetIconURI(nsIURI** aIconURI);

  // Overrides nsIconLoaderObserver::OnComplete. Handles the NSImage* created
  // by nsIconLoaderService.
  nsresult OnComplete(NSImage* aImage) override;

  // Unless we take precautions, we may outlive the object that created us
  // (mMenuObject, which owns our native menu item (mNativeMenuItem)).
  // Destroy() should be called from mMenuObject's destructor to prevent
  // this from happening.  See bug 499600.
  void Destroy();

 protected:
  nsCOMPtr<nsIContent> mContent;
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  nsContentPolicyType mContentType;
  nsMenuObjectX* mMenuObject;  // [weak]
  nsIntRect mImageRegionRect;
  bool mSetIcon;
  NSMenuItem* mNativeMenuItem;  // [weak]
  // The icon loader object should never outlive its creating nsMenuItemIconX
  // object.
  RefPtr<nsIconLoaderService> mIconLoader;
};

#endif  // nsMenuItemIconX_h_
