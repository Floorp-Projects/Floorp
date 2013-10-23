/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 sw=4 sts=4 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppProtocolHandler.h"
#include "nsBaseChannel.h"
#include "nsJARChannel.h"
#include "nsNetCID.h"
#include "nsIAppsService.h"
#include "nsCxPusher.h"
#include "nsXULAppAPI.h"

/**
  * This dummy channel implementation only provides enough functionality
  * to return a fake 404 error when the caller asks for an app:// URL
  * containing an unknown appId.
  */
class DummyChannel : public nsIJARChannel
                          , nsRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHANNEL
  NS_DECL_NSIJARCHANNEL

  DummyChannel();

  NS_IMETHODIMP Run();

private:
  bool                        mPending;
  uint32_t                    mSuspendCount;
  nsCOMPtr<nsISupports>       mListenerContext;
  nsCOMPtr<nsIStreamListener> mListener;
  nsCOMPtr<nsILoadGroup>      mLoadGroup;
  nsLoadFlags                 mLoadFlags;
};

NS_IMPL_ISUPPORTS3(DummyChannel, nsIRequest, nsIChannel, nsIJARChannel)

DummyChannel::DummyChannel() : mPending(false)
                             , mSuspendCount(0)
                             , mLoadFlags(LOAD_NORMAL)
{
}

NS_IMETHODIMP DummyChannel::GetName(nsACString &result)
{
  result = "dummy_app_channel";
  return NS_OK;
}

NS_IMETHODIMP DummyChannel::GetStatus(nsresult *aStatus)
{
  *aStatus = NS_ERROR_FILE_NOT_FOUND;
  return NS_OK;
}

NS_IMETHODIMP DummyChannel::IsPending(bool *aResult)
{
  *aResult = mPending;
  return NS_OK;
}

NS_IMETHODIMP DummyChannel::Suspend()
{
  mSuspendCount++;
  return NS_OK;
}

NS_IMETHODIMP DummyChannel::Resume()
{
  if (mSuspendCount <= 0) {
    return NS_ERROR_UNEXPECTED;
  }

  if (--mSuspendCount == 0) {
    NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL);
  }
  return NS_OK;
}

NS_IMETHODIMP DummyChannel::Open(nsIInputStream**)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::AsyncOpen(nsIStreamListener* aListener, nsISupports* aContext)
{
  mListener = aListener;
  mListenerContext = aContext;
  mPending = true;

  if (mLoadGroup) {
    mLoadGroup->AddRequest(this, aContext);
  }

  if (mSuspendCount == 0) {
    NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL);
  }

  return NS_OK;
}

