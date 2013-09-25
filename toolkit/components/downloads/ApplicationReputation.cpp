/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ApplicationReputation.h"
#include "csd.pb.h"

#include "nsIApplicationReputation.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIIOService.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsIStreamListener.h"
#include "nsIStringStream.h"
#include "nsIUploadChannel2.h"
#include "nsIURI.h"

#include "mozilla/Preferences.h"
#include "mozilla/Services.h"

#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXPCOMStrings.h"

using mozilla::Preferences;

// Preferences that we need to initialize the query. We may need another
// preference than browser.safebrowsing.malware.enabled, or simply use
// browser.safebrowsing.appRepURL. See bug 887041.
#define PREF_SB_APP_REP_URL "browser.safebrowsing.appRepURL"
#define PREF_SB_MALWARE_ENABLED "browser.safebrowsing.malware.enabled"
#define PREF_GENERAL_LOCALE "general.useragent.locale"

NS_IMPL_ISUPPORTS1(ApplicationReputationService, nsIApplicationReputationService)

ApplicationReputationService* ApplicationReputationService::gApplicationReputationService = nullptr;

ApplicationReputationService *
ApplicationReputationService::GetSingleton()
{
  if (gApplicationReputationService) {
    NS_ADDREF(gApplicationReputationService);
    return gApplicationReputationService;
  }

  gApplicationReputationService = new ApplicationReputationService();
  if (gApplicationReputationService) {
    NS_ADDREF(gApplicationReputationService);
  }

  return gApplicationReputationService;
}

ApplicationReputationService::ApplicationReputationService() { }
ApplicationReputationService::~ApplicationReputationService() { }

NS_IMETHODIMP
ApplicationReputationService::QueryReputation(
  nsIApplicationReputationQuery* aQuery,
  nsIApplicationReputationCallback* aCallback) {
  NS_ENSURE_ARG_POINTER(aQuery);
  NS_ENSURE_ARG_POINTER(aCallback);

  nsresult rv = QueryReputationInternal(aQuery, aCallback);
  if (NS_FAILED(rv)) {
    aCallback->OnComplete(false, rv);
    aCallback = nullptr;
  }
  return NS_OK;
}

