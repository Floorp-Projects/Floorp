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
#include "nsIPrefService.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamListener.h"
#include "nsIStringStream.h"
#include "nsIUploadChannel2.h"
#include "nsIURI.h"
#include "nsIUrlClassifierDBService.h"

#include "mozilla/Preferences.h"
#include "mozilla/Services.h"

#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsNetCID.h"
#include "nsReadableUtils.h"
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
#define PREF_DOWNLOAD_BLOCK_TABLE "urlclassifier.download_block_table"
#define PREF_DOWNLOAD_ALLOW_TABLE "urlclassifier.download_allow_table"

/**
 * Keep track of pending lookups. Once the ApplicationReputationService creates
 * this, it is guaranteed to call mCallback. This class is private to
 * ApplicationReputationService.
 */
class PendingLookup MOZ_FINAL :
  public nsIStreamListener,
  public nsIUrlClassifierCallback {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIURLCLASSIFIERCALLBACK
  PendingLookup(nsIApplicationReputationQuery* aQuery,
                nsIApplicationReputationCallback* aCallback);
  ~PendingLookup();

private:
  nsCOMPtr<nsIApplicationReputationQuery> mQuery;
  nsCOMPtr<nsIApplicationReputationCallback> mCallback;
  /**
   * The response from the application reputation query. This is read in chunks
   * as part of our nsIStreamListener implementation and may contain embedded
   * NULLs.
   */
  nsCString mResponse;
  /**
   * Clean up and call the callback. PendingLookup must not be used after this
   * function is called.
   */
  nsresult OnComplete(bool shouldBlock, nsresult rv);
  /**
   * Wrapper function for nsIStreamListener.onStopRequest to make it easy to
   * guarantee calling the callback
   */
  nsresult OnStopRequestInternal(nsIRequest *aRequest,
                                 nsISupports *aContext,
                                 nsresult aResult,
                                 bool* aShouldBlock);
  /**
   * Sends a query to the remote application reputation service. Returns NS_OK
   * on success.
   */
  nsresult SendRemoteQuery();
};

NS_IMPL_ISUPPORTS3(PendingLookup,
                   nsIStreamListener,
                   nsIRequestObserver,
                   nsIUrlClassifierCallback)

PendingLookup::PendingLookup(nsIApplicationReputationQuery* aQuery,
                             nsIApplicationReputationCallback* aCallback) :
  mQuery(aQuery),
  mCallback(aCallback) {
}

PendingLookup::~PendingLookup() {
}

