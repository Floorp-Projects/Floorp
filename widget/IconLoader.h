/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_IconLoader_h_
#define mozilla_widget_IconLoader_h_

#include "imgINotificationObserver.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsIContentPolicy.h"

class nsIURI;
class nsINode;
class imgRequestProxy;
class imgIContainer;

namespace mozilla::widget {

/**
 * IconLoader is a utility for loading icons from either the disk or over
 * the network, and then using them in native OS widgetry like the macOS
 * global menu bar or the Windows notification area.
 */

class IconLoader : public imgINotificationObserver {
 public:
  /**
   * IconLoader itself is cross-platform, but each platform needs to supply
   * it with a Helper that does the platform-specific conversion work. The
   * Helper needs to implement the OnComplete method in order to handle the
   * imgIContainer of the loaded icon.
   */
  class Helper {
   public:
    NS_INLINE_DECL_REFCOUNTING(mozilla::widget::IconLoader::Helper)
    virtual nsresult OnComplete(imgIContainer* aContainer,
                                const nsIntRect& aRect) = 0;

   protected:
    virtual ~Helper() = default;
  };

  IconLoader(Helper* aHelper, nsINode* aContent,
             const nsIntRect& aImageRegionRect);

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER

  // LoadIcon will start a load request for the icon.
  // The request may not complete until after LoadIcon returns.
  // If aIsInternalIcon is true, the document and principal will not be
  // used when loading.
  nsresult LoadIcon(nsIURI* aIconURI, bool aIsInternalIcon = false);

  void ReleaseJSObjects() { mContent = nullptr; }

  void Destroy();

 protected:
  virtual ~IconLoader();

 private:
  nsresult OnFrameComplete(imgIRequest* aRequest);

  nsCOMPtr<nsINode> mContent;
  nsContentPolicyType mContentType;
  RefPtr<imgRequestProxy> mIconRequest;
  nsIntRect mImageRegionRect;
  bool mLoadedIcon;
  RefPtr<Helper> mHelper;
};

}  // namespace mozilla::widget
#endif  // mozilla_widget_IconLoader_h_
