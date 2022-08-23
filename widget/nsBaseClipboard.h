/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseClipboard_h__
#define nsBaseClipboard_h__

#include "mozilla/Logging.h"
#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsCOMPtr.h"

static mozilla::LazyLogModule sWidgetClipboardLog("WidgetClipboard");
#define CLIPBOARD_LOG(...) \
  MOZ_LOG(sWidgetClipboardLog, LogLevel::Debug, (__VA_ARGS__))
#define CLIPBOARD_LOG_ENABLED() \
  MOZ_LOG_TEST(sWidgetClipboardLog, LogLevel::Debug)

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
  nsCOMPtr<nsIClipboardOwner> mClipboardOwner;
  nsCOMPtr<nsITransferable> mTransferable;

 private:
  bool mIgnoreEmptyNotification = false;
};

#endif  // nsBaseClipboard_h__
