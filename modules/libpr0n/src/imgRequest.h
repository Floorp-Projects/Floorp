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

#ifndef imgRequest_h__
#define imgRequest_h__

#include "imgILoad.h"

#include "imgIContainer.h"
#include "imgIDecoder.h"
#include "imgIDecoderObserver.h"

#include "nsICacheEntryDescriptor.h"
#include "nsIChannel.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsWeakReference.h"

class imgRequestProxy;

enum {
  onStartDecode    = PR_BIT(0),
  onStartContainer = PR_BIT(1),
  onStopContainer  = PR_BIT(2),
  onStopDecode     = PR_BIT(3),
  onStopRequest    = PR_BIT(4)
};

class imgRequest : public imgILoad,
                   public imgIDecoderObserver,
                   public nsIStreamListener,
                   public nsSupportsWeakReference
{
public:
  imgRequest();
  virtual ~imgRequest();

  NS_DECL_ISUPPORTS

  nsresult Init(nsIChannel *aChannel,
                nsICacheEntryDescriptor *aCacheEntry,
                void *aCacheId);

  nsresult AddProxy   (imgRequestProxy *proxy);
  nsresult RemoveProxy(imgRequestProxy *proxy, nsresult aStatus);

  void SniffMimeType(const char *buf, PRUint32 len);

  // a request is "reusable" if it has already been loaded, or it is
  // currently being loaded on the same event queue as the new request
  // being made...
  PRBool IsReusable(void *aCacheId) { return !mLoading || (aCacheId == mCacheId); }

private:
  friend class imgRequestProxy;

  inline PRUint32 GetImageStatus() const { return mImageStatus; }
  inline nsresult GetResultFromImageStatus(PRUint32 aStatus) const;
  void Cancel(nsresult aStatus);
  nsresult GetURI(nsIURI **aURI);
  void RemoveFromCache();
  inline const char *GetMimeType() const {
    return mContentType;
  }

public:
  NS_DECL_IMGILOAD
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_IMGICONTAINEROBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

private:
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<imgIContainer> mImage;
  nsCOMPtr<imgIDecoder> mDecoder;

  nsVoidArray mObservers;

  PRBool mLoading;
  PRBool mProcessing;

  PRUint32 mImageStatus;
  PRUint32 mState;

  char *mContentType;

  nsCOMPtr<nsICacheEntryDescriptor> mCacheEntry; /* we hold on to this to this so long as we have observers */

  void *mCacheId;
};

#endif
