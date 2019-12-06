/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIMacDockSupport.h"
#include "nsIStandaloneNativeMenu.h"
#include "nsITaskbarProgress.h"
#include "nsCOMPtr.h"
#include "nsString.h"

@class MOZProgressDockOverlayView;

class nsMacDockSupport : public nsIMacDockSupport, public nsITaskbarProgress {
 public:
  nsMacDockSupport();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMACDOCKSUPPORT
  NS_DECL_NSITASKBARPROGRESS

 protected:
  virtual ~nsMacDockSupport();

  nsCOMPtr<nsIStandaloneNativeMenu> mDockMenu;
  nsString mBadgeText;

  NSView* mDockTileWrapperView;
  MOZProgressDockOverlayView* mProgressDockOverlayView;

  nsTaskbarProgressState mProgressState;
  double mProgressFraction;

  nsresult UpdateDockTile();
};
