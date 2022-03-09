/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsClipboard_h_
#define __nsClipboard_h_

#include "mozilla/UniquePtr.h"
#include "mozilla/Span.h"
#include "nsIClipboard.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "GUniquePtr.h"
#include <gtk/gtk.h>

#ifdef MOZ_LOGGING
#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"
extern mozilla::LazyLogModule gClipboardLog;
#  define LOGCLIP(...) \
    MOZ_LOG(gClipboardLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#  define LOGCLIP_ENABLED() \
    MOZ_LOG_TEST(gClipboardLog, mozilla::LogLevel::Debug)
#else
#  define LOGCLIP(...)
#  define LOGCLIP_ENABLED() false
#endif /* MOZ_LOGGING */

class ClipboardTargets {
  friend class ClipboardData;

  mozilla::GUniquePtr<GdkAtom> mTargets;
  uint32_t mCount = 0;

 public:
  ClipboardTargets() = default;
  ClipboardTargets(mozilla::GUniquePtr<GdkAtom> aTargets, uint32_t aCount)
      : mTargets(std::move(aTargets)), mCount(aCount) {}

  void Set(ClipboardTargets);
  ClipboardTargets Clone();
  void Clear() {
    mTargets = nullptr;
    mCount = 0;
  };

  mozilla::Span<GdkAtom> AsSpan() const { return {mTargets.get(), mCount}; }
  explicit operator bool() const { return bool(mTargets); }
};

class ClipboardData {
  mozilla::GUniquePtr<char> mData;
  uint32_t mLength = 0;

 public:
  ClipboardData() = default;

  void SetData(mozilla::Span<const uint8_t>);
  void SetText(mozilla::Span<const char>);
  void SetTargets(ClipboardTargets);

  ClipboardTargets ExtractTargets();
  mozilla::GUniquePtr<char> ExtractText() {
    mLength = 0;
    return std::move(mData);
  }

  mozilla::Span<char> AsSpan() const { return {mData.get(), mLength}; }
  explicit operator bool() const { return bool(mData); }
};

enum class ClipboardDataType { Data, Text, Targets };

class nsRetrievalContext {
 public:
  // We intentionally use unsafe thread refcount as clipboard is used in
  // main thread only.
  NS_INLINE_DECL_REFCOUNTING(nsRetrievalContext)

  // Get actual clipboard content (GetClipboardData/GetClipboardText).
  virtual ClipboardData GetClipboardData(const char* aMimeType,
                                         int32_t aWhichClipboard) = 0;
  virtual mozilla::GUniquePtr<char> GetClipboardText(
      int32_t aWhichClipboard) = 0;

  // Get data mime types which can be obtained from clipboard.
  ClipboardTargets GetTargets(int32_t aWhichClipboard);

  // Clipboard/Primary selection owner changed. Clear internal cached data.
  static void ClearCachedTargetsClipboard(GtkClipboard* aClipboard,
                                          GdkEvent* aEvent, gpointer data);
  static void ClearCachedTargetsPrimary(GtkClipboard* aClipboard,
                                        GdkEvent* aEvent, gpointer data);

  nsRetrievalContext();

 protected:
  virtual ClipboardTargets GetTargetsImpl(int32_t aWhichClipboard) = 0;
  virtual ~nsRetrievalContext();

  static ClipboardTargets sClipboardTargets;
  static ClipboardTargets sPrimaryTargets;
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
  void ClearCachedTargets(int32_t aWhichClipboard);

  bool FilterImportedFlavors(int32_t aWhichClipboard,
                             nsTArray<nsCString>& aFlavors);

  // Hang on to our owners and transferables so we can transfer data
  // when asked.
  nsCOMPtr<nsIClipboardOwner> mSelectionOwner;
  nsCOMPtr<nsIClipboardOwner> mGlobalOwner;
  nsCOMPtr<nsITransferable> mSelectionTransferable;
  nsCOMPtr<nsITransferable> mGlobalTransferable;
  RefPtr<nsRetrievalContext> mContext;
};

extern const int kClipboardTimeout;
extern const int kClipboardFastIterationNum;

GdkAtom GetSelectionAtom(int32_t aWhichClipboard);
int GetGeckoClipboardType(GtkClipboard* aGtkClipboard);

#endif /* __nsClipboard_h_ */
