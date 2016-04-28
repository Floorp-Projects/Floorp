/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set sw=2 sts=2 ts=2 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PSMContentListener.h"

#include "nsIDivertableChannel.h"
#include "nsIStreamListener.h"
#include "nsIX509CertDB.h"
#include "nsIXULAppInfo.h"

#include "mozilla/Casting.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/net/ChannelDiverterParent.h"
#include "mozilla/net/ChannelDiverterChild.h"

#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "nsNSSHelper.h"

#include "mozilla/Logging.h"

extern mozilla::LazyLogModule gPIPNSSLog;

namespace mozilla { namespace psm {

namespace {

const int32_t kDefaultCertAllocLength = 2048;

enum {
  UNKNOWN_TYPE = 0,
  X509_CA_CERT = 1,
  X509_USER_CERT = 2,
  X509_EMAIL_CERT = 3,
  X509_SERVER_CERT = 4
};

/* other mime types that we should handle sometime:

   application/x-pkcs7-mime
   application/pkcs7-signature
   application/pre-encrypted

*/

uint32_t
getPSMContentType(const char* aContentType)
{
  // Don't forget to update the registration of content listeners in nsNSSModule.cpp
  // for every supported content type.

  if (!nsCRT::strcasecmp(aContentType, "application/x-x509-ca-cert"))
    return X509_CA_CERT;
  if (!nsCRT::strcasecmp(aContentType, "application/x-x509-server-cert"))
    return X509_SERVER_CERT;
  if (!nsCRT::strcasecmp(aContentType, "application/x-x509-user-cert"))
    return X509_USER_CERT;
  if (!nsCRT::strcasecmp(aContentType, "application/x-x509-email-cert"))
    return X509_EMAIL_CERT;

  return UNKNOWN_TYPE;
}

int64_t
ComputeContentLength(nsIRequest* request)
{
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(request));
  if (!channel) {
    return -1;
  }

  int64_t contentLength;
  nsresult rv = channel->GetContentLength(&contentLength);
  if (NS_FAILED(rv) || contentLength <= 0) {
    return kDefaultCertAllocLength;
  }

  if (contentLength > INT32_MAX) {
    return -1;
  }

  return contentLength;
}

} // unnamed namespace

/* ------------------------
 * PSMContentStreamListener
 * ------------------------ */

PSMContentStreamListener::PSMContentStreamListener(uint32_t type)
  : mType(type)
{
}

PSMContentStreamListener::~PSMContentStreamListener()
{
}

NS_IMPL_ISUPPORTS(PSMContentStreamListener, nsIStreamListener, nsIRequestObserver)

NS_IMETHODIMP
PSMContentStreamListener::OnStartRequest(nsIRequest* request, nsISupports* context)
{
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("CertDownloader::OnStartRequest\n"));

  int64_t contentLength = ComputeContentLength(request);
  if (contentLength < 0) {
    return NS_ERROR_FAILURE;
  }

  mByteData.SetCapacity(contentLength);
  return NS_OK;
}

NS_IMETHODIMP
PSMContentStreamListener::OnDataAvailable(nsIRequest* request,
                                          nsISupports* context,
                                          nsIInputStream* aIStream,
                                          uint64_t aSourceOffset,
                                          uint32_t aLength)
{
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("CertDownloader::OnDataAvailable\n"));

  nsCString chunk;
  nsresult rv = NS_ReadInputStreamToString(aIStream, chunk, aLength);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mByteData.Append(chunk);
  return NS_OK;
}

NS_IMETHODIMP
PSMContentStreamListener::OnStopRequest(nsIRequest* request,
                                        nsISupports* context,
                                        nsresult aStatus)
{
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("CertDownloader::OnStopRequest\n"));

  // Because importing the cert can spin the event loop (via alerts), we can't
  // do it here. Do it off the event loop instead.
  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod(this, &PSMContentStreamListener::ImportCertificate);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(r));

  return NS_OK;
}

