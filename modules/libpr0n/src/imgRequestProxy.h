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

#ifndef imgRequestProxy_h__
#define imgRequestProxy_h__

#include "imgIRequest.h"
#include "imgIDecoderObserver.h"
#include "nsISecurityInfoProvider.h"

#include "nsIRequestObserver.h"
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsISupportsPriority.h"
#include "nsITimedChannel.h"
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

class imgRequestNotifyRunnable;
class imgStatusNotifyRunnable;

namespace mozilla {
namespace imagelib {
class Image;
} // namespace imagelib
} // namespace mozilla

class imgRequestProxy : public imgIRequest, 
                        public nsISupportsPriority, 
                        public nsISecurityInfoProvider,
                        public nsITimedChannel
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIREQUEST
  NS_DECL_NSIREQUEST
  NS_DECL_NSISUPPORTSPRIORITY
  NS_DECL_NSISECURITYINFOPROVIDER
  // nsITimedChannel declared below

  imgRequestProxy();
  virtual ~imgRequestProxy();

  // Callers to Init or ChangeOwner are required to call NotifyListener after
  // (although not immediately after) doing so.
  nsresult Init(imgRequest *request, nsILoadGroup *aLoadGroup,
                mozilla::imagelib::Image* aImage,
                nsIURI* aURI, imgIDecoderObserver *aObserver);

  nsresult ChangeOwner(imgRequest *aNewOwner); // this will change mOwner.  Do not call this if the previous
                                               // owner has already sent notifications out!

  void AddToLoadGroup();
  void RemoveFromLoadGroup(bool releaseLoadGroup);

  inline bool HasObserver() const {
    return mListener != nsnull;
  }

  void SetPrincipal(nsIPrincipal *aPrincipal);

  // Asynchronously notify this proxy's listener of the current state of the
  // image, and, if we have an imgRequest mOwner, any status changes that
  // happen between the time this function is called and the time the
  // notification is scheduled.
  void NotifyListener();

  // Synchronously notify this proxy's listener of the current state of the
  // image. Only use this function if you are currently servicing an
  // asynchronously-called function.
  void SyncNotifyListener();

  // Whether we want notifications from imgStatusTracker to be deferred until
  // an event it has scheduled has been fired.
  bool NotificationsDeferred() const
  {
    return mDeferNotifications;
  }
  void SetNotificationsDeferred(bool aDeferNotifications)
  {
    mDeferNotifications = aDeferNotifications;
  }

  // Setter for our |mImage| pointer, for imgRequest to use, once it
  // instantiates an Image.
  void SetImage(mozilla::imagelib::Image* aImage);

  // Removes all animation consumers that were created with
  // IncrementAnimationConsumers. This is necessary since we need
  // to do it before the proxy itself is destroyed. See
  // imgRequest::RemoveProxy
  void ClearAnimationConsumers();

protected:
  friend class imgStatusTracker;
  friend class imgStatusNotifyRunnable;
  friend class imgRequestNotifyRunnable;

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

  // The following notification functions are protected to ensure that (friend
  // class) imgStatusTracker is the only class allowed to send us
  // notifications.

  /* non-virtual imgIDecoderObserver methods */
  void OnStartDecode   ();
  void OnStartContainer(imgIContainer *aContainer);
  void OnStartFrame    (PRUint32 aFrame);
  void OnDataAvailable (bool aCurrentFrame, const nsIntRect * aRect);
  void OnStopFrame     (PRUint32 aFrame);
  void OnStopContainer (imgIContainer *aContainer);
  void OnStopDecode    (nsresult status, const PRUnichar *statusArg); 
  void OnDiscard       ();

  /* non-virtual imgIContainerObserver methods */
  void FrameChanged(imgIContainer *aContainer,
                    const nsIntRect *aDirtyRect);

  /* non-virtual sort-of-nsIRequestObserver methods */
  void OnStartRequest();
  void OnStopRequest(bool aLastPart);

  /* Finish up canceling ourselves */
  void DoCancel(nsresult status);

  /* Do the proper refcount management to null out mListener */
  void NullOutListener();

  void DoRemoveFromLoadGroup() {
    RemoveFromLoadGroup(PR_TRUE);
  }

  // Return the imgStatusTracker associated with mOwner and/or mImage. It may
  // live either on mOwner or mImage, depending on whether
  //   (a) we have an mOwner at all
  //   (b) whether mOwner has instantiated its image yet
  imgStatusTracker& GetStatusTracker();

  nsITimedChannel* TimedChannel()
  {
    if (!mOwner)
      return nsnull;
    return mOwner->mTimedChannel;
  }

public:
  NS_FORWARD_SAFE_NSITIMEDCHANNEL(TimedChannel())

private:
  friend class imgCacheValidator;

  // We maintain the following invariant:
  // The proxy is registered at most with a single imgRequest as an observer,
  // and whenever it is, mOwner points to that object. This helps ensure that
  // imgRequestProxy::~imgRequestProxy unregisters the proxy as an observer
  // from whatever request it was registered with (if any). This, in turn,
  // means that imgRequest::mObservers will not have any stale pointers in it.
  nsRefPtr<imgRequest> mOwner;

  // The URI of our request.
  nsCOMPtr<nsIURI> mURI;

  // The image we represent. Is null until data has been received, and is then
  // set by imgRequest.
  nsRefPtr<mozilla::imagelib::Image> mImage;

  // Our principal. Is null until data has been received from the channel, and
  // is then set by imgRequest.
  nsCOMPtr<nsIPrincipal> mPrincipal;

  // mListener is only promised to be a weak ref (see imgILoader.idl),
  // but we actually keep a strong ref to it until we've seen our
  // first OnStopRequest.
  imgIDecoderObserver* mListener;
  nsCOMPtr<nsILoadGroup> mLoadGroup;

  nsLoadFlags mLoadFlags;
  PRUint32    mLockCount;
  PRUint32    mAnimationConsumers;
  bool mCanceled;
  bool mIsInLoadGroup;
  bool mListenerIsStrongRef;
  bool mDecodeRequested;

  // Whether we want to defer our notifications by the non-virtual Observer
  // interfaces as image loads proceed.
  bool mDeferNotifications;

  // We only want to send OnStartContainer once for each proxy, but we might
  // get multiple OnStartContainer calls (e.g. from multipart/x-mixed-replace).
  bool mSentStartContainer;
};

#endif // imgRequestProxy_h__
