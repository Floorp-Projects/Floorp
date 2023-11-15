/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_contentanalysis_h
#define mozilla_contentanalysis_h

#include "mozilla/DataMutex.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/Mutex.h"
#include "nsIContentAnalysis.h"
#include "nsProxyRelease.h"
#include "nsString.h"

#include <string>

namespace content_analysis::sdk {
class Client;
class ContentAnalysisResponse;
}  // namespace content_analysis::sdk

namespace mozilla::contentanalysis {

class ContentAnalysisRequest final : public nsIContentAnalysisRequest {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTANALYSISREQUEST

  ContentAnalysisRequest(unsigned long aAnalysisType, nsString&& aString,
                         bool aStringIsFilePath, nsCString&& aSha256Digest,
                         nsString&& aUrl, unsigned long aResourceNameType,
                         dom::WindowGlobalParent* aWindowGlobalParent);

 private:
  ~ContentAnalysisRequest() = default;
  // Remove unneeded copy constructor/assignment
  ContentAnalysisRequest(const ContentAnalysisRequest&) = delete;
  ContentAnalysisRequest& operator=(ContentAnalysisRequest&) = delete;

  // See nsIContentAnalysisRequest for values
  unsigned long mAnalysisType;

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
  unsigned long mOperationTypeForDisplay;

  // String to display if mOperationTypeForDisplay is
  // OPERATION_CUSTOMDISPLAYSTRING
  nsString mOperationDisplayString;

  RefPtr<dom::WindowGlobalParent> mWindowGlobalParent;
};

class ContentAnalysisResponse;
class ContentAnalysis final : public nsIContentAnalysis {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICONTENTANALYSIS

  ContentAnalysis() = default;

 private:
  ~ContentAnalysis();
  // Remove unneeded copy constructor/assignment
  ContentAnalysis(const ContentAnalysis&) = delete;
  ContentAnalysis& operator=(ContentAnalysis&) = delete;
  nsresult EnsureContentAnalysisClient();
  nsresult RunAnalyzeRequestTask(RefPtr<nsIContentAnalysisRequest> aRequest,
                                 RefPtr<nsIContentAnalysisCallback> aCallback);
  nsresult RunAcknowledgeTask(
      nsIContentAnalysisAcknowledgement* aAcknowledgement,
      const nsACString& aRequestToken);

  static StaticDataMutex<UniquePtr<content_analysis::sdk::Client>> sCaClient;
  friend class ContentAnalysisResponse;
};

class ContentAnalysisResponse final : public nsIContentAnalysisResponse {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTANALYSISRESPONSE

  static RefPtr<ContentAnalysisResponse> FromAction(
      unsigned long aAction, const nsACString& aRequestToken);

  void SetOwner(RefPtr<ContentAnalysis> aOwner);

 private:
  ~ContentAnalysisResponse() = default;
  // Remove unneeded copy constructor/assignment
  ContentAnalysisResponse(const ContentAnalysisResponse&) = delete;
  ContentAnalysisResponse& operator=(ContentAnalysisResponse&) = delete;
  explicit ContentAnalysisResponse(
      content_analysis::sdk::ContentAnalysisResponse&& aResponse);
  ContentAnalysisResponse(unsigned long aAction,
                          const nsACString& aRequestToken);
  static already_AddRefed<ContentAnalysisResponse> FromProtobuf(
      content_analysis::sdk::ContentAnalysisResponse&& aResponse);

  // See nsIContentAnalysisResponse for values
  uint32_t mAction;

  // Identifier for the corresponding nsIContentAnalysisRequest
  nsCString mRequestToken;

  // ContentAnalysis (or, more precisely, it's Client object) must outlive
  // the transaction.
  RefPtr<ContentAnalysis> mOwner;

  friend class ContentAnalysis;
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
