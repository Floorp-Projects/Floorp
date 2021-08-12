/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsClipboard_h_
#define __nsClipboard_h_

#include "mozilla/UniquePtr.h"
#include "nsIClipboard.h"
#include "nsIObserver.h"
#include <gtk/gtk.h>

#ifdef MOZ_LOGGING
#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"
extern mozilla::LazyLogModule gClipboardLog;
#  define LOGCLIP(args) MOZ_LOG(gClipboardLog, mozilla::LogLevel::Debug, args)
#else
#  define LOGCLIP(args)
#endif /* MOZ_LOGGING */

enum ClipboardDataType { CLIPBOARD_DATA, CLIPBOARD_TEXT, CLIPBOARD_TARGETS };

class nsRetrievalContext {
 public:
  // We intentionally use unsafe thread refcount as clipboard is used in
  // main thread only.
  NS_INLINE_DECL_REFCOUNTING(nsRetrievalContext)

  // Get actual clipboard content (GetClipboardData/GetClipboardText)
  // which has to be released by ReleaseClipboardData().
  virtual const char* GetClipboardData(const char* aMimeType,
                                       int32_t aWhichClipboard,
                                       uint32_t* aContentLength) = 0;
  virtual const char* GetClipboardText(int32_t aWhichClipboard) = 0;
  virtual void ReleaseClipboardData(const char* aClipboardData) = 0;

  // Get data mime types which can be obtained from clipboard.
  // The returned array has to be released by g_free().
  virtual GdkAtom* GetTargets(int32_t aWhichClipboard, int* aTargetNum) = 0;

  virtual bool HasSelectionSupport(void) = 0;

 protected:
  virtual ~nsRetrievalContext() = default;
};

class nsClipboard : public nsIClipboard, public nsIObserver {
 public:
  nsClipboard();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSICLIPBOARD

  // Make sure we are initialized, called from the factory
  // constructor
  nsresult Init(void);

  // Someone requested the selection
  void SelectionGetEvent(GtkClipboard* aGtkClipboard,
                         GtkSelectionData* aSelectionData);
  void SelectionClearEvent(GtkClipboard* aGtkClipboard);

 private:
  virtual ~nsClipboard();

  // Get our hands on the correct transferable, given a specific
  // clipboard
  nsITransferable* GetTransferable(int32_t aWhichClipboard);

  // Send clipboard data by nsITransferable
  void SetTransferableData(nsITransferable* aTransferable, nsCString& aFlavor,
                           const char* aClipboardData,
                           uint32_t aClipboardDataLength);

  void ClearTransferable(int32_t aWhichClipboard);

  // Hang on to our owners and transferables so we can transfer data
  // when asked.
  nsCOMPtr<nsIClipboardOwner> mSelectionOwner;
  nsCOMPtr<nsIClipboardOwner> mGlobalOwner;
  nsCOMPtr<nsITransferable> mSelectionTransferable;
  nsCOMPtr<nsITransferable> mGlobalTransferable;
  RefPtr<nsRetrievalContext> mContext;
};

extern const int kClipboardTimeout;

GdkAtom GetSelectionAtom(int32_t aWhichClipboard);
int GetGeckoClipboardType(GtkClipboard* aGtkClipboard);

#endif /* __nsClipboard_h_ */
