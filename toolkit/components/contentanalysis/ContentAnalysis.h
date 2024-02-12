/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_contentanalysis_h
#define mozilla_contentanalysis_h

#include "mozilla/DataMutex.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "nsIContentAnalysis.h"
#include "nsProxyRelease.h"
#include "nsString.h"
#include "nsTHashMap.h"

#include <atomic>
#include <string>

namespace content_analysis::sdk {
class Client;
class ContentAnalysisRequest;
class ContentAnalysisResponse;
}  // namespace content_analysis::sdk

namespace mozilla::contentanalysis {

class ContentAnalysisRequest final : public nsIContentAnalysisRequest {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTANALYSISREQUEST

  ContentAnalysisRequest(AnalysisType aAnalysisType, nsString aString,
                         bool aStringIsFilePath, nsCString aSha256Digest,
                         nsString aUrl, OperationType aOperationType,
                         dom::WindowGlobalParent* aWindowGlobalParent);
  static nsresult GetFileDigest(const nsAString& aFilePath,
                                nsCString& aDigestString);

 private:
  ~ContentAnalysisRequest() = default;
  // Remove unneeded copy constructor/assignment
  ContentAnalysisRequest(const ContentAnalysisRequest&) = delete;
  ContentAnalysisRequest& operator=(ContentAnalysisRequest&) = delete;

  // See nsIContentAnalysisRequest for values
  AnalysisType mAnalysisType;

  // Text content to analyze.  Only one of textContent or filePath is defined.
  nsString mTextContent;

  // Name of file to analyze.  Only one of textContent or filePath is defined.
  nsString mFilePath;

  // The URL containing the file download/upload or to which web content is
  // being uploaded.
  nsString mUrl;

  // Sha256 digest of file.
  nsCString mSha256Digest;

  // URLs involved in the download.
  nsTArray<RefPtr<nsIClientDownloadResource>> mResources;

  // Email address of user.
  nsString mEmail;

  // Unique identifier for this request
  nsCString mRequestToken;

  // Type of text to display, see nsIContentAnalysisRequest for values
  OperationType mOperationTypeForDisplay;

  // String to display if mOperationTypeForDisplay is
  // OPERATION_CUSTOMDISPLAYSTRING
  nsString mOperationDisplayString;

  RefPtr<dom::WindowGlobalParent> mWindowGlobalParent;
};

#define CONTENTANALYSIS_IID                          \
  {                                                  \
    0xa37bed74, 0x4b50, 0x443a, {                    \
      0xbf, 0x58, 0xf4, 0xeb, 0xbd, 0x30, 0x67, 0xb4 \
    }                                                \
  }

class ContentAnalysisResponse;
class ContentAnalysis final : public nsIContentAnalysis {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(CONTENTANALYSIS_IID)
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICONTENTANALYSIS

  ContentAnalysis();

 private:
  ~ContentAnalysis();
  // Remove unneeded copy constructor/assignment
  ContentAnalysis(const ContentAnalysis&) = delete;
  ContentAnalysis& operator=(ContentAnalysis&) = delete;
  nsresult CreateContentAnalysisClient(nsCString&& aPipePathName,
                                       bool aIsPerUser);
  nsresult RunAnalyzeRequestTask(
      const RefPtr<nsIContentAnalysisRequest>& aRequest, bool aAutoAcknowledge,
      const RefPtr<nsIContentAnalysisCallback>& aCallback);
  nsresult RunAcknowledgeTask(
      nsIContentAnalysisAcknowledgement* aAcknowledgement,
      const nsACString& aRequestToken);
  nsresult CancelWithError(nsCString aRequestToken, nsresult aResult);
  static RefPtr<ContentAnalysis> GetContentAnalysisFromService();
  static void DoAnalyzeRequest(
      nsCString aRequestToken,
      content_analysis::sdk::ContentAnalysisRequest&& aRequest,
      const std::shared_ptr<content_analysis::sdk::Client>& aClient);

  using ClientPromise =
      MozPromise<std::shared_ptr<content_analysis::sdk::Client>, nsresult,
                 false>;
  RefPtr<ClientPromise::Private> mCaClientPromise;
  // Only accessed from the main thread
  bool mClientCreationAttempted;

