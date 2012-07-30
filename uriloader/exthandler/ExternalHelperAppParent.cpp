/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExternalHelperAppParent.h"
#include "nsIContent.h"
#include "nsCExternalHandlerService.h"
#include "nsIExternalHelperAppService.h"
#include "mozilla/dom/ContentParent.h"
#include "nsIBrowserDOMWindow.h"
#include "nsStringStream.h"

#include "mozilla/unused.h"
#include "mozilla/Util.h" // for DebugOnly

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_INHERITED4(ExternalHelperAppParent,
                             nsHashPropertyBag,
                             nsIRequest,
                             nsIChannel,
                             nsIMultiPartChannel,
                             nsIResumableChannel)

ExternalHelperAppParent::ExternalHelperAppParent(
    const IPC::URI& uri,
    const PRInt64& aContentLength)
  : mURI(uri)
  , mPending(false)
  , mLoadFlags(0)
  , mStatus(NS_OK)
  , mContentLength(aContentLength)
{
}

void
ExternalHelperAppParent::Init(ContentParent *parent,
                              const nsCString& aMimeContentType,
                              const nsCString& aContentDispositionHeader,
                              const bool& aForceSave,
                              const IPC::URI& aReferrer)
{
  nsHashPropertyBag::Init();

  nsCOMPtr<nsIExternalHelperAppService> helperAppService =
    do_GetService(NS_EXTERNALHELPERAPPSERVICE_CONTRACTID);
  NS_ASSERTION(helperAppService, "No Helper App Service!");

  SetPropertyAsInt64(NS_CHANNEL_PROP_CONTENT_LENGTH, mContentLength);
  if (aReferrer)
    SetPropertyAsInterface(NS_LITERAL_STRING("docshell.internalReferrer"), aReferrer);
  mContentDispositionHeader = aContentDispositionHeader;
  NS_GetFilenameFromDisposition(mContentDispositionFilename, mContentDispositionHeader, mURI);
  mContentDisposition = NS_GetContentDispositionFromHeader(mContentDispositionHeader, this);
  helperAppService->DoContent(aMimeContentType, this, nullptr,
                              aForceSave, getter_AddRefs(mListener));
}

bool
ExternalHelperAppParent::RecvOnStartRequest(const nsCString& entityID)
{
  mEntityID = entityID;
  mPending = true;
  mStatus = mListener->OnStartRequest(this, nullptr);
  return true;
}

bool
ExternalHelperAppParent::RecvOnDataAvailable(const nsCString& data,
                                             const PRUint32& offset,
                                             const PRUint32& count)
{
  if (NS_FAILED(mStatus))
    return true;

  NS_ASSERTION(mPending, "must be pending!");
  nsCOMPtr<nsIInputStream> stringStream;
  DebugOnly<nsresult> rv = NS_NewByteInputStream(getter_AddRefs(stringStream), data.get(), count, NS_ASSIGNMENT_DEPEND);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to create dependent string!");
  mStatus = mListener->OnDataAvailable(this, nullptr, stringStream, offset, count);

  return true;
}

bool
ExternalHelperAppParent::RecvOnStopRequest(const nsresult& code)
{
  mPending = false;
  mListener->OnStopRequest(this, nullptr,
                           (NS_SUCCEEDED(code) && NS_FAILED(mStatus)) ? mStatus : code);
  unused << Send__delete__(this);
  return true;
}

ExternalHelperAppParent::~ExternalHelperAppParent()
{
}

//
// nsIRequest implementation...
//

NS_IMETHODIMP
ExternalHelperAppParent::GetName(nsACString& aResult)
{
  mURI->GetAsciiSpec(aResult);
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::IsPending(bool *aResult)
{
  *aResult = mPending;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetStatus(nsresult *aResult)
{
  *aResult = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::Cancel(nsresult aStatus)
{
  mStatus = aStatus;
  unused << SendCancel(aStatus);
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::Suspend()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::Resume()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//
// nsIChannel implementation
//

NS_IMETHODIMP
ExternalHelperAppParent::GetOriginalURI(nsIURI * *aURI)
{
  NS_IF_ADDREF(*aURI = mURI);
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetOriginalURI(nsIURI *aURI)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetURI(nsIURI **aURI)
{
  NS_IF_ADDREF(*aURI = mURI);
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::Open(nsIInputStream **aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::AsyncOpen(nsIStreamListener *aListener,
                                   nsISupports *aContext)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
ExternalHelperAppParent::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetLoadFlags(nsLoadFlags aLoadFlags)
{
  mLoadFlags = aLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
  *aLoadGroup = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetOwner(nsISupports* *aOwner)
{
  *aOwner = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetOwner(nsISupports* aOwner)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetNotificationCallbacks(nsIInterfaceRequestor* *aCallbacks)
{
  *aCallbacks = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetNotificationCallbacks(nsIInterfaceRequestor* aCallbacks)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  *aSecurityInfo = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetContentType(nsACString& aContentType)
{
  aContentType.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetContentType(const nsACString& aContentType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetContentCharset(nsACString& aContentCharset)
{
  aContentCharset.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetContentCharset(const nsACString& aContentCharset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetContentDisposition(PRUint32 *aContentDisposition)
{
  if (mContentDispositionHeader.IsEmpty())
    return NS_ERROR_NOT_AVAILABLE;

  *aContentDisposition = mContentDisposition;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetContentDispositionFilename(nsAString& aContentDispositionFilename)
{
  if (mContentDispositionFilename.IsEmpty())
    return NS_ERROR_NOT_AVAILABLE;

  aContentDispositionFilename = mContentDispositionFilename;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetContentDispositionHeader(nsACString& aContentDispositionHeader)
{
  if (mContentDispositionHeader.IsEmpty())
    return NS_ERROR_NOT_AVAILABLE;

  aContentDispositionHeader = mContentDispositionHeader;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetContentLength(PRInt32 *aContentLength)
{
  if (mContentLength > PR_INT32_MAX || mContentLength < 0)
    *aContentLength = -1;
  else
    *aContentLength = (PRInt32)mContentLength;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetContentLength(PRInt32 aContentLength)
{
  mContentLength = aContentLength;
  return NS_OK;
}

//
// nsIResumableChannel implementation
//

NS_IMETHODIMP
ExternalHelperAppParent::ResumeAt(PRUint64 startPos, const nsACString& entityID)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetEntityID(nsACString& aEntityID)
{
  aEntityID = mEntityID;
  return NS_OK;
}

//
// nsIMultiPartChannel implementation
//

NS_IMETHODIMP
ExternalHelperAppParent::GetBaseChannel(nsIChannel* *aChannel)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetPartID(PRUint32* aPartID)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetIsLastPart(bool* aIsLastPart)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

} // namespace dom
} // namespace mozilla
