/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_contentanalysis_h
#define mozilla_contentanalysis_h

#include "mozilla/DataMutex.h"
#include "mozilla/MoveOnlyFunction.h"
#include "mozilla/MozPromise.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/MaybeDiscarded.h"
#include "mozilla/dom/Promise.h"
#include "nsIContentAnalysis.h"
#include "nsProxyRelease.h"
#include "nsString.h"
#include "nsTHashMap.h"

#include <atomic>
#include <regex>
#include <string>

#ifdef XP_WIN
#  include <windows.h>
#endif  // XP_WIN

class nsBaseClipboard;
class nsIPrincipal;
class nsIPrintSettings;
class ContentAnalysisTest;

namespace mozilla::dom {
class CanonicalBrowsingContext;
class DataTransfer;
class WindowGlobalParent;
}  // namespace mozilla::dom

namespace content_analysis::sdk {
class Client;
class ContentAnalysisRequest;
class ContentAnalysisResponse;
}  // namespace content_analysis::sdk

namespace mozilla::contentanalysis {

enum class DefaultResult : uint8_t {
  eBlock = 0,
  eWarn = 1,
  eAllow = 2,
  eLastValue = 2
};

class ContentAnalysisDiagnosticInfo final
    : public nsIContentAnalysisDiagnosticInfo {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTANALYSISDIAGNOSTICINFO
  ContentAnalysisDiagnosticInfo(bool aConnectedToAgent, nsString aAgentPath,
                                bool aFailedSignatureVerification,
                                int64_t aRequestCount)
      : mConnectedToAgent(aConnectedToAgent),
        mAgentPath(std::move(aAgentPath)),
        mFailedSignatureVerification(aFailedSignatureVerification),
        mRequestCount(aRequestCount) {}

 private:
  ~ContentAnalysisDiagnosticInfo() = default;
  bool mConnectedToAgent;
  nsString mAgentPath;
  bool mFailedSignatureVerification;
  int64_t mRequestCount;
};

class ContentAnalysisRequest final : public nsIContentAnalysisRequest {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTANALYSISREQUEST

  ContentAnalysisRequest(AnalysisType aAnalysisType, nsString aString,
                         bool aStringIsFilePath, nsCString aSha256Digest,
                         nsCOMPtr<nsIURI> aUrl, OperationType aOperationType,
                         dom::WindowGlobalParent* aWindowGlobalParent);
  ContentAnalysisRequest(const nsTArray<uint8_t> aPrintData,
                         nsCOMPtr<nsIURI> aUrl, nsString aPrinterName,
                         dom::WindowGlobalParent* aWindowGlobalParent);
  static nsresult GetFileDigest(const nsAString& aFilePath,
                                nsCString& aDigestString);

 private:
  ~ContentAnalysisRequest();

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
  nsCOMPtr<nsIURI> mUrl;

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

  // The name of the printer being printed to
  nsString mPrinterName;

  RefPtr<dom::WindowGlobalParent> mWindowGlobalParent;
#ifdef XP_WIN
  // The printed data to analyze, in PDF format
  HANDLE mPrintDataHandle = 0;
  // The size of the printed data in mPrintDataHandle
  uint64_t mPrintDataSize = 0;
#endif

  friend class ::ContentAnalysisTest;
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
  nsCString GetUserActionId();
  void SetLastResult(nsresult aLastResult) { mLastResult = aLastResult; }

#if defined(XP_WIN)
  struct PrintAllowedResult final {
    bool mAllowed;
    dom::MaybeDiscarded<dom::BrowsingContext>
        mCachedStaticDocumentBrowsingContext;
    PrintAllowedResult(bool aAllowed, dom::MaybeDiscarded<dom::BrowsingContext>
                                          aCachedStaticDocumentBrowsingContext)
        : mAllowed(aAllowed),
          mCachedStaticDocumentBrowsingContext(
              aCachedStaticDocumentBrowsingContext) {}
    explicit PrintAllowedResult(bool aAllowed)
        : PrintAllowedResult(aAllowed, dom::MaybeDiscardedBrowsingContext()) {}
  };
  struct PrintAllowedError final {
    nsresult mError;
    dom::MaybeDiscarded<dom::BrowsingContext>
        mCachedStaticDocumentBrowsingContext;
    PrintAllowedError(nsresult aError, dom::MaybeDiscarded<dom::BrowsingContext>
                                           aCachedStaticDocumentBrowsingContext)
        : mError(aError),
          mCachedStaticDocumentBrowsingContext(
              aCachedStaticDocumentBrowsingContext) {}
    explicit PrintAllowedError(nsresult aError)
        : PrintAllowedError(aError, dom::MaybeDiscardedBrowsingContext()) {}
  };
  using PrintAllowedPromise =
      MozPromise<PrintAllowedResult, PrintAllowedError, true>;
  MOZ_CAN_RUN_SCRIPT static RefPtr<PrintAllowedPromise>
  PrintToPDFToDetermineIfPrintAllowed(
      dom::CanonicalBrowsingContext* aBrowsingContext,
      nsIPrintSettings* aPrintSettings);
#endif  // defined(XP_WIN)

