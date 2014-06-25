/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_CLIPBOARD_PROXY_H
#define NS_CLIPBOARD_PROXY_H

#include "nsIClipboard.h"

class nsClipboardProxy MOZ_FINAL : public nsIClipboard
{
  ~nsClipboardProxy() {}

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLIPBOARD

  nsClipboardProxy();
};

#endif
