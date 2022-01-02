/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentTreeOwner_h__
#define nsContentTreeOwner_h__

// Helper Classes
#include "nsCOMPtr.h"
#include "nsString.h"

// Interfaces Needed
#include "nsIBaseWindow.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWindowProvider.h"

namespace mozilla {
class AppWindow;
}
class nsSiteWindow;

class nsContentTreeOwner final : public nsIDocShellTreeOwner,
                                 public nsIBaseWindow,
                                 public nsIInterfaceRequestor,
                                 public nsIWebBrowserChrome,
                                 public nsIWindowProvider {
  friend class mozilla::AppWindow;
  friend class nsSiteWindow;

 public:
  NS_DECL_ISUPPORTS

  NS_DECL_NSIBASEWINDOW
  NS_DECL_NSIDOCSHELLTREEOWNER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIWEBBROWSERCHROME
  NS_DECL_NSIWINDOWPROVIDER

 protected:
  explicit nsContentTreeOwner(bool fPrimary);
  virtual ~nsContentTreeOwner();

  void AppWindow(mozilla::AppWindow* aAppWindow);
  mozilla::AppWindow* AppWindow();

 protected:
  mozilla::AppWindow* mAppWindow;
  nsSiteWindow* mSiteWindow;
  bool mPrimary;
};

#endif /* nsContentTreeOwner_h__ */
