/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ClipboardReadRequestChild_h
#define mozilla_ClipboardReadRequestChild_h

#include "mozilla/PClipboardReadRequestChild.h"

class nsITransferable;

namespace mozilla {

class ClipboardReadRequestChild final : public PClipboardReadRequestChild {
 public:
  explicit ClipboardReadRequestChild(const nsTArray<nsCString>& aFlavorList) {
    mFlavorList.AppendElements(aFlavorList);
  }

  NS_INLINE_DECL_REFCOUNTING(ClipboardReadRequestChild)

  const nsTArray<nsCString>& FlavorList() const { return mFlavorList; }

 protected:
  virtual ~ClipboardReadRequestChild() = default;

 private:
  nsTArray<nsCString> mFlavorList;
};

}  // namespace mozilla

#endif  // mozilla_ClipboardReadRequestChild_h
