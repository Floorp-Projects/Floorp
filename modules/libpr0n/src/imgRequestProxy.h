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

#include "imgIContainer.h"
#include "imgIDecoder.h"
#include "nsIRequestObserver.h"
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsCOMPtr.h"

#include "imgRequest.h"

#include "prlock.h"

#define NS_IMGREQUESTPROXY_CID \
{ /* 20557898-1dd2-11b2-8f65-9c462ee2bc95 */         \
     0x20557898,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x8f, 0x65, 0x9c, 0x46, 0x2e, 0xe2, 0xbc, 0x95} \
}

class imgRequestProxy : public imgIRequest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIREQUEST
  NS_DECL_NSIREQUEST

  imgRequestProxy();
  virtual ~imgRequestProxy();

  /* additional members */
  nsresult Init(imgRequest *request, nsILoadGroup *aLoadGroup, imgIDecoderObserver *aObserver);
  nsresult ChangeOwner(imgRequest *aNewOwner); // this will change mOwner.  Do not call this if the previous
                                               // owner has already sent notifications out!

  void AddToLoadGroup();
  void RemoveFromLoadGroup();

protected:
  friend class imgRequest;

  /* non-virtual imgIDecoderObserver methods */
  void OnStartDecode   (void);
  void OnStartContainer(imgIContainer *aContainer);
  void OnStartFrame    (gfxIImageFrame *aFrame);
  void OnDataAvailable (gfxIImageFrame *aFrame, const nsRect * aRect);
  void OnStopFrame     (gfxIImageFrame *aFrame);
  void OnStopContainer (imgIContainer *aContainer);
  void OnStopDecode    (nsresult status, const PRUnichar *statusArg); 

  /* non-virtual imgIContainerObserver methods */
  void FrameChanged(imgIContainer *aContainer, gfxIImageFrame *aFrame, nsRect * aDirtyRect);

  /* non-virtual nsIRequestObserver methods */
  void OnStartRequest(nsIRequest *request, nsISupports *ctxt);
  void OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult statusCode); 

  inline PRBool HasObserver() const {
    return mListener != nsnull;
  }
  
private:
  friend class imgCacheValidator;

  imgRequest *mOwner;

  imgIDecoderObserver* mListener;  // Weak ref; see imgILoader.idl
  nsCOMPtr<nsILoadGroup> mLoadGroup;

  nsLoadFlags mLoadFlags;
  PRPackedBool mCanceled;
  PRPackedBool mIsInLoadGroup;

  PRLock *mLock;
};
