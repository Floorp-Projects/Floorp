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

#include "imgRequest.h"
#include "imgIDecoderObserver.h"

#include "imgIContainer.h"
#include "imgIDecoder.h"
#include "nsIRequestObserver.h"
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsCOMPtr.h"

#define NS_IMGREQUESTPROXY_CID \
{ /* 20557898-1dd2-11b2-8f65-9c462ee2bc95 */         \
     0x20557898,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x8f, 0x65, 0x9c, 0x46, 0x2e, 0xe2, 0xbc, 0x95} \
}

class imgRequestProxy : public imgIRequest,
                        public imgIDecoderObserver,
                        public nsIRequestObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIREQUEST
  NS_DECL_NSIREQUEST
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_IMGICONTAINEROBSERVER
  NS_DECL_NSIREQUESTOBSERVER

  imgRequestProxy();
  virtual ~imgRequestProxy();

  /* additional members */
  nsresult Init(imgRequest *request, nsILoadGroup *aLoadGroup, imgIDecoderObserver *aObserver, nsISupports *cx);

private:
  nsCOMPtr<imgIDecoderObserver> mObserver;

  nsCOMPtr<nsISupports> mContext;

  nsCOMPtr<imgIRequest> mOwner;

  nsCOMPtr<nsIChannel> mDummyChannel;

  PRBool mCanceled;
};
