/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
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

class imgCacheValidator;

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
                void *aCacheId,
                void *aLoadId);

  nsresult AddProxy   (imgRequestProxy *proxy, PRBool aNotify);
  nsresult RemoveProxy(imgRequestProxy *proxy, nsresult aStatus, PRBool aNotify);
  nsresult NotifyProxyListener(imgRequestProxy *proxy);

  void SniffMimeType(const char *buf, PRUint32 len);

  // a request is "reusable" if it has already been loaded, or it is
  // currently being loaded on the same event queue as the new request
  // being made...
  PRBool IsReusable(void *aCacheId) { return !mLoading || (aCacheId == mCacheId); }

private:
  friend class imgRequestProxy;
  friend class imgLoader;
  friend class imgCacheValidator;

  inline void SetLoadId(void *aLoadId) {
    mLoadId = aLoadId;
    mLoadTime = PR_Now();
  }
  inline PRUint32 GetImageStatus() const { return mImageStatus; }
  inline nsresult GetResultFromImageStatus(PRUint32 aStatus) const;
  void Cancel(nsresult aStatus);
  nsresult GetURI(nsIURI **aURI);
  void RemoveFromCache();
  inline const char *GetMimeType() const {
    return mContentType.get();
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

  PRPackedBool mLoading;
  PRPackedBool mProcessing;

  PRUint32 mImageStatus;
  PRUint32 mState;

  nsCString mContentType;

  nsCOMPtr<nsICacheEntryDescriptor> mCacheEntry; /* we hold on to this to this so long as we have observers */

  void *mCacheId;

  void *mLoadId;
  PRTime mLoadTime;

  imgCacheValidator *mValidator;
  PRBool   mIsMultiPartChannel;
};

#endif
