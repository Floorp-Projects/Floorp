/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsImageRequest.h"

#include "nsXPIDLString.h"

#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsIImageLoader.h"
#include "nsIComponentManager.h"

#include "nsIComponentManager.h"

#include "nsString.h"

#include "nspr.h"

NS_IMPL_ISUPPORTS4(nsImageRequest, nsIImageRequest, nsIRequest, nsIStreamListener, nsIStreamObserver)

nsImageRequest::nsImageRequest()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsImageRequest::~nsImageRequest()
{
  /* destructor code */
}

/* void init (in nsIChannel aChannel, in gfx_dimension width, in gfx_dimension height); */
NS_IMETHODIMP nsImageRequest::Init(nsIChannel *aChannel)
{
  if (mImage)
    return NS_ERROR_FAILURE; // XXX

  mChannel = aChannel;

  // XXX do not init the image here.  this has to be done from the image decoder.
  mImage = do_CreateInstance("@mozilla.org/gfx/image;2");

  return aChannel->AsyncRead(NS_STATIC_CAST(nsIStreamListener*, this), nsnull);
}

/* readonly attribute nsIImage image; */
NS_IMETHODIMP nsImageRequest::GetImage(nsIImageContainer * *aImage)
{
  *aImage = mImage;
  NS_IF_ADDREF(*aImage);
  return NS_OK;
}





/** nsIRequest methods **/

/* readonly attribute wstring name; */
NS_IMETHODIMP nsImageRequest::GetName(PRUnichar * *aName)
{
  return mChannel->GetName(aName);
}

/* boolean isPending (); */
NS_IMETHODIMP nsImageRequest::IsPending(PRBool *_retval)
{
  return mChannel->IsPending(_retval);
}

/* readonly attribute nsresult status; */
NS_IMETHODIMP nsImageRequest::GetStatus(nsresult *aStatus)
{
  return mChannel->GetStatus(aStatus);
}

/* void cancel (in nsresult status); */
NS_IMETHODIMP nsImageRequest::Cancel(nsresult status)
{
  return mChannel->Cancel(status);
}

/* void suspend (); */
NS_IMETHODIMP nsImageRequest::Suspend()
{
  return mChannel->Suspend();
}

/* void resume (); */
NS_IMETHODIMP nsImageRequest::Resume()
{
  return mChannel->Resume();
}






/** nsIStreamObserver methods **/

/* void onStartRequest (in nsIChannel channel, in nsISupports ctxt); */
NS_IMETHODIMP nsImageRequest::OnStartRequest(nsIChannel *channel, nsISupports *ctxt)
{
  nsXPIDLCString contentType;
  channel->GetContentType(getter_Copies(contentType));
  printf("content type is %s\n", contentType.get());

  nsCAutoString conid("@mozilla.org/image/decoder?");
  conid += contentType.get();
  conid += ";1";

  mDecoder = do_CreateInstance(conid);

  if (!mDecoder) {
    // no image decoder for this mimetype :(
    channel->Cancel(NS_BINDING_ABORTED);

    // XXX notify the person that owns us now that wants the nsIImageContainer off of us?

    return NS_ERROR_FAILURE;
  }

  mDecoder->Init(NS_STATIC_CAST(nsIImageRequest*, this));
  return NS_OK;
}

/* void onStopRequest (in nsIChannel channel, in nsISupports ctxt, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP nsImageRequest::OnStopRequest(nsIChannel *channel, nsISupports *ctxt, nsresult status, const PRUnichar *statusArg)
{
  if (!mDecoder) return NS_ERROR_FAILURE;

  return mDecoder->Close();
}




/** nsIStreamListener methods **/

/* void onDataAvailable (in nsIChannel channel, in nsISupports ctxt, in nsIInputStream inStr, in unsigned long sourceOffset, in unsigned long count); */
NS_IMETHODIMP nsImageRequest::OnDataAvailable(nsIChannel *channel, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  if (!mDecoder) return NS_ERROR_FAILURE;

  PRUint32 wrote;
  return mDecoder->WriteFrom(inStr, count, &wrote);
}






