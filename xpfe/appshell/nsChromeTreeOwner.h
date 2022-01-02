/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChromeTreeOwner_h__
#define nsChromeTreeOwner_h__

// Helper Classes
#include "nsCOMPtr.h"

// Interfaces Needed
#include "nsIBaseWindow.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"

namespace mozilla {
class AppWindow;
}

class nsChromeTreeOwner : public nsIDocShellTreeOwner,
                          public nsIBaseWindow,
                          public nsIInterfaceRequestor,
                          public nsIWebProgressListener,
                          public nsSupportsWeakReference {
  friend class mozilla::AppWindow;

 public:
  NS_DECL_ISUPPORTS

  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIBASEWINDOW
  NS_DECL_NSIDOCSHELLTREEOWNER
  NS_DECL_NSIWEBPROGRESSLISTENER

 protected:
  nsChromeTreeOwner();
  virtual ~nsChromeTreeOwner();

  void AppWindow(mozilla::AppWindow* aAppWindow);
  mozilla::AppWindow* AppWindow();

 protected:
  mozilla::AppWindow* mAppWindow;
};

#endif /* nsChromeTreeOwner_h__ */
