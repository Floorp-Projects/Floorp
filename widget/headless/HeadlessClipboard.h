/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_HeadlessClipboard_h
#define mozilla_widget_HeadlessClipboard_h

#include "nsBaseClipboard.h"
#include "nsIClipboard.h"
#include "mozilla/UniquePtr.h"
#include "HeadlessClipboardData.h"

namespace mozilla {
namespace widget {

class HeadlessClipboard final : public nsBaseClipboard {
 public:
  HeadlessClipboard();

  NS_DECL_ISUPPORTS_INHERITED

 protected:
  ~HeadlessClipboard() = default;

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

 private:
  UniquePtr<HeadlessClipboardData>
      mClipboards[nsIClipboard::kClipboardTypeCount];
};

}  // namespace widget
}  // namespace mozilla

#endif