  class CallbackData final {
   public:
    CallbackData(
        nsMainThreadPtrHandle<nsIContentAnalysisCallback>&& aCallbackHolder,
        bool aAutoAcknowledge)
        : mCallbackHolder(aCallbackHolder),
          mAutoAcknowledge(aAutoAcknowledge) {}

    nsMainThreadPtrHandle<nsIContentAnalysisCallback> TakeCallbackHolder() {
      return std::move(mCallbackHolder);
    }
    bool AutoAcknowledge() const { return mAutoAcknowledge; }
    void SetCanceled() { mCallbackHolder = nullptr; }
    bool Canceled() const { return !mCallbackHolder; }

   private:
    nsMainThreadPtrHandle<nsIContentAnalysisCallback> mCallbackHolder;
    bool mAutoAcknowledge;
  };
  DataMutex<nsTHashMap<nsCString, CallbackData>> mCallbackMap;

  struct WarnResponseData {
    WarnResponseData(CallbackData&& aCallbackData,
                     RefPtr<ContentAnalysisResponse> aResponse)
        : mCallbackData(std::move(aCallbackData)), mResponse(aResponse) {}
    ContentAnalysis::CallbackData mCallbackData;
    RefPtr<ContentAnalysisResponse> mResponse;
  };
  DataMutex<nsTHashMap<nsCString, WarnResponseData>> mWarnResponseDataMap;

  friend class ContentAnalysisResponse;
};

NS_DEFINE_STATIC_IID_ACCESSOR(ContentAnalysis, CONTENTANALYSIS_IID)

class ContentAnalysisResponse final : public nsIContentAnalysisResponse {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTANALYSISRESPONSE

  static RefPtr<ContentAnalysisResponse> FromAction(
      Action aAction, const nsACString& aRequestToken);

  void SetOwner(RefPtr<ContentAnalysis> aOwner);

 private:
  ~ContentAnalysisResponse() = default;
  // Remove unneeded copy constructor/assignment
  ContentAnalysisResponse(const ContentAnalysisResponse&) = delete;
  ContentAnalysisResponse& operator=(ContentAnalysisResponse&) = delete;
  explicit ContentAnalysisResponse(
      content_analysis::sdk::ContentAnalysisResponse&& aResponse);
  ContentAnalysisResponse(Action aAction, const nsACString& aRequestToken);
  static already_AddRefed<ContentAnalysisResponse> FromProtobuf(
      content_analysis::sdk::ContentAnalysisResponse&& aResponse);

  void ResolveWarnAction(bool aAllowContent);

  // Action requested by the agent
  Action mAction;

  // Identifier for the corresponding nsIContentAnalysisRequest
  nsCString mRequestToken;

  // ContentAnalysis (or, more precisely, it's Client object) must outlive
  // the transaction.
  RefPtr<ContentAnalysis> mOwner;

  // Whether the response has been acknowledged
  bool mHasAcknowledged;

  friend class ContentAnalysis;
};

class ContentAnalysisAcknowledgement final
    : public nsIContentAnalysisAcknowledgement {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICONTENTANALYSISACKNOWLEDGEMENT

  ContentAnalysisAcknowledgement(Result aResult, FinalAction aFinalAction);

 private:
  ~ContentAnalysisAcknowledgement() = default;

  Result mResult;
  FinalAction mFinalAction;
};

class ContentAnalysisCallback final : public nsIContentAnalysisCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICONTENTANALYSISCALLBACK
  ContentAnalysisCallback(std::function<void(nsIContentAnalysisResponse*)>&&
                              aContentResponseCallback,
                          std::function<void(nsresult)>&& aErrorCallback)
      : mContentResponseCallback(std::move(aContentResponseCallback)),
        mErrorCallback(std::move(aErrorCallback)) {}

 private:
  ~ContentAnalysisCallback() = default;
  explicit ContentAnalysisCallback(RefPtr<dom::Promise> aPromise);
  std::function<void(nsIContentAnalysisResponse*)> mContentResponseCallback;
  std::function<void(nsresult)> mErrorCallback;
  Maybe<nsMainThreadPtrHandle<dom::Promise>> mPromise;
  friend class ContentAnalysis;
};

}  // namespace mozilla::contentanalysis

#endif  // mozilla_contentanalysis_h
