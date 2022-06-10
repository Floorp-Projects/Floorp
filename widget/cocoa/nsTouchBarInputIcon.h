/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Retrieves and displays icons on the macOS Touch Bar.
 */

#ifndef nsTouchBarInputIcon_h_
#define nsTouchBarInputIcon_h_

#import <Cocoa/Cocoa.h>

#include "mozilla/widget/IconLoader.h"
#include "nsTouchBarInput.h"

using namespace mozilla::dom;

class nsIURI;
class nsIPrincipal;
class imgRequestProxy;

namespace mozilla::dom {
class Document;
}

class nsTouchBarInputIcon : public mozilla::widget::IconLoader::Listener {
 public:
  explicit nsTouchBarInputIcon(RefPtr<Document> aDocument,
                               TouchBarInput* aInput, NSTouchBarItem* aItem);

  NS_INLINE_DECL_REFCOUNTING(nsTouchBarInputIcon)

 private:
  virtual ~nsTouchBarInputIcon();

 public:
  // SetupIcon succeeds if it was able to set up the icon, or if there should
  // be no icon, in which case it clears any existing icon but still succeeds.
  nsresult SetupIcon(nsCOMPtr<nsIURI> aIconURI);

  // Implements this method for mozilla::widget::IconLoader::Listener.
  // Called once the icon load is complete.
  nsresult OnComplete(imgIContainer* aImage) override;

  // Unless we take precautions, we may outlive the object that created us
  // (mTouchBar, which owns our native menu item (mTouchBarInput)).
  // Destroy() should be called from mTouchBar's destructor to prevent
  // this from happening.
  void Destroy();

  void ReleaseJSObjects();

 protected:
  RefPtr<Document> mDocument;
  nsIntRect mImageRegionRect;
  bool mSetIcon;
  NSButton* mButton;
  // We accept a mShareScrubber only as a special case since
  // NSSharingServicePickerTouchBarItem does not expose an NSButton* on which we
  // can set the `image` property.
  NSSharingServicePickerTouchBarItem* mShareScrubber;
  // We accept a popover only as a special case.
  NSPopoverTouchBarItem* mPopoverItem;
  // The icon loader object should never outlive its creating
  // nsTouchBarInputIcon object.
  RefPtr<mozilla::widget::IconLoader> mIconLoader;
};

#endif  // nsTouchBarInputIcon_h_
