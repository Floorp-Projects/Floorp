/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsClipboardX11_h_
#define __nsClipboardX11_h_

#include <gtk/gtk.h>

class nsRetrievalContextX11 : public nsRetrievalContext {
 public:
  enum State { INITIAL, COMPLETED, TIMED_OUT };

  virtual const char* GetClipboardData(const char* aMimeType,
                                       int32_t aWhichClipboard,
                                       uint32_t* aContentLength) override;
  virtual const char* GetClipboardText(int32_t aWhichClipboard) override;
  virtual void ReleaseClipboardData(const char* aClipboardData) override;

  virtual GdkAtom* GetTargets(int32_t aWhichClipboard,
                              int* aTargetNums) override;

  virtual bool HasSelectionSupport(void) override;

  // Call this when data or text has been retrieved.
  void Complete(ClipboardDataType aDataType, const void* aData,
                int aDataRequestNumber);

  nsRetrievalContextX11();

 private:
  bool WaitForClipboardData(ClipboardDataType aDataType,
                            GtkClipboard* clipboard,
                            const char* aMimeType = nullptr);

  /**
   * Spins X event loop until timing out or being completed. Returns
   * null if we time out, otherwise returns the completed data (passing
   * ownership to caller).
   */
  bool WaitForX11Content();

  State mState;
  int mClipboardRequestNumber;
  void* mClipboardData;
  uint32_t mClipboardDataLength;
  GdkAtom mTargetMIMEType;
};

class ClipboardRequestHandler {
 public:
  ClipboardRequestHandler(nsRetrievalContextX11* aContext,
                          ClipboardDataType aDataType, int aDataRequestNumber)
      : mContext(aContext),
        mDataRequestNumber(aDataRequestNumber),
        mDataType(aDataType) {}

  void Complete(const void* aData) {
    mContext->Complete(mDataType, aData, mDataRequestNumber);
  }

 private:
  nsRetrievalContextX11* mContext;
  int mDataRequestNumber;
  ClipboardDataType mDataType;
};

#endif /* __nsClipboardX11_h_ */
