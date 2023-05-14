/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseClipboard_h__
#define nsBaseClipboard_h__

#include "mozilla/Logging.h"
#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsCOMPtr.h"

static mozilla::LazyLogModule sWidgetClipboardLog("WidgetClipboard");
#define CLIPBOARD_LOG(...) \
  MOZ_LOG(sWidgetClipboardLog, LogLevel::Debug, (__VA_ARGS__))
#define CLIPBOARD_LOG_ENABLED() \
  MOZ_LOG_TEST(sWidgetClipboardLog, LogLevel::Debug)

class nsITransferable;
class nsIClipboardOwner;
class nsIWidget;

/**
 * A helper base class to implement nsIClipboard::SetData/AsyncSetData, so that
 * all platform can share the same implementation.
 *
 * XXX this could be merged into nsBaseClipboard once all platform use
 * nsBaseClipboard as base clipboard class to share the common code, see bug
 * 1773707.
 */
class ClipboardSetDataHelper : public nsIClipboard {
 public:
  NS_DECL_ISUPPORTS

  ClipboardSetDataHelper() = default;

  // nsIClipboard
  // XXX the Cocoa widget currently overrides `SetData` for `kSelectionCache`
  // type, so it cannot be marked as final. Once the Cocoa widget handles
  // `kSelectionCache` type more generic after bug 1812078, it can be marked
  // as final, too.
  NS_IMETHOD SetData(nsITransferable* aTransferable, nsIClipboardOwner* aOwner,
                     int32_t aWhichClipboard) override;
  NS_IMETHOD AsyncSetData(int32_t aWhichClipboard,
                          nsIAsyncSetClipboardDataCallback* aCallback,
                          nsIAsyncSetClipboardData** _retval) override final;

 protected:
  virtual ~ClipboardSetDataHelper();

  // Implement the native clipboard behavior.
  NS_IMETHOD SetNativeClipboardData(nsITransferable* aTransferable,
                                    nsIClipboardOwner* aOwner,
                                    int32_t aWhichClipboard) = 0;

  class AsyncSetClipboardData final : public nsIAsyncSetClipboardData {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIASYNCSETCLIPBOARDDATA

    AsyncSetClipboardData(int32_t aClipboardType,
                          ClipboardSetDataHelper* aClipboard,
                          nsIAsyncSetClipboardDataCallback* aCallback);

   private:
    virtual ~AsyncSetClipboardData() = default;
    bool IsValid() const {
      // If this request is no longer valid, the callback should be notified.
      MOZ_ASSERT_IF(!mClipboard, !mCallback);
      return !!mClipboard;
    }
    void MaybeNotifyCallback(nsresult aResult);

    // The clipboard type defined in nsIClipboard.
    int32_t mClipboardType;
    // It is safe to use a raw pointer as it will be nullified (by calling
    // NotifyCallback()) once ClipboardSetDataHelper stops tracking us. This is
    // also used to indicate whether this request is valid.
    ClipboardSetDataHelper* mClipboard;
    // mCallback will be nullified once the callback is notified to ensure the
    // callback is only notified once.
    nsCOMPtr<nsIAsyncSetClipboardDataCallback> mCallback;
  };

 private:
  void RejectPendingAsyncSetDataRequestIfAny(int32_t aClipboardType);

  // Track the pending request for each clipboard type separately. And only need
  // to track the latest request for each clipboard type as the prior pending
  // request will be canceled when a new request is made.
  RefPtr<AsyncSetClipboardData>
      mPendingWriteRequests[nsIClipboard::kClipboardTypeCount];
};

/**
 * A base clipboard class for Windows and Cocoa widget.
 */
class nsBaseClipboard : public ClipboardSetDataHelper {
 public:
  nsBaseClipboard();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIClipboard
  NS_IMETHOD SetData(nsITransferable* aTransferable, nsIClipboardOwner* anOwner,
                     int32_t aWhichClipboard) override;
  NS_IMETHOD GetData(nsITransferable* aTransferable,
                     int32_t aWhichClipboard) override;
  NS_IMETHOD EmptyClipboard(int32_t aWhichClipboard) override;
  NS_IMETHOD HasDataMatchingFlavors(const nsTArray<nsCString>& aFlavorList,
                                    int32_t aWhichClipboard,
                                    bool* _retval) override;
  NS_IMETHOD IsClipboardTypeSupported(int32_t aWhichClipboard,
                                      bool* _retval) override;
  RefPtr<mozilla::GenericPromise> AsyncGetData(
      nsITransferable* aTransferable, int32_t aWhichClipboard) override;
  RefPtr<DataFlavorsPromise> AsyncHasDataMatchingFlavors(
      const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard) override;

 protected:
  virtual ~nsBaseClipboard();

  // Implement the native clipboard behavior.
  NS_IMETHOD GetNativeClipboardData(nsITransferable* aTransferable,
                                    int32_t aWhichClipboard) = 0;

  void ClearClipboardCache();

  bool mEmptyingForSetData;
  nsCOMPtr<nsIClipboardOwner> mClipboardOwner;
  nsCOMPtr<nsITransferable> mTransferable;

 private:
  bool mIgnoreEmptyNotification = false;
};

#endif  // nsBaseClipboard_h__