nsresult
ApplicationReputationService::QueryReputationInternal(
  nsIApplicationReputationQuery* aQuery,
  nsIApplicationReputationCallback* aCallback) {
  nsresult rv;
  aQuery->SetCallback(aCallback);

  // If malware checks aren't enabled, don't query application reputation.
  if (!Preferences::GetBool(PREF_SB_MALWARE_ENABLED, false)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // If there is no service URL for querying application reputation, abort.
  nsCString serviceUrl;
  NS_ENSURE_SUCCESS(Preferences::GetCString(PREF_SB_APP_REP_URL, &serviceUrl),
                    NS_ERROR_NOT_AVAILABLE);
  if (serviceUrl.EqualsLiteral("")) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  safe_browsing::ClientDownloadRequest req;

  nsCString spec;
  nsCOMPtr<nsIURI> aURI;
  rv = aQuery->GetSourceURI(getter_AddRefs(aURI));
  NS_ENSURE_SUCCESS(rv, rv);
  // If the URI hasn't been set, bail
  NS_ENSURE_STATE(aURI);
  rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  req.set_url(spec.get());
  uint32_t fileSize;
  rv = aQuery->GetFileSize(&fileSize);
  NS_ENSURE_SUCCESS(rv, rv);
  req.set_length(fileSize);
  // We have no way of knowing whether or not a user initiated the download.
  req.set_user_initiated(false);

  nsCString locale;
  NS_ENSURE_SUCCESS(Preferences::GetCString(PREF_GENERAL_LOCALE, &locale),
                    NS_ERROR_NOT_AVAILABLE);
  req.set_locale(locale.get());
  nsCString sha256Hash;
  rv = aQuery->GetSha256Hash(sha256Hash);
  NS_ENSURE_SUCCESS(rv, rv);
  req.mutable_digests()->set_sha256(sha256Hash.Data());
  nsString fileName;
  rv = aQuery->GetSuggestedFileName(fileName);
  NS_ENSURE_SUCCESS(rv, rv);
  req.set_file_basename(NS_ConvertUTF16toUTF8(fileName).get());

  // Serialize the protocol buffer to a string. This can only fail if we are
  // out of memory, or if the protocol buffer req is missing required fields
  // (only the URL for now).
  std::string serialized;
  if (!req.SerializeToString(&serialized)) {
    return NS_ERROR_UNEXPECTED;
  }

  // Set the input stream to the serialized protocol buffer
  nsCOMPtr<nsIStringInputStream> sstream =
    do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = sstream->SetData(serialized.c_str(), serialized.length());
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIIOService> ios = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set up the channel to transmit the request to the service.
  nsCOMPtr<nsIChannel> channel;
  rv = ios->NewChannel(serviceUrl, nullptr, nullptr, getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // See bug 887044 for finalizing the user agent.
  const nsCString userAgent = NS_LITERAL_CSTRING("CsdTesting/Mozilla");
  rv = httpChannel->SetRequestHeader(
    NS_LITERAL_CSTRING("User-Agent"), userAgent, false);
  NS_ENSURE_SUCCESS(rv, rv);

  // Upload the protobuf to the application reputation service.
  nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(channel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = uploadChannel->ExplicitSetUploadStream(sstream,
    NS_LITERAL_CSTRING("application/octet-stream"), serialized.size(),
    NS_LITERAL_CSTRING("POST"), false);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(aQuery, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = channel->AsyncOpen(listener, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMPL_ISUPPORTS3(ApplicationReputationQuery,
                   nsIApplicationReputationQuery,
                   nsIStreamListener,
                   nsIRequestObserver)

ApplicationReputationQuery::ApplicationReputationQuery() :
  mURI(nullptr),
  mFileSize(0),
  mCallback(nullptr) {
}

ApplicationReputationQuery::~ApplicationReputationQuery() {
}

NS_IMETHODIMP
ApplicationReputationQuery::GetSourceURI(nsIURI** aURI) {
  *aURI = mURI;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
ApplicationReputationQuery::SetSourceURI(nsIURI* aURI) {
  mURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP
ApplicationReputationQuery::GetSuggestedFileName(
  nsAString& aSuggestedFileName) {
  aSuggestedFileName = mSuggestedFileName;
  return NS_OK;
}

NS_IMETHODIMP
ApplicationReputationQuery::SetSuggestedFileName(
  const nsAString& aSuggestedFileName) {
  mSuggestedFileName = aSuggestedFileName;
  return NS_OK;
}

NS_IMETHODIMP
ApplicationReputationQuery::GetFileSize(uint32_t* aFileSize) {
  *aFileSize = mFileSize;
  return NS_OK;
}

NS_IMETHODIMP
ApplicationReputationQuery::SetFileSize(uint32_t aFileSize) {
  mFileSize = aFileSize;
  return NS_OK;
}

NS_IMETHODIMP
ApplicationReputationQuery::GetSha256Hash(nsACString& aSha256Hash) {
  aSha256Hash = mSha256Hash;
  return NS_OK;
}

NS_IMETHODIMP
ApplicationReputationQuery::SetSha256Hash(const nsACString& aSha256Hash) {
  mSha256Hash = aSha256Hash;
  return NS_OK;
}

NS_IMETHODIMP
ApplicationReputationQuery::GetCallback(
  nsIApplicationReputationCallback** aCallback) {
  *aCallback = mCallback;
  return NS_OK;
}

NS_IMETHODIMP
ApplicationReputationQuery::SetCallback(
  nsIApplicationReputationCallback* aCallback) {
  mCallback = aCallback;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIStreamListener

static NS_METHOD
AppendSegmentToString(nsIInputStream* inputStream,
                      void *closure,
                      const char *rawSegment,
                      uint32_t toOffset,
                      uint32_t count,
                      uint32_t *writeCount) {
  nsAutoCString* decodedData = static_cast<nsAutoCString*>(closure);
  decodedData->Append(rawSegment, count);
  *writeCount = count;
  return NS_OK;
}

NS_IMETHODIMP
ApplicationReputationQuery::OnDataAvailable(nsIRequest *aRequest,
                                            nsISupports *aContext,
                                            nsIInputStream *aStream,
                                            uint64_t offset,
                                            uint32_t count) {
  uint32_t read;
  return aStream->ReadSegments(AppendSegmentToString, &mResponse, count, &read);
}

NS_IMETHODIMP
ApplicationReputationQuery::OnStartRequest(nsIRequest *aRequest,
                                      nsISupports *aContext) {
  return NS_OK;
}

NS_IMETHODIMP
ApplicationReputationQuery::OnStopRequest(nsIRequest *aRequest,
                                          nsISupports *aContext,
                                          nsresult aResult) {
  NS_ENSURE_STATE(mCallback);

  bool shouldBlock = false;
  nsresult rv = OnStopRequestInternal(aRequest, aContext, aResult,
                                      &shouldBlock);
  mCallback->OnComplete(shouldBlock, rv);
  mCallback = nullptr;
  return rv;
}

nsresult
ApplicationReputationQuery::OnStopRequestInternal(nsIRequest *aRequest,
                                                  nsISupports *aContext,
                                                  nsresult aResult,
                                                  bool* aShouldBlock) {
  *aShouldBlock = false;
  nsresult rv;
  nsCOMPtr<nsIHttpChannel> channel = do_QueryInterface(aRequest, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t status = 0;
  rv = channel->GetResponseStatus(&status);
  NS_ENSURE_SUCCESS(rv, rv);

  if (status != 200) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  std::string buf(mResponse.Data(), mResponse.Length());
  safe_browsing::ClientDownloadResponse response;
  if (!response.ParseFromString(buf)) {
    NS_WARNING("Could not parse protocol buffer");
    return NS_ERROR_CANNOT_CONVERT_DATA;
  }

  // There are several more verdicts, but we only respect one for now and treat
  // everything else as SAFE.
  if (response.verdict() == safe_browsing::ClientDownloadResponse::DANGEROUS) {
    *aShouldBlock = true;
  }

  return NS_OK;
}
