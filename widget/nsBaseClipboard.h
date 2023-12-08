/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseClipboard_h__
#define nsBaseClipboard_h__

#include "mozilla/dom/PContent.h"
#include "mozilla/Logging.h"
#include "mozilla/MoveOnlyFunction.h"
#include "mozilla/Result.h"
#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsCOMPtr.h"

static mozilla::LazyLogModule sWidgetClipboardLog("WidgetClipboard");
#define MOZ_CLIPBOARD_LOG(...) \
  MOZ_LOG(sWidgetClipboardLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#define MOZ_CLIPBOARD_LOG_ENABLED() \
  MOZ_LOG_TEST(sWidgetClipboardLog, mozilla::LogLevel::Debug)

class nsITransferable;
class nsIClipboardOwner;
class nsIPrincipal;
class nsIWidget;

namespace mozilla::dom {
class WindowContext;
}  // namespace mozilla::dom

/**
 * A base clipboard class for all platform, so that they can share the same
 * implementation.
 */
class nsBaseClipboard : public nsIClipboard {
 public:
  explicit nsBaseClipboard(
      const mozilla::dom::ClipboardCapabilities& aClipboardCaps);

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIClipboard
  NS_IMETHOD SetData(nsITransferable* aTransferable, nsIClipboardOwner* aOwner,
                     int32_t aWhichClipboard) override final;
  NS_IMETHOD AsyncSetData(int32_t aWhichClipboard,
                          nsIAsyncClipboardRequestCallback* aCallback,
                          nsIAsyncSetClipboardData** _retval) override final;
  NS_IMETHOD GetData(nsITransferable* aTransferable,
                     int32_t aWhichClipboard) override final;
  NS_IMETHOD AsyncGetData(
      const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard,
      mozilla::dom::WindowContext* aRequestingWindowContext,
      nsIPrincipal* aRequestingPrincipal,
      nsIAsyncClipboardGetCallback* aCallback) override final;
  NS_IMETHOD EmptyClipboard(int32_t aWhichClipboard) override final;
  NS_IMETHOD HasDataMatchingFlavors(const nsTArray<nsCString>& aFlavorList,
                                    int32_t aWhichClipboard,
                                    bool* aOutResult) override final;
  NS_IMETHOD IsClipboardTypeSupported(int32_t aWhichClipboard,
                                      bool* aRetval) override final;

  void AsyncGetDataInternal(const nsTArray<nsCString>& aFlavorList,
                            int32_t aClipboardType,
                            nsIAsyncClipboardGetCallback* aCallback);

  using GetDataCallback = mozilla::MoveOnlyFunction<void(nsresult)>;
  using HasMatchingFlavorsCallback = mozilla::MoveOnlyFunction<void(
      mozilla::Result<nsTArray<nsCString>, nsresult>)>;

 protected:
  virtual ~nsBaseClipboard();

  // Implement the native clipboard behavior.
  NS_IMETHOD SetNativeClipboardData(nsITransferable* aTransferable,
                                    int32_t aWhichClipboard) = 0;
  NS_IMETHOD GetNativeClipboardData(nsITransferable* aTransferable,
                                    int32_t aWhichClipboard) = 0;
  virtual void AsyncGetNativeClipboardData(nsITransferable* aTransferable,
                                           int32_t aWhichClipboard,
                                           GetDataCallback&& aCallback);
  virtual nsresult EmptyNativeClipboardData(int32_t aWhichClipboard) = 0;
  virtual mozilla::Result<int32_t, nsresult> GetNativeClipboardSequenceNumber(
      int32_t aWhichClipboard) = 0;
  virtual mozilla::Result<bool, nsresult> HasNativeClipboardDataMatchingFlavors(
      const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard) = 0;
  virtual void AsyncHasNativeClipboardDataMatchingFlavors(
      const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard,
      HasMatchingFlavorsCallback&& aCallback);

  void ClearClipboardCache(int32_t aClipboardType);

 private:
  void RejectPendingAsyncSetDataRequestIfAny(int32_t aClipboardType);

  class AsyncSetClipboardData final : public nsIAsyncSetClipboardData {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIASYNCSETCLIPBOARDDATA

    AsyncSetClipboardData(int32_t aClipboardType, nsBaseClipboard* aClipboard,
                          nsIAsyncClipboardRequestCallback* aCallback);

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
    // NotifyCallback()) once nsBaseClipboard stops tracking us. This is
    // also used to indicate whether this request is valid.
    nsBaseClipboard* mClipboard;
    // mCallback will be nullified once the callback is notified to ensure the
    // callback is only notified once.
    nsCOMPtr<nsIAsyncClipboardRequestCallback> mCallback;
  };

  class AsyncGetClipboardData final : public nsIAsyncGetClipboardData {
   public:
    AsyncGetClipboardData(int32_t aClipboardType, int32_t aSequenceNumber,
                          nsTArray<nsCString>&& aFlavors, bool aFromCache,
                          nsBaseClipboard* aClipboard);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIASYNCGETCLIPBOARDDATA

   private:
    virtual ~AsyncGetClipboardData() = default;
    bool IsValid();

    // The clipboard type defined in nsIClipboard.
    const int32_t mClipboardType;
    // The sequence number associated with the clipboard content for this
    // request. If it doesn't match with the current sequence number in system
    // clipboard, this request targets stale data and is deemed invalid.
    const int32_t mSequenceNumber;
    // List of available data types for clipboard content.
    const nsTArray<nsCString> mFlavors;
    // Data should be read from cache.
    const bool mFromCache;
    // This is also used to indicate whether this request is still valid.
    RefPtr<nsBaseClipboard> mClipboard;
  };

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
    nsresult GetData(nsITransferable* aTransferable) const;

   private:
    nsCOMPtr<nsITransferable> mTransferable;
    nsCOMPtr<nsIClipboardOwner> mClipboardOwner;
    int32_t mSequenceNumber = -1;
  };

  void MaybeRetryGetAvailableFlavors(const nsTArray<nsCString>& aFlavorList,
                                     int32_t aWhichClipboard,
                                     nsIAsyncClipboardGetCallback* aCallback,
                                     int32_t aRetryCount);

  // Return clipboard cache if the cached data is valid, otherwise clear the
  // cached data and returns null.
  ClipboardCache* GetClipboardCacheIfValid(int32_t aClipboardType);

  mozilla::Result<nsTArray<nsCString>, nsresult> GetFlavorsFromClipboardCache(
      int32_t aClipboardType);
  nsresult GetDataFromClipboardCache(nsITransferable* aTransferable,
                                     int32_t aClipboardType);

  void RequestUserConfirmation(int32_t aClipboardType,
                               const nsTArray<nsCString>& aFlavorList,
                               mozilla::dom::WindowContext* aWindowContext,
                               nsIPrincipal* aRequestingPrincipal,
                               nsIAsyncClipboardGetCallback* aCallback);

  // Track the pending request for each clipboard type separately. And only need
  // to track the latest request for each clipboard type as the prior pending
  // request will be canceled when a new request is made.
  RefPtr<AsyncSetClipboardData>
      mPendingWriteRequests[nsIClipboard::kClipboardTypeCount];

  mozilla::UniquePtr<ClipboardCache> mCaches[nsIClipboard::kClipboardTypeCount];
  const mozilla::dom::ClipboardCapabilities mClipboardCaps;
  bool mIgnoreEmptyNotification = false;
};

#endif  // nsBaseClipboard_h__