void
PSMContentStreamListener::ImportCertificate()
{
  nsCOMPtr<nsIX509CertDB> certdb;

  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();

  switch (mType) {
  case X509_CA_CERT:
  case X509_USER_CERT:
  case X509_EMAIL_CERT:
    certdb = do_GetService(NS_X509CERTDB_CONTRACTID);
    break;

  default:
    break;
  }

  if (!certdb) {
    return;
  }

  switch (mType) {
  case X509_CA_CERT:
    certdb->ImportCertificates(reinterpret_cast<uint8_t*>(mByteData.BeginWriting()),
                               mByteData.Length(), mType, ctx);
    break;

  case X509_USER_CERT:
    certdb->ImportUserCertificate(reinterpret_cast<uint8_t*>(mByteData.BeginWriting()),
                                  mByteData.Length(), ctx);
    break;

  case X509_EMAIL_CERT:
    certdb->ImportEmailCertificate(reinterpret_cast<uint8_t*>(mByteData.BeginWriting()),
                                   mByteData.Length(), ctx);
    break;

  default:
    break;
  }
}

/* ------------------------
 * PSMContentDownloaderParent
 * ------------------------ */

PSMContentDownloaderParent::PSMContentDownloaderParent(uint32_t type)
  : PSMContentStreamListener(type)
  , mIPCOpen(true)
{
}

PSMContentDownloaderParent::~PSMContentDownloaderParent()
{
}

bool
PSMContentDownloaderParent::RecvOnStartRequest(const uint32_t& contentLength)
{
  mByteData.SetCapacity(contentLength);
  return true;
}

bool
PSMContentDownloaderParent::RecvOnDataAvailable(const nsCString& data,
                                                const uint64_t& offset,
                                                const uint32_t& count)
{
  mByteData.Append(data);
  return true;
}

bool
PSMContentDownloaderParent::RecvOnStopRequest(const nsresult& code)
{
  if (NS_SUCCEEDED(code)) {
    // See also PSMContentStreamListener::OnStopRequest. In this case, we don't
    // have to dispatch ImportCertificate off of an event because we don't have
    // to worry about Necko sending "clean up" events and destroying us if
    // ImportCertificate spins the event loop.
    ImportCertificate();
  }

  if (mIPCOpen) {
    mozilla::Unused << Send__delete__(this);
  }
  return true;
}

NS_IMETHODIMP
PSMContentDownloaderParent::OnStopRequest(nsIRequest* request, nsISupports* context, nsresult code)
{
  nsresult rv = PSMContentStreamListener::OnStopRequest(request, context, code);

  if (mIPCOpen) {
    mozilla::Unused << Send__delete__(this);
  }
  return rv;
}

bool
PSMContentDownloaderParent::RecvDivertToParentUsing(mozilla::net::PChannelDiverterParent* diverter)
{
  MOZ_ASSERT(diverter);
  auto p = static_cast<mozilla::net::ChannelDiverterParent*>(diverter);
  p->DivertTo(this);
  mozilla::Unused << p->Send__delete__(p);
  return true;
}

void
PSMContentDownloaderParent::ActorDestroy(ActorDestroyReason why)
{
  mIPCOpen = false;
}

/* ------------------------
 * PSMContentDownloaderChild
 * ------------------------ */

NS_IMPL_ISUPPORTS(PSMContentDownloaderChild, nsIStreamListener)

PSMContentDownloaderChild::PSMContentDownloaderChild()
{
}

PSMContentDownloaderChild::~PSMContentDownloaderChild()
{
}

NS_IMETHODIMP
PSMContentDownloaderChild::OnStartRequest(nsIRequest* request, nsISupports* context)
{
  nsCOMPtr<nsIDivertableChannel> divertable = do_QueryInterface(request);
  if (divertable) {
    mozilla::net::ChannelDiverterChild* diverter = nullptr;
    nsresult rv = divertable->DivertToParent(&diverter);
    if (NS_FAILED(rv)) {
      return rv;
    }
    MOZ_ASSERT(diverter);

    return SendDivertToParentUsing(diverter) ? NS_OK : NS_ERROR_FAILURE;
  }

  int64_t contentLength = ComputeContentLength(request);
  if (contentLength < 0) {
    return NS_ERROR_FAILURE;
  }

  mozilla::Unused << SendOnStartRequest(contentLength);
  return NS_OK;
}

