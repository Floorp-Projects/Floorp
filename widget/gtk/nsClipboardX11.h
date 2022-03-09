/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsClipboardX11_h_
#define __nsClipboardX11_h_

#include <gtk/gtk.h>

#include "mozilla/Maybe.h"
#include "nsClipboard.h"

class nsRetrievalContextX11 : public nsRetrievalContext {
 public:
  ClipboardData GetClipboardData(const char* aMimeType,
                                 int32_t aWhichClipboard) override;
  mozilla::GUniquePtr<char> GetClipboardText(int32_t aWhichClipboard) override;
  ClipboardTargets GetTargetsImpl(int32_t aWhichClipboard) override;

  nsRetrievalContextX11();

 private:
  // Spins X event loop until timing out or being completed.
  ClipboardData WaitForClipboardData(ClipboardDataType aDataType,
                                     int32_t aWhichClipboard,
                                     const char* aMimeType = nullptr);
};

#endif /* __nsClipboardX11_h_ */
