/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Boris Zbarsky <bzbarsky@mit.edu>  (original author)
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

#include "nsUnicharStreamLoader.h"
#include "nsIPipe.h"
#include "nsIChannel.h"
#include "nsNetUtil.h"
#include "nsProxiedService.h"
#include "nsIChannel.h"
#include "nsIUnicharInputStream.h"
#include "nsIConverterInputStream.h"
#include "nsIPipe.h"

#ifdef DEBUG // needed for IsASCII assertion
#include "nsReadableUtils.h"
#endif // DEBUG

static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);

NS_IMETHODIMP
nsUnicharStreamLoader::Init(nsIChannel *aChannel,
                            nsIUnicharStreamLoaderObserver *aObserver,
                            nsISupports *aContext,
                            PRUint32 aSegmentSize)
{
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aObserver);

  if (aSegmentSize <= 0) {
    aSegmentSize = nsIUnicharStreamLoader::DEFAULT_SEGMENT_SIZE;
  }
  
  nsresult rv = aChannel->AsyncOpen(this, aContext);

  if (NS_FAILED(rv)) {
    // don't callback synchronously as it puts the caller
    // in a recursive situation and breaks the asynchronous
    // semantics of nsIStreamLoader
    nsresult rv2 = NS_OK;
    nsCOMPtr<nsIProxyObjectManager> pIProxyObjectManager = 
      do_GetService(kProxyObjectManagerCID, &rv2);
    if (NS_FAILED(rv2))
      return rv2;

    nsCOMPtr<nsIUnicharStreamLoaderObserver> pObserver;
    rv2 =
      pIProxyObjectManager->GetProxyForObject(NS_CURRENT_EVENTQ, 
                                              NS_GET_IID(nsIUnicharStreamLoaderObserver),
                                              aObserver, 
                                              PROXY_ASYNC | PROXY_ALWAYS,
                                              getter_AddRefs(pObserver));
    if (NS_FAILED(rv2))
      return rv2;

    rv = pObserver->OnStreamComplete(this, aContext, rv, nsnull);
  }

  mObserver = aObserver;
  mContext = aContext;
  mCharset.Truncate();
  mChannel = nsnull; // Leave this null till OnStopRequest
  mSegmentSize = aSegmentSize;
  return rv;
}

