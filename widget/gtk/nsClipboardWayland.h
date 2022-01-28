/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsClipboardWaylandAsync_h_
#define __nsClipboardWaylandAsync_h_

#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <nsTArray.h>

#include "mozilla/Mutex.h"
#include "nsIThread.h"
#include "mozilla/UniquePtr.h"
#include "nsClipboard.h"
#include "nsWaylandDisplay.h"

class nsRetrievalContextWaylandAsync : public nsRetrievalContext {
 public:
  nsRetrievalContextWaylandAsync();

  // Successful call of GetClipboardData()/GetClipboardText() needs to be paired
  // with ReleaseClipboardData().
  virtual const char* GetClipboardData(const char* aMimeType,
                                       int32_t aWhichClipboard,
                                       uint32_t* aContentLength) override;
  virtual const char* GetClipboardText(int32_t aWhichClipboard) override;
  virtual void ReleaseClipboardData(const char* aClipboardData) override;

  // GetTargets() uses clipboard data internally so it can't be used between
  // GetClipboardData()/GetClipboardText() and ReleaseClipboardData() calls.
  virtual GdkAtom* GetTargets(int32_t aWhichClipboard,
                              int* aTargetNum) override;

  void TransferAsyncClipboardData(ClipboardDataType aDataType,
                                  int aClipboardRequestNumber,
                                  const void* aData);

  bool HasSelectionSupport(void) override { return true; }

 private:
  bool WaitForClipboardContent();

 private:
  int mClipboardRequestNumber;
  bool mClipboardDataReceived;
  char* mClipboardData;
  uint32_t mClipboardDataLength;
  mozilla::Mutex mMutex;
};

#endif /* __nsClipboardWayland_h_ */