NS_IMETHODIMP
PSMContentDownloaderChild::OnDataAvailable(nsIRequest* request,
                                           nsISupports* context,
                                           nsIInputStream* aIStream,
                                           uint64_t aSourceOffset,
                                           uint32_t aLength)
{
  nsCString chunk;
  nsresult rv = NS_ReadInputStreamToString(aIStream, chunk, aLength);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mozilla::Unused << SendOnDataAvailable(chunk, aSourceOffset, aLength);
  return NS_OK;
}

NS_IMETHODIMP
PSMContentDownloaderChild::OnStopRequest(nsIRequest* request,
                                         nsISupports* context,
                                         nsresult aStatus)
{
  mozilla::Unused << SendOnStopRequest(aStatus);
  return NS_OK;
}

/* ------------------------
 * PSMContentListener
 * ------------------------ */

NS_IMPL_ISUPPORTS(PSMContentListener,
                  nsIURIContentListener,
                  nsISupportsWeakReference) 

PSMContentListener::PSMContentListener()
{
  mLoadCookie = nullptr;
  mParentContentListener = nullptr;
}

PSMContentListener::~PSMContentListener()
{
}

nsresult
PSMContentListener::init()
{
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::OnStartURIOpen(nsIURI* aURI, bool* aAbortOpen)
{
  //if we don't want to handle the URI, return true in
  //*aAbortOpen
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::IsPreferred(const char* aContentType,
                                 char** aDesiredContentType,
                                 bool* aCanHandleContent)
{
  return CanHandleContent(aContentType, true,
                          aDesiredContentType, aCanHandleContent);
}

NS_IMETHODIMP
PSMContentListener::CanHandleContent(const char* aContentType,
                                      bool aIsContentPreferred,
                                      char** aDesiredContentType,
                                      bool* aCanHandleContent)
{
  uint32_t type = getPSMContentType(aContentType);
  *aCanHandleContent = (type != UNKNOWN_TYPE);
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::DoContent(const nsACString& aContentType,
                               bool aIsContentPreferred,
                               nsIRequest* aRequest,
                               nsIStreamListener** aContentHandler,
                               bool* aAbortProcess)
{
  uint32_t type;
  type = getPSMContentType(PromiseFlatCString(aContentType).get());
  if (gPIPNSSLog) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("PSMContentListener::DoContent\n"));
  }
  if (type != UNKNOWN_TYPE) {
    nsCOMPtr<nsIStreamListener> downloader;
    if (XRE_IsParentProcess()) {
      downloader = new PSMContentStreamListener(type);
    } else {
      downloader = static_cast<PSMContentDownloaderChild*>(
          dom::ContentChild::GetSingleton()->SendPPSMContentDownloaderConstructor(type));
    }

    downloader.forget(aContentHandler);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PSMContentListener::GetLoadCookie(nsISupports** aLoadCookie)
{
  nsCOMPtr<nsISupports> loadCookie(mLoadCookie);
  loadCookie.forget(aLoadCookie);
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::SetLoadCookie(nsISupports* aLoadCookie)
{
  mLoadCookie = aLoadCookie;
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::GetParentContentListener(nsIURIContentListener** aContentListener)
{
  nsCOMPtr<nsIURIContentListener> listener(mParentContentListener);
  listener.forget(aContentListener);
  return NS_OK;
}

NS_IMETHODIMP
PSMContentListener::SetParentContentListener(nsIURIContentListener* aContentListener)
{
  mParentContentListener = aContentListener;
  return NS_OK;
}

} } // namespace mozilla::psm
