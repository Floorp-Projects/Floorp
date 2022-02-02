/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsClipboardWayland_h_
#define __nsClipboardWayland_h_

#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <nsTArray.h>

#include "mozilla/Mutex.h"
#include "mozilla/Maybe.h"
#include "nsIThread.h"
#include "nsClipboard.h"
#include "nsWaylandDisplay.h"

class nsRetrievalContextWayland final : public nsRetrievalContext {
 public:
  nsRetrievalContextWayland();

  // Successful call of GetClipboardData()/GetClipboardText() needs to be paired
  // with ReleaseClipboardData().
  ClipboardData GetClipboardData(const char* aMimeType,
                                 int32_t aWhichClipboard) override;
  mozilla::GUniquePtr<char> GetClipboardText(int32_t aWhichClipboard) override;

  // GetTargets() uses clipboard data internally so it can't be used between
  // GetClipboardData()/GetClipboardText() and ReleaseClipboardData() calls.
  ClipboardTargets GetTargets(int32_t aWhichClipboard) override;

  void TransferClipboardData(ClipboardDataType aDataType,
                             int aClipboardRequestNumber, const void* aData);

  bool HasSelectionSupport(void) override { return true; }

 private:
  ClipboardData WaitForClipboardData(ClipboardDataType, int32_t aWhichClipboard,
                                     const char* aMimeType = nullptr);
  ClipboardData WaitForClipboardContent();

  int mClipboardRequestNumber = 0;
  mozilla::Maybe<ClipboardData> mClipboardData;
  mozilla::Mutex mMutex;
};

#endif /* __nsClipboardWayland_h_ */
