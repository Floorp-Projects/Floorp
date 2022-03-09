/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsClipboardWayland_h_
#define __nsClipboardWayland_h_

#include "mozilla/Mutex.h"
#include "nsClipboard.h"

class nsRetrievalContextWayland final : public nsRetrievalContext {
 public:
  nsRetrievalContextWayland();

  ClipboardData GetClipboardData(const char* aMimeType,
                                 int32_t aWhichClipboard) override;
  mozilla::GUniquePtr<char> GetClipboardText(int32_t aWhichClipboard) override;
  ClipboardTargets GetTargetsImpl(int32_t aWhichClipboard) override;

 private:
  ClipboardData WaitForClipboardData(ClipboardDataType, int32_t aWhichClipboard,
                                     const char* aMimeType = nullptr);
};

#endif /* __nsClipboardWayland_h_ */