nsresult
PendingLookup::OnComplete(bool shouldBlock, nsresult rv) {
  nsresult res = mCallback->OnComplete(shouldBlock, rv);
  return res;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIUrlClassifierCallback
NS_IMETHODIMP
PendingLookup::HandleEvent(const nsACString& tables) {
  // HandleEvent is guaranteed to call the callback if either the URL can be
  // classified locally, or if there is an error sending the remote lookup.
  // Allow listing trumps block listing.
  nsCString allow_list;
  Preferences::GetCString(PREF_DOWNLOAD_ALLOW_TABLE, &allow_list);
  if (FindInReadable(tables, allow_list)) {
    return OnComplete(false, NS_OK);
  }

  nsCString block_list;
  Preferences::GetCString(PREF_DOWNLOAD_BLOCK_TABLE, &block_list);
  if (FindInReadable(tables, block_list)) {
    return OnComplete(true, NS_OK);
  }

  nsresult rv = SendRemoteQuery();
  if (NS_FAILED(rv)) {
    return OnComplete(false, rv);
  }
  return NS_OK;
}

nsresult
PendingLookup::SendRemoteQuery() {
  // We did not find a local result, so fire off the query to the application
  // reputation service.
  safe_browsing::ClientDownloadRequest req;
  nsCOMPtr<nsIURI> uri;
  nsresult rv;
  rv = mQuery->GetSourceURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCString spec;
  rv = uri->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);
  req.set_url(spec.get());

  uint32_t fileSize;
  rv = mQuery->GetFileSize(&fileSize);
  NS_ENSURE_SUCCESS(rv, rv);
  req.set_length(fileSize);
  // We have no way of knowing whether or not a user initiated the download.
  req.set_user_initiated(false);

  nsCString locale;
  NS_ENSURE_SUCCESS(Preferences::GetCString(PREF_GENERAL_LOCALE, &locale),
                    NS_ERROR_NOT_AVAILABLE);
  req.set_locale(locale.get());
  nsCString sha256Hash;
  rv = mQuery->GetSha256Hash(sha256Hash);
  NS_ENSURE_SUCCESS(rv, rv);
  req.mutable_digests()->set_sha256(sha256Hash.Data());
  nsString fileName;
  rv = mQuery->GetSuggestedFileName(fileName);
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
  nsCString serviceUrl;
  NS_ENSURE_SUCCESS(Preferences::GetCString(PREF_SB_APP_REP_URL, &serviceUrl),
                    NS_ERROR_NOT_AVAILABLE);
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

  rv = channel->AsyncOpen(this, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

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
PendingLookup::OnDataAvailable(nsIRequest *aRequest,
                               nsISupports *aContext,
                               nsIInputStream *aStream,
                               uint64_t offset,
                               uint32_t count) {
  uint32_t read;
  return aStream->ReadSegments(AppendSegmentToString, &mResponse, count, &read);
}

NS_IMETHODIMP
PendingLookup::OnStartRequest(nsIRequest *aRequest,
                              nsISupports *aContext) {
  return NS_OK;
}

NS_IMETHODIMP
PendingLookup::OnStopRequest(nsIRequest *aRequest,
                             nsISupports *aContext,
                             nsresult aResult) {
  NS_ENSURE_STATE(mCallback);

  bool shouldBlock = false;
  nsresult rv = OnStopRequestInternal(aRequest, aContext, aResult,
                                      &shouldBlock);
  OnComplete(shouldBlock, rv);
  return rv;
}

nsresult
PendingLookup::OnStopRequestInternal(nsIRequest *aRequest,
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

NS_IMPL_ISUPPORTS1(ApplicationReputationService,
                   nsIApplicationReputationService)

ApplicationReputationService*
  ApplicationReputationService::gApplicationReputationService = nullptr;

ApplicationReputationService*
ApplicationReputationService::GetSingleton()
{
  if (gApplicationReputationService) {
    NS_ADDREF(gApplicationReputationService);
    return gApplicationReputationService;
  }

  // We're not initialized yet.
  gApplicationReputationService = new ApplicationReputationService();
  if (gApplicationReputationService) {
    NS_ADDREF(gApplicationReputationService);
  }

  return gApplicationReputationService;
}

ApplicationReputationService::ApplicationReputationService() :
  mDBService(nullptr),
  mSecurityManager(nullptr) {
}

ApplicationReputationService::~ApplicationReputationService() {
}

NS_IMETHODIMP
ApplicationReputationService::QueryReputation(
    nsIApplicationReputationQuery* aQuery,
    nsIApplicationReputationCallback* aCallback) {
  NS_ENSURE_ARG_POINTER(aQuery);
  NS_ENSURE_ARG_POINTER(aCallback);

  nsresult rv = QueryReputationInternal(aQuery, aCallback);
  if (NS_FAILED(rv)) {
    aCallback->OnComplete(false, rv);
  }
  return NS_OK;
}

nsresult ApplicationReputationService::QueryReputationInternal(
  nsIApplicationReputationQuery* aQuery,
  nsIApplicationReputationCallback* aCallback) {
  // Lazily instantiate mDBService and mSecurityManager
  nsresult rv;
  if (!mDBService) {
    mDBService = do_GetService(NS_URLCLASSIFIERDBSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (!mSecurityManager) {
    mSecurityManager = do_GetService("@mozilla.org/scriptsecuritymanager;1",
                                     &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }
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

  // Create a new pending lookup.
  nsRefPtr<PendingLookup> lookup(new PendingLookup(aQuery, aCallback));
  NS_ENSURE_STATE(lookup);

  nsCOMPtr<nsIURI> uri;
  rv = aQuery->GetSourceURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  // If the URI hasn't been set, bail
  NS_ENSURE_STATE(uri);
  nsCOMPtr<nsIPrincipal> principal;
  // In nsIUrlClassifierDBService.lookup, the only use of the principal is to
  // wrap the URI. nsISecurityManager.getNoAppCodebasePrincipal is the easiest
  // way to wrap a URI inside a principal, since principals can't be
  // constructed.
  rv = mSecurityManager->GetNoAppCodebasePrincipal(uri,
                                                   getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  // Check local lists to see if the URI has already been whitelisted or
  // blacklisted.
  return mDBService->Lookup(principal, lookup);
}
