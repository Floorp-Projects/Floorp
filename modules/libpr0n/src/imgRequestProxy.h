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

#include "imgIRequest.h"
#include "imgIDecoderObserver.h"
#include "nsISecurityInfoProvider.h"

#include "imgIContainer.h"
#include "imgIDecoder.h"
#include "nsIRequestObserver.h"
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsISupportsPriority.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"

#include "imgRequest.h"

#define NS_IMGREQUESTPROXY_CID \
{ /* 20557898-1dd2-11b2-8f65-9c462ee2bc95 */         \
     0x20557898,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x8f, 0x65, 0x9c, 0x46, 0x2e, 0xe2, 0xbc, 0x95} \
}

class imgRequestProxy : public imgIRequest, public nsISupportsPriority, public nsISecurityInfoProvider
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIREQUEST
  NS_DECL_NSIREQUEST
  NS_DECL_NSISUPPORTSPRIORITY
  NS_DECL_NSISECURITYINFOPROVIDER

  imgRequestProxy();
  virtual ~imgRequestProxy();

  // Callers to Init or ChangeOwner are required to call
  // NotifyProxyListener on the request after (although not immediately
  // after) doing so.
  nsresult Init(imgRequest *request, nsILoadGroup *aLoadGroup, imgIDecoderObserver *aObserver);
  nsresult ChangeOwner(imgRequest *aNewOwner); // this will change mOwner.  Do not call this if the previous
                                               // owner has already sent notifications out!

  void AddToLoadGroup();
  void RemoveFromLoadGroup(PRBool releaseLoadGroup);

protected:
  friend class imgRequest;

  class imgCancelRunnable;
  friend class imgCancelRunnable;

  class imgCancelRunnable : public nsRunnable
  {
    public:
      imgCancelRunnable(imgRequestProxy* owner, nsresult status)
        : mOwner(owner), mStatus(status)
      {}

      NS_IMETHOD Run() {
        mOwner->DoCancel(mStatus);
        return NS_OK;
      }

    private:
      nsRefPtr<imgRequestProxy> mOwner;
      nsresult mStatus;
  };



  /* non-virtual imgIDecoderObserver methods */
  void OnStartDecode   ();
  void OnStartContainer(imgIContainer *aContainer);
  void OnStartFrame    (gfxIImageFrame *aFrame);
  void OnDataAvailable (gfxIImageFrame *aFrame, const nsIntRect * aRect);
  void OnStopFrame     (gfxIImageFrame *aFrame);
  void OnStopContainer (imgIContainer *aContainer);
  void OnStopDecode    (nsresult status, const PRUnichar *statusArg); 

  /* non-virtual imgIContainerObserver methods */
  void FrameChanged(imgIContainer *aContainer, gfxIImageFrame *aFrame, nsIntRect * aDirtyRect);

  /* non-virtual nsIRequestObserver (plus some) methods */
  void OnStartRequest(nsIRequest *request, nsISupports *ctxt);
  void OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult statusCode, PRBool aLastPart); 

  inline PRBool HasObserver() const {
    return mListener != nsnull;
  }

  /* Finish up canceling ourselves */
  void DoCancel(nsresult status);

  /* Do the proper refcount management to null out mListener */
  void NullOutListener();
  
private:
  friend class imgCacheValidator;

  // We maintain the following invariant:
  // The proxy is registered at most with a single imgRequest as an observer,
  // and whenever it is, mOwner points to that object. This helps ensure that
  // imgRequestProxy::~imgRequestProxy unregisters the proxy as an observer
  // from whatever request it was registered with (if any). This, in turn,
  // means that imgRequest::mObservers will not have any stale pointers in it.
  nsRefPtr<imgRequest> mOwner;

  // mListener is only promised to be a weak ref (see imgILoader.idl),
  // but we actually keep a strong ref to it until we've seen our
  // first OnStopRequest.
  imgIDecoderObserver* mListener;
  nsCOMPtr<nsILoadGroup> mLoadGroup;

  nsLoadFlags mLoadFlags;
  PRPackedBool mCanceled;
  PRPackedBool mIsInLoadGroup;
  PRPackedBool mListenerIsStrongRef;
};
