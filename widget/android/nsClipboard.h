/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_CLIPBOARD_H
#define NS_CLIPBOARD_H

#include "nsBaseClipboard.h"

class nsClipboard final : public nsBaseClipboard {
 private:
  ~nsClipboard();

 public:
  nsClipboard();

  NS_DECL_ISUPPORTS_INHERITED

 protected:
  // Implement the native clipboard behavior.
  NS_IMETHOD SetNativeClipboardData(nsITransferable* aTransferable,
                                    int32_t aWhichClipboard) override;
  NS_IMETHOD GetNativeClipboardData(nsITransferable* aTransferable,
                                    int32_t aWhichClipboard) override;
  nsresult EmptyNativeClipboardData(int32_t aWhichClipboard) override;
  mozilla::Result<int32_t, nsresult> GetNativeClipboardSequenceNumber(
      int32_t aWhichClipboard) override;
  mozilla::Result<bool, nsresult> HasNativeClipboardDataMatchingFlavors(
      const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard) override;
};

#endif
