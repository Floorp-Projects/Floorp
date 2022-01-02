/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseClipboard_h__
#define nsBaseClipboard_h__

#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsCOMPtr.h"

class nsITransferable;
class nsIClipboardOwner;
class nsIWidget;

/**
 * Native Win32 BaseClipboard wrapper
 */

class nsBaseClipboard : public nsIClipboard {
 public:
  nsBaseClipboard();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIClipboard
  NS_DECL_NSICLIPBOARD

 protected:
  virtual ~nsBaseClipboard();

  NS_IMETHOD SetNativeClipboardData(int32_t aWhichClipboard) = 0;
  NS_IMETHOD GetNativeClipboardData(nsITransferable* aTransferable,
                                    int32_t aWhichClipboard) = 0;

  bool mEmptyingForSetData;
  bool mIgnoreEmptyNotification;
  nsCOMPtr<nsIClipboardOwner> mClipboardOwner;
  nsCOMPtr<nsITransferable> mTransferable;
};

#endif  // nsBaseClipboard_h__