  class SafeContentAnalysisResultCallback final
      : public nsIContentAnalysisCallback {
   public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSICONTENTANALYSISCALLBACK
    explicit SafeContentAnalysisResultCallback(
        std::function<void(RefPtr<nsIContentAnalysisResult>&&)> aResolver)
        : mResolver(std::move(aResolver)) {}
    void Callback(RefPtr<nsIContentAnalysisResult>&& aResult) {
      MOZ_ASSERT(mResolver, "Called SafeContentAnalysisResultCallback twice!");
      if (auto resolver = std::move(mResolver)) {
        resolver(std::move(aResult));
      }
    }

   private:
    ~SafeContentAnalysisResultCallback() {
      MOZ_ASSERT(!mResolver, "SafeContentAnalysisResultCallback never called!");
    }
    mozilla::MoveOnlyFunction<void(RefPtr<nsIContentAnalysisResult>&&)>
        mResolver;
  };
  static bool MightBeActive();
  static bool CheckClipboardContentAnalysisSync(
      nsBaseClipboard* aClipboard, mozilla::dom::WindowGlobalParent* aWindow,
      const nsCOMPtr<nsITransferable>& trans, int32_t aClipboardType);
  static void CheckClipboardContentAnalysis(
      nsBaseClipboard* aClipboard, mozilla::dom::WindowGlobalParent* aWindow,
      nsITransferable* aTransferable, int32_t aClipboardType,
      SafeContentAnalysisResultCallback* aResolver);

 private:
  ~ContentAnalysis();
  // Remove unneeded copy constructor/assignment
  ContentAnalysis(const ContentAnalysis&) = delete;
  ContentAnalysis& operator=(ContentAnalysis&) = delete;
  nsresult CreateContentAnalysisClient(nsCString&& aPipePathName,
                                       nsString&& aClientSignatureSetting,
                                       bool aIsPerUser);
  nsresult AnalyzeContentRequestCallbackPrivate(
      nsIContentAnalysisRequest* aRequest, bool aAutoAcknowledge,
      nsIContentAnalysisCallback* aCallback);

  nsresult RunAnalyzeRequestTask(
      const RefPtr<nsIContentAnalysisRequest>& aRequest, bool aAutoAcknowledge,
      int64_t aRequestCount,
      const RefPtr<nsIContentAnalysisCallback>& aCallback);
  nsresult RunAcknowledgeTask(
      nsIContentAnalysisAcknowledgement* aAcknowledgement,
      const nsACString& aRequestToken);
  nsresult CancelWithError(nsCString aRequestToken, nsresult aResult);
  void GenerateUserActionId();
  static RefPtr<ContentAnalysis> GetContentAnalysisFromService();
  static void DoAnalyzeRequest(
      nsCString aRequestToken,
      content_analysis::sdk::ContentAnalysisRequest&& aRequest,
      const std::shared_ptr<content_analysis::sdk::Client>& aClient);
  void IssueResponse(RefPtr<ContentAnalysisResponse>& response);
  bool LastRequestSucceeded();
  // Did the URL filter completely handle the request or do we need to check
  // with the agent.
  enum UrlFilterResult { eCheck, eDeny, eAllow };

  UrlFilterResult FilterByUrlLists(nsIContentAnalysisRequest* aRequest);
  void EnsureParsedUrlFilters();

  using ClientPromise =
      MozPromise<std::shared_ptr<content_analysis::sdk::Client>, nsresult,
                 false>;
  nsCString mUserActionId;
  int64_t mRequestCount = 0;
  RefPtr<ClientPromise::Private> mCaClientPromise;
  // Only accessed from the main thread
  bool mClientCreationAttempted;

  bool mSetByEnterprise;
  nsresult mLastResult = NS_OK;

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
  void SendWarnResponse(nsCString&& aResponseRequestToken,
                        CallbackData aCallbackData,
                        RefPtr<ContentAnalysisResponse>& aResponse);

  std::vector<std::regex> mAllowUrlList;
  std::vector<std::regex> mDenyUrlList;
  bool mParsedUrlLists = false;

  friend class ContentAnalysisResponse;
  friend class ::ContentAnalysisTest;
};

NS_DEFINE_STATIC_IID_ACCESSOR(ContentAnalysis, CONTENTANALYSIS_IID)

class ContentAnalysisResponse final : public nsIContentAnalysisResponse {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTANALYSISRESPONSE

  static RefPtr<ContentAnalysisResponse> FromAction(
      Action aAction, const nsACString& aRequestToken);

  void SetOwner(RefPtr<ContentAnalysis> aOwner);
  void DoNotAcknowledge() { mDoNotAcknowledge = true; }
  void SetCancelError(CancelError aCancelError);

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

  // If mAction is eCanceled, this is the error explaining why the request was
  // canceled, or eUserInitiated if the user canceled it.
  CancelError mCancelError = CancelError::eUserInitiated;

  // ContentAnalysis (or, more precisely, its Client object) must outlive
  // the transaction.
  RefPtr<ContentAnalysis> mOwner;

  // Whether the response has been acknowledged
  bool mHasAcknowledged = false;

  // If true, the request was completely handled by URL filter lists, so it
  // was not sent to the agent and should not send an Acknowledge.
  bool mDoNotAcknowledge = false;

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