NS_METHOD
nsUnicharStreamLoader::Create(nsISupports *aOuter,
                              REFNSIID aIID,
                              void **aResult)
{
  if (aOuter) return NS_ERROR_NO_AGGREGATION;

  nsUnicharStreamLoader* it = new nsUnicharStreamLoader();
  if (it == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(it);
  nsresult rv = it->QueryInterface(aIID, aResult);
  NS_RELEASE(it);
  return rv;
}

NS_IMPL_ISUPPORTS3(nsUnicharStreamLoader, nsIUnicharStreamLoader,
                   nsIRequestObserver, nsIStreamListener)

/* readonly attribute nsIChannel channel; */
NS_IMETHODIMP 
nsUnicharStreamLoader::GetChannel(nsIChannel **aChannel)
{
  NS_IF_ADDREF(*aChannel = mChannel);
  return NS_OK;
}

/* readonly attribute nsACString charset */
NS_IMETHODIMP
nsUnicharStreamLoader::GetCharset(nsACString& aCharset)
{
  aCharset = mCharset;
  return NS_OK;
}

/* nsIRequestObserver implementation */
NS_IMETHODIMP
nsUnicharStreamLoader::OnStartRequest(nsIRequest* request,
                                      nsISupports *ctxt)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsUnicharStreamLoader::OnStopRequest(nsIRequest *request,
                                     nsISupports *ctxt,
                                     nsresult aStatus)
{
  // if we trigger this assertion, then it means that the channel called
  // OnStopRequest before returning from AsyncOpen, which is totally
  // unexpected behavior.
  if (!mObserver) {
    NS_ERROR("No way we should not have an mObserver here!");
    return NS_ERROR_UNEXPECTED;
  }

  if (mInputStream) {
    nsresult rv;
    // We got some data at some point.  I guess we should tell our
    // observer about it or something....

    // Make sure mChannel points to the channel that we ended up with
    mChannel = do_QueryInterface(request);

    // Determine the charset
    PRUint32 readCount = 0;
    rv = mInputStream->ReadSegments(WriteSegmentFun,
                                    this,
                                    mSegmentSize, 
                                    &readCount);
    if (NS_FAILED(rv)) {
      rv = mObserver->OnStreamComplete(this, mContext, rv, nsnull);
      goto cleanup;
    }

    nsCOMPtr<nsIConverterInputStream> uin =
      do_CreateInstance("@mozilla.org/intl/converter-input-stream;1",
                        &rv);
    if (NS_FAILED(rv)) {
      rv = mObserver->OnStreamComplete(this, mContext, rv, nsnull);
      goto cleanup;
    }

    rv = uin->Init(mInputStream,
                   mCharset.get(),
                   mSegmentSize,
                   PR_TRUE);
    
    if (NS_FAILED(rv)) {
      rv = mObserver->OnStreamComplete(this, mContext, rv, nsnull);
      goto cleanup;
    }
    
    mObserver->OnStreamComplete(this, mContext, aStatus, uin);

  } else {
    // We never got any data, so just tell our observer that we are
    // done and give them no stream
    mObserver->OnStreamComplete(this, mContext, aStatus, nsnull);
  }
  
  // Clean up.
 cleanup:
  mObserver = nsnull;
  mChannel = nsnull;
  mContext = nsnull;
  mInputStream = nsnull;
  mOutputStream = nsnull;
  return NS_OK;
}

/* nsIStreamListener implementation */
NS_METHOD
nsUnicharStreamLoader::WriteSegmentFun(nsIInputStream *aInputStream,
                                       void *aClosure,
                                       const char *aSegment,
                                       PRUint32 aToOffset,
                                       PRUint32 aCount,
                                       PRUint32 *aWriteCount)
{
  nsUnicharStreamLoader *self = (nsUnicharStreamLoader *) aClosure;
  if (self->mCharset.IsEmpty()) {
    // First time through.  Call our observer.
    NS_ASSERTION(self->mObserver, "This should never be possible");

    nsresult rv = self->mObserver->OnDetermineCharset(self,
                                                      self->mContext,
                                                      aSegment,
                                                      aCount,
                                                      self->mCharset);
    
    if (NS_FAILED(rv) || self->mCharset.IsEmpty()) {
      // The observer told us nothing useful
      self->mCharset.AssignLiteral("ISO-8859-1");
    }

    NS_ASSERTION(IsASCII(self->mCharset),
                 "Why is the charset name non-ascii?  Whose bright idea was that?");
  }
  // Don't consume any data
  *aWriteCount = 0;
  return NS_BASE_STREAM_WOULD_BLOCK;
}


NS_IMETHODIMP
nsUnicharStreamLoader::OnDataAvailable(nsIRequest *aRequest,
                                       nsISupports *aContext,
                                       nsIInputStream *aInputStream, 
                                       PRUint32 aSourceOffset,
                                       PRUint32 aCount)
{
  nsresult rv = NS_OK;
  if (!mInputStream) {
    // We are not initialized.  Time to set things up.
    NS_ASSERTION(!mOutputStream, "Why are we sorta-initialized?");
    rv = NS_NewPipe(getter_AddRefs(mInputStream),
                    getter_AddRefs(mOutputStream),
                    mSegmentSize,
                    PRUint32(-1),  // give me all the data you can!
                    PR_TRUE,  // non-blocking input
                    PR_TRUE); // non-blocking output
    if (NS_FAILED(rv))
      return rv;
  }

  PRUint32 writeCount = 0;
  do {
    rv = mOutputStream->WriteFrom(aInputStream, aCount, &writeCount);
    if (NS_FAILED(rv)) return rv;
    aCount -= writeCount;
  } while (aCount > 0);

  return NS_OK;
}
