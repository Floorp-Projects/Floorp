/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseClipboard_h__
#define nsBaseClipboard_h__

#include "mozilla/dom/PContent.h"
#include "mozilla/Logging.h"
#include "mozilla/Result.h"
#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsCOMPtr.h"

static mozilla::LazyLogModule sWidgetClipboardLog("WidgetClipboard");
#define MOZ_CLIPBOARD_LOG(...) \
  MOZ_LOG(sWidgetClipboardLog, LogLevel::Debug, (__VA_ARGS__))
#define MOZ_CLIPBOARD_LOG_ENABLED() \
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
  explicit nsBaseClipboard(
      const mozilla::dom::ClipboardCapabilities& aClipboardCaps);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIClipboard
  NS_IMETHOD SetData(nsITransferable* aTransferable, nsIClipboardOwner* anOwner,
                     int32_t aWhichClipboard) override final;
  NS_IMETHOD GetData(nsITransferable* aTransferable,
                     int32_t aWhichClipboard) override final;
  NS_IMETHOD EmptyClipboard(int32_t aWhichClipboard) override final;
  NS_IMETHOD HasDataMatchingFlavors(const nsTArray<nsCString>& aFlavorList,
                                    int32_t aWhichClipboard,
                                    bool* aOutResult) override final;
  NS_IMETHOD IsClipboardTypeSupported(int32_t aWhichClipboard,
                                      bool* aRetval) override final;
  RefPtr<mozilla::GenericPromise> AsyncGetData(
      nsITransferable* aTransferable, int32_t aWhichClipboard) override final;
  RefPtr<DataFlavorsPromise> AsyncHasDataMatchingFlavors(
      const nsTArray<nsCString>& aFlavorList,
      int32_t aWhichClipboard) override final;

 protected:
  virtual ~nsBaseClipboard() = default;

  // Implement the native clipboard behavior.
  NS_IMETHOD GetNativeClipboardData(nsITransferable* aTransferable,
                                    int32_t aWhichClipboard) = 0;
  virtual nsresult EmptyNativeClipboardData(int32_t aWhichClipboard) = 0;
  virtual mozilla::Result<int32_t, nsresult> GetNativeClipboardSequenceNumber(
      int32_t aWhichClipboard) = 0;
  virtual mozilla::Result<bool, nsresult> HasNativeClipboardDataMatchingFlavors(
      const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard) = 0;

 private:
  class ClipboardCache final {
   public:
    ~ClipboardCache() {
      // In order to notify the old clipboard owner.
      Clear();
    }

    /**
     * Clear the cached transferable and notify the original clipboard owner
     * that it has lost ownership.
     */
    void Clear();
    void Update(nsITransferable* aTransferable,
                nsIClipboardOwner* aClipboardOwner, int32_t aSequenceNumber) {
      // Clear first to notify the old clipboard owner.
      Clear();
      mTransferable = aTransferable;
      mClipboardOwner = aClipboardOwner;
      mSequenceNumber = aSequenceNumber;
    }
    nsITransferable* GetTransferable() const { return mTransferable; }
    nsIClipboardOwner* GetClipboardOwner() const { return mClipboardOwner; }
    int32_t GetSequenceNumber() const { return mSequenceNumber; }

   private:
    nsCOMPtr<nsITransferable> mTransferable;
    nsCOMPtr<nsIClipboardOwner> mClipboardOwner;
    int32_t mSequenceNumber = -1;
  };

  // Return clipboard cache if the cached data is valid, otherwise clear the
  // cached data and returns null.
  ClipboardCache* GetClipboardCacheIfValid(int32_t aClipboardType);

  mozilla::UniquePtr<ClipboardCache> mCaches[nsIClipboard::kClipboardTypeCount];
  const mozilla::dom::ClipboardCapabilities mClipboardCaps;
  bool mIgnoreEmptyNotification = false;
};

#endif  // nsBaseClipboard_h__
