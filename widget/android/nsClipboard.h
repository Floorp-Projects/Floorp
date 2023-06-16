/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_CLIPBOARD_H
#define NS_CLIPBOARD_H

#include "nsBaseClipboard.h"

class nsClipboard final : public ClipboardSetDataHelper {
 private:
  ~nsClipboard() = default;

 public:
  nsClipboard() = default;

  NS_DECL_ISUPPORTS_INHERITED

  // nsIClipboard
  NS_IMETHOD GetData(nsITransferable* aTransferable,
                     int32_t aWhichClipboard) override;
  NS_IMETHOD EmptyClipboard(int32_t aWhichClipboard) override;
  NS_IMETHOD HasDataMatchingFlavors(const nsTArray<nsCString>& aFlavorList,
                                    int32_t aWhichClipboard,
                                    bool* _retval) override;
  NS_IMETHOD IsClipboardTypeSupported(int32_t aWhichClipboard,
                                      bool* _retval) override;
  RefPtr<mozilla::GenericPromise> AsyncGetData(
      nsITransferable* aTransferable, int32_t aWhichClipboard) override;
  RefPtr<DataFlavorsPromise> AsyncHasDataMatchingFlavors(
      const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard) override;

 protected:
  // Implement the native clipboard behavior.
  NS_IMETHOD SetNativeClipboardData(nsITransferable* aTransferable,
                                    nsIClipboardOwner* aOwner,
                                    int32_t aWhichClipboard) override;
};

#endif