// nsIJarChannel, needed for XHR to turn NS_ERROR_FILE_NOT_FOUND into
// a 404 error.
NS_IMETHODIMP DummyChannel::GetIsUnsafe(bool *aResult)
{
  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP DummyChannel::SetAppURI(nsIURI *aURI)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::Run()
{
  nsresult rv = mListener->OnStartRequest(this, mListenerContext);
  NS_ENSURE_SUCCESS(rv, rv);
  mPending = false;
  rv = mListener->OnStopRequest(this, mListenerContext, NS_ERROR_FILE_NOT_FOUND);
  NS_ENSURE_SUCCESS(rv, rv);
  if (mLoadGroup) {
    mLoadGroup->RemoveRequest(this, mListenerContext, NS_ERROR_FILE_NOT_FOUND);
  }

  mListener = nullptr;
  mListenerContext = nullptr;
  rv = SetNotificationCallbacks(nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP DummyChannel::Cancel(nsresult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
  *aLoadGroup = mLoadGroup;
  NS_IF_ADDREF(*aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP DummyChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP DummyChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP DummyChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
  mLoadFlags = aLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP DummyChannel::GetOriginalURI(nsIURI**)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::SetOriginalURI(nsIURI*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::GetOwner(nsISupports**)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::SetOwner(nsISupports*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::GetNotificationCallbacks(nsIInterfaceRequestor**)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::SetNotificationCallbacks(nsIInterfaceRequestor*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::GetSecurityInfo(nsISupports**)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::GetContentType(nsACString&)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::SetContentType(const nsACString&)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::GetContentCharset(nsACString&)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::SetContentCharset(const nsACString&)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::GetContentLength(int64_t*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::SetContentLength(int64_t)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::GetContentDisposition(uint32_t*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::SetContentDisposition(uint32_t)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::GetURI(nsIURI**)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::GetContentDispositionFilename(nsAString&)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::SetContentDispositionFilename(nsAString const &)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DummyChannel::GetContentDispositionHeader(nsACString&)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/**
  * app:// protocol implementation.
  */

AppProtocolHandler::AppProtocolHandler() {
}

AppProtocolHandler::~AppProtocolHandler() {
  mAppInfoCache.Clear();
}

NS_IMPL_ISUPPORTS1(AppProtocolHandler, nsIProtocolHandler)

/* static */
nsresult
AppProtocolHandler::Create(nsISupports* aOuter,
                           const nsIID& aIID,
                           void* *aResult)
{
  AppProtocolHandler* ph = new AppProtocolHandler();
  if (ph == nullptr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(ph);
  nsresult rv = ph->QueryInterface(aIID, aResult);
  NS_RELEASE(ph);
  return rv;
}

NS_IMETHODIMP
AppProtocolHandler::GetScheme(nsACString &aResult)
{
  aResult.AssignLiteral("app");
  return NS_OK;
}

NS_IMETHODIMP
AppProtocolHandler::GetDefaultPort(int32_t *aResult)
{
  // No ports for the app protocol.
  *aResult = -1;
  return NS_OK;
}

NS_IMETHODIMP
AppProtocolHandler::GetProtocolFlags(uint32_t *aResult)
{
  *aResult = URI_NOAUTH |
             URI_DANGEROUS_TO_LOAD |
             URI_CROSS_ORIGIN_NEEDS_WEBAPPS_PERM;
  return NS_OK;
}

NS_IMETHODIMP
AppProtocolHandler::NewURI(const nsACString &aSpec,
                           const char *aCharset, // ignore charset info
                           nsIURI *aBaseURI,
                           nsIURI **result)
{
  nsresult rv;
  nsCOMPtr<nsIStandardURL> surl(do_CreateInstance(NS_STANDARDURL_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = surl->Init(nsIStandardURL::URLTYPE_STANDARD, -1, aSpec, aCharset, aBaseURI);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURL> url(do_QueryInterface(surl, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  surl->SetMutable(false);
  NS_ADDREF(*result = url);
  return NS_OK;
}

// We map app://ABCDEF/path/to/file.ext to
// jar:file:///path/to/profile/webapps/ABCDEF/application.zip!/path/to/file.ext
NS_IMETHODIMP
AppProtocolHandler::NewChannel(nsIURI* aUri, nsIChannel* *aResult)
{
  NS_ENSURE_ARG_POINTER(aUri);
  nsJARChannel* channel = new nsJARChannel();
  if (!channel) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsAutoCString host;
  nsresult rv = aUri->GetHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString fileSpec;
  nsCOMPtr<nsIURL> url = do_QueryInterface(aUri);
  rv = url->GetFilePath(fileSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  mozilla::dom::AppInfo appInfo;

  if (!mAppInfoCache.Get(host, &appInfo)) {
    nsCOMPtr<nsIAppsService> appsService = do_GetService(APPS_SERVICE_CONTRACTID);
    if (!appsService) {
      return NS_ERROR_FAILURE;
    }

    JS::Value jsInfo;
    rv = appsService->GetAppInfo(NS_ConvertUTF8toUTF16(host), &jsInfo);
    if (NS_FAILED(rv)) {
      // Return a DummyChannel.
      delete channel;
      NS_IF_ADDREF(*aResult = new DummyChannel());
      return NS_OK;
    }

    mozilla::AutoSafeJSContext cx;
    if (!appInfo.Init(cx, JS::Handle<JS::Value>::fromMarkedLocation(&jsInfo)) ||
        appInfo.mPath.IsEmpty()) {
      printf_stderr("!! No appInfo for %s\n", host.get());
      // Return a DummyChannel.
      delete channel;
      NS_IF_ADDREF(*aResult = new DummyChannel());
      return NS_OK;
    }

    mAppInfoCache.Put(host, appInfo);
  }

  bool noRemote = (appInfo.mIsCoreApp ||
                   XRE_GetProcessType() == GeckoProcessType_Default);

  // In-parent and CoreApps can directly access files, so use jar:file://
  nsAutoCString jarSpec(noRemote ? "jar:file://"
                                 : "jar:remoteopenfile://");
  jarSpec += NS_ConvertUTF16toUTF8(appInfo.mPath) +
             NS_LITERAL_CSTRING("/application.zip!") +
             fileSpec;

  nsCOMPtr<nsIURI> jarURI;
  rv = NS_NewURI(getter_AddRefs(jarURI),
                 jarSpec, nullptr, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = channel->Init(jarURI);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = channel->SetAppURI(aUri);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = channel->SetOriginalURI(aUri);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*aResult = channel);
  return NS_OK;
}

NS_IMETHODIMP
AppProtocolHandler::AllowPort(int32_t aPort, const char *aScheme, bool *aRetval)
{
  // No port allowed for this scheme.
  *aRetval = false;
  return NS_OK;
}

