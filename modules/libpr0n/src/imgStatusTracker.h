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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Drew <joe@drew.ca> (original author)
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

#ifndef imgStatusTracker_h__
#define imgStatusTracker_h__

class nsIntRect;
class imgIContainer;
class imgRequest;
class imgRequestProxy;

#include "prtypes.h"
#include "nscore.h"

enum {
  stateRequestStarted    = PR_BIT(0),
  stateHasSize           = PR_BIT(1),
  stateDecodeStarted     = PR_BIT(2),
  stateDecodeStopped     = PR_BIT(3),
  stateFrameStopped      = PR_BIT(4),
  stateRequestStopped    = PR_BIT(5)
};

/*
 * The image status tracker is a class that encapsulates all the loading and
 * decoding status about an image (imgContainer), and makes it possible to send
 * notifications to imgRequestProxys. When a new proxy needs to be notified of
 * the current state of an image, simply call the Notify() method on this class
 * with the relevant proxy as its argument.
 */

class imgStatusTracker
{
public:
  // aImage is the image that this status tracker will pass to the
  // imgRequestProxys in Notify() and EmulateRequestFinished(), and must be
  // alive as long as this instance is, because we hold a weak reference to it.
  imgStatusTracker(imgIContainer* aImage);

  // "Replay" all of the notifications that would have to happen to put us in
  // the state we're currently in.
  void Notify(imgRequestProxy* proxy);

  // Send all notifications that would be necessary to make |proxy| believe the
  // request is finished downloading and decoding.
  // If aOnlySendStopRequest is true, we will only send OnStopRequest, and then
  // only if that is necessary.
  void EmulateRequestFinished(imgRequestProxy* proxy, nsresult aStatus,
                              PRBool aOnlySendStopRequest);

  // Returns whether we are in the process of loading; that is, whether we have
  // not received OnStopRequest.
  PRBool IsLoading() const;

  // Get the current image status (as in imgIRequest).
  PRUint32 GetImageStatus() const;

  // Following are all the notification methods. You must call the Record
  // variant on this status tracker, then call the Send variant for each proxy
  // you want to notify.

  // Call when the request is being cancelled.
  void RecordCancel();

  // Shorthand for recording all the load notifications: StartRequest,
  // StartContainer, StopRequest.
  void RecordLoaded();

  // Shorthand for recording all the decode notifications: StartDecode,
  // StartFrame, DataAvailable, StopFrame, StopContainer, StopDecode.
  void RecordDecoded();

  /* non-virtual imgIDecoderObserver methods */
  void RecordStartDecode();
  void SendStartDecode(imgRequestProxy* aProxy);
  void RecordStartContainer(imgIContainer* aContainer);
  void SendStartContainer(imgRequestProxy* aProxy, imgIContainer* aContainer);
  void RecordStartFrame(PRUint32 aFrame);
  void SendStartFrame(imgRequestProxy* aProxy, PRUint32 aFrame);
  void RecordDataAvailable(PRBool aCurrentFrame, const nsIntRect* aRect);
  void SendDataAvailable(imgRequestProxy* aProxy, PRBool aCurrentFrame, const nsIntRect* aRect);
  void RecordStopFrame(PRUint32 aFrame);
  void SendStopFrame(imgRequestProxy* aProxy, PRUint32 aFrame);
  void RecordStopContainer(imgIContainer* aContainer);
  void SendStopContainer(imgRequestProxy* aProxy, imgIContainer* aContainer);
  void RecordStopDecode(nsresult status, const PRUnichar* statusArg);
  void SendStopDecode(imgRequestProxy* aProxy, nsresult aStatus, const PRUnichar* statusArg);
  void RecordDiscard();
  void SendDiscard(imgRequestProxy* aProxy);

  /* non-virtual imgIContainerObserver methods */
  void RecordFrameChanged(imgIContainer* aContainer, nsIntRect* aDirtyRect);
  void SendFrameChanged(imgRequestProxy* aProxy, imgIContainer* aContainer, nsIntRect* aDirtyRect);

  /* non-virtual sort-of-nsIRequestObserver methods */
  void RecordStartRequest();
  void SendStartRequest(imgRequestProxy* aProxy);
  void RecordStopRequest(PRBool aLastPart, nsresult aStatus);
  void SendStopRequest(imgRequestProxy* aProxy, PRBool aLastPart, nsresult aStatus);

private:
  // A weak pointer to the imgIContainer, because the container owns us, and we
  // can't create a cycle.
  imgIContainer* mImage;
  PRUint32 mState;
  nsresult mImageStatus;
  PRPackedBool mHadLastPart;
};

#endif
