/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_IconLoader_h_
#define mozilla_widget_IconLoader_h_

#include "imgINotificationObserver.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"

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
  // This is the interface that our listeners need to implement so that they can
  // be notified when the icon is loaded.
  class Listener {
   public:
    virtual nsresult OnComplete(imgIContainer* aContainer) = 0;
  };

  // Create the loader.
  // aListener will be notified when the load is complete.
  // The loader does not keep an owning reference to the listener. Call Destroy
  // before the listener goes away.
  explicit IconLoader(Listener* aListener);

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER

  // LoadIcon will start a load request for the icon.
  // The request may not complete until after LoadIcon returns.
  // If aIsInternalIcon is true, the document and principal will not be
  // used when loading.
  nsresult LoadIcon(nsIURI* aIconURI, nsINode* aNode,
                    bool aIsInternalIcon = false);

  void Destroy();

 protected:
  virtual ~IconLoader();

 private:
  RefPtr<imgRequestProxy> mIconRequest;

  // The listener, which is notified when loading completes.
  // Can be null, after a call to Destroy.
  // This is a non-owning reference and needs to be cleared with a call to
  // Destroy before the listener goes away.
  Listener* mListener;
};

}  // namespace mozilla::widget
#endif  // mozilla_widget_IconLoader_h_
