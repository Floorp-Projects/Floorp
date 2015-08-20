/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsClipbard_h__
#define nsClipbard_h__

#include "GonkClipboardData.h"
#include "mozilla/UniquePtr.h"
#include "nsIClipboard.h"

class nsClipboard final : public nsIClipboard
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLIPBOARD

  nsClipboard();

protected:
  ~nsClipboard() {}

private:
  mozilla::UniquePtr<mozilla::GonkClipboardData> mClipboard;
};

#endif
