/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Retrieves and displays icons in native menu items on Mac OS X.
 */

#ifndef nsMenuItemIconX_h_
#define nsMenuItemIconX_h_

#include "mozilla/widget/IconLoader.h"

class nsIconLoaderService;
class nsIURI;
class nsIContent;
class nsIPrincipal;
class imgRequestProxy;
class nsMenuObjectX;

#import <Cocoa/Cocoa.h>

class nsMenuItemIconX final : public mozilla::widget::IconLoader::Listener {
 public:
  nsMenuItemIconX(nsMenuObjectX* aMenuItem, nsIContent* aContent,
                  NSMenuItem* aNativeMenuItem);
  ~nsMenuItemIconX();

 public:
  // SetupIcon succeeds if it was able to set up the icon, or if there should
  // be no icon, in which case it clears any existing icon but still succeeds.
  nsresult SetupIcon();

  // GetIconURI fails if the item should not have any icon.
  nsresult GetIconURI(nsIURI** aIconURI);

  // Implements this method for mozilla::widget::IconLoader::Listener.
  // Called once the icon load is complete.
  nsresult OnComplete(imgIContainer* aImage) override;

 protected:
  nsCOMPtr<nsIContent> mContent;
  nsMenuObjectX* mMenuObject;  // [weak]
  nsIntRect mImageRegionRect;
  bool mSetIcon;
  NSMenuItem* mNativeMenuItem;  // [weak]
  // The icon loader object should never outlive its creating nsMenuItemIconX
  // object.
  RefPtr<mozilla::widget::IconLoader> mIconLoader;
};

#endif  // nsMenuItemIconX_h_
