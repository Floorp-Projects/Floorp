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

#include "imgILoader.h"

#ifdef LOADER_THREADSAFE
#include "prlock.h"
#endif

class imgRequest;
class imgIRequest;
class imgIDecoderObserver;
class nsILoadGroup;

#define NS_IMGLOADER_CID \
{ /* 9f6a0d2e-1dd1-11b2-a5b8-951f13c846f7 */         \
     0x9f6a0d2e,                                     \
     0x1dd1,                                         \
     0x11b2,                                         \
    {0xa5, 0xb8, 0x95, 0x1f, 0x13, 0xc8, 0x46, 0xf7} \
}

class imgLoader : public imgILoader
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGILOADER

  imgLoader();
  virtual ~imgLoader();

private:
  nsresult CreateNewProxyForRequest(imgRequest *aRequest, nsILoadGroup *aLoadGroup,
                                    imgIDecoderObserver *aObserver, nsISupports *cx,
                                    nsLoadFlags aLoadFlags, imgIRequest *aRequestProxy,
                                    imgIRequest **_retval);
};



/**
 * proxy stream listener class used to handle multipart/x-mixed-replace
 */

#include "nsCOMPtr.h"
#include "nsIStreamListener.h"

class ProxyListener : public nsIStreamListener
{
public:
  ProxyListener(nsIStreamListener *dest);
  virtual ~ProxyListener();

  /* additional members */
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

private:
  nsCOMPtr<nsIStreamListener> mDestListener;
};
