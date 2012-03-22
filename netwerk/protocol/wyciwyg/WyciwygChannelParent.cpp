/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *  The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michal Novotny <michal.novotny@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsWyciwyg.h"

#include "mozilla/net/WyciwygChannelParent.h"
#include "nsWyciwygChannel.h"
#include "nsNetUtil.h"
#include "nsISupportsPriority.h"
#include "nsCharsetSource.h"
#include "nsISerializable.h"
#include "nsSerializationHelper.h"

namespace mozilla {
namespace net {

WyciwygChannelParent::WyciwygChannelParent()
 : mIPCClosed(false)
{
#if defined(PR_LOGGING)
  if (!gWyciwygLog)
    gWyciwygLog = PR_NewLogModule("nsWyciwygChannel");
#endif
}

WyciwygChannelParent::~WyciwygChannelParent()
{
}

void
WyciwygChannelParent::ActorDestroy(ActorDestroyReason why)
{
  // We may still have refcount>0 if the channel hasn't called OnStopRequest
  // yet, but we must not send any more msgs to child.
  mIPCClosed = true;
}

//-----------------------------------------------------------------------------
// WyciwygChannelParent::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS2(WyciwygChannelParent,
                   nsIStreamListener,
                   nsIRequestObserver);

//-----------------------------------------------------------------------------
// WyciwygChannelParent::PWyciwygChannelParent
//-----------------------------------------------------------------------------

bool
WyciwygChannelParent::RecvInit(const IPC::URI& aURI)
{
  nsresult rv;

  nsCOMPtr<nsIURI> uri(aURI);

  nsCString uriSpec;
  uri->GetSpec(uriSpec);
  LOG(("WyciwygChannelParent RecvInit [this=%x uri=%s]\n",
       this, uriSpec.get()));

  nsCOMPtr<nsIIOService> ios(do_GetIOService(&rv));
  if (NS_FAILED(rv))
    return SendCancelEarly(rv);

  nsCOMPtr<nsIChannel> chan;
  rv = NS_NewChannel(getter_AddRefs(chan), uri, ios);
  if (NS_FAILED(rv))
    return SendCancelEarly(rv);

  mChannel = do_QueryInterface(chan, &rv);
  if (NS_FAILED(rv))
    return SendCancelEarly(rv);

  return true;
}

bool
WyciwygChannelParent::RecvAsyncOpen(const IPC::URI& aOriginal,
                                    const PRUint32& aLoadFlags)
{
  nsCOMPtr<nsIURI> original(aOriginal);

  LOG(("WyciwygChannelParent RecvAsyncOpen [this=%x]\n", this));

  if (!mChannel)
    return true;

  nsresult rv;

  rv = mChannel->SetOriginalURI(original);
  if (NS_FAILED(rv))
    return SendCancelEarly(rv);

  rv = mChannel->SetLoadFlags(aLoadFlags);
  if (NS_FAILED(rv))
    return SendCancelEarly(rv);

  rv = mChannel->AsyncOpen(this, nsnull);
  if (NS_FAILED(rv))
    return SendCancelEarly(rv);

  return true;
}

bool
WyciwygChannelParent::RecvWriteToCacheEntry(const nsString& data)
{
  if (mChannel)
    mChannel->WriteToCacheEntry(data);

  return true;
}

bool
WyciwygChannelParent::RecvCloseCacheEntry(const nsresult& reason)
{
  if (mChannel)
    mChannel->CloseCacheEntry(reason);

  return true;
}

bool
WyciwygChannelParent::RecvSetCharsetAndSource(const PRInt32& aCharsetSource,
                                              const nsCString& aCharset)
{
  if (mChannel)
    mChannel->SetCharsetAndSource(aCharsetSource, aCharset);

  return true;
}

bool
WyciwygChannelParent::RecvSetSecurityInfo(const nsCString& aSecurityInfo)
{
  if (mChannel) {
    nsCOMPtr<nsISupports> securityInfo;
    NS_DeserializeObject(aSecurityInfo, getter_AddRefs(securityInfo));
    mChannel->SetSecurityInfo(securityInfo);
  }

  return true;
}

bool
WyciwygChannelParent::RecvCancel(const nsresult& aStatusCode)
{
  if (mChannel)
    mChannel->Cancel(aStatusCode);
  return true;
}

//-----------------------------------------------------------------------------
// WyciwygChannelParent::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WyciwygChannelParent::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
  LOG(("WyciwygChannelParent::OnStartRequest [this=%x]\n", this));

  nsresult rv;

  nsCOMPtr<nsIWyciwygChannel> chan = do_QueryInterface(aRequest, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsresult status;
  chan->GetStatus(&status);

  PRInt32 contentLength = -1;
  chan->GetContentLength(&contentLength);

  PRInt32 charsetSource = kCharsetUninitialized;
  nsCAutoString charset;
  chan->GetCharsetAndSource(&charsetSource, charset);

  nsCOMPtr<nsISupports> securityInfo;
  chan->GetSecurityInfo(getter_AddRefs(securityInfo));
  nsCString secInfoStr;
  if (securityInfo) {
    nsCOMPtr<nsISerializable> serializable = do_QueryInterface(securityInfo);
    if (serializable)
      NS_SerializeToString(serializable, secInfoStr);
    else {
      NS_ERROR("Can't serialize security info");
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (mIPCClosed ||
      !SendOnStartRequest(status, contentLength, charsetSource, charset, secInfoStr)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
WyciwygChannelParent::OnStopRequest(nsIRequest *aRequest,
                                    nsISupports *aContext,
                                    nsresult aStatusCode)
{
  LOG(("WyciwygChannelParent::OnStopRequest: [this=%x status=%ul]\n",
       this, aStatusCode));

  if (mIPCClosed || !SendOnStopRequest(aStatusCode)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// WyciwygChannelParent::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WyciwygChannelParent::OnDataAvailable(nsIRequest *aRequest,
                                      nsISupports *aContext,
                                      nsIInputStream *aInputStream,
                                      PRUint32 aOffset,
                                      PRUint32 aCount)
{
  LOG(("WyciwygChannelParent::OnDataAvailable [this=%x]\n", this));

  nsCString data;
  nsresult rv = NS_ReadInputStreamToString(aInputStream, data, aCount);
  if (NS_FAILED(rv))
    return rv;

  if (mIPCClosed || !SendOnDataAvailable(data, aOffset)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

}} // mozilla::net
