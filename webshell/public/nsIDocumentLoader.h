/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsIDocumentLoader_h___
#define nsIDocumentLoader_h___

#include "nsweb.h"
#include "prtypes.h"
#include "nsISupports.h"


/* Forward declarations... */
class nsString;
class nsIURL;
class nsIFactory;
class nsIPostData;
class nsIDocumentWidget;
class nsIStreamListener;
class nsIStreamObserver;


/* f43ba260-0737-11d2-beb9-00805f8a66dc */
#define NS_IDOCUMENTLOADERFACTORY_IID   \
{ 0xf43ba260, 0x0737, 0x11d2, \
  {0xbe, 0xb9, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }

/*
 *
 *
 */
class nsIDocumentLoaderFactory : public nsISupports
{
public:
    NS_IMETHOD CreateInstance(nsIURL* aURL,
                              const char* aContentType, 
                              const char *aCommand,
                              nsIStreamListener** aDocListener,
                              nsIDocumentWidget** aDocViewer) = 0;
};



/* b0e73260-0739-11d2-beb9-00805f8a66dc */
#define NS_IVIEWERCONTAINER_IID   \
{ 0xb0e73260, 0x0739, 0x11d2, \
  {0xbe, 0xb9, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }

/*
 *
 *
 */
class nsIViewerContainer : public nsISupports
{
public:

    NS_IMETHOD Embed(nsIDocumentWidget* aDocViewer, 
                     const char* aCommand,
                     nsISupports* aExtraInfo) = 0;
};


/* b9d685e0-fcae-11d1-beb9-00805f8a66dc */
#define NS_IDOCUMENTLOADER_IID   \
{ 0xb9d685e0, 0xfcae, 0x11d1, \
  {0xbe, 0xb9, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }

/*
 *
 *
 */
class nsIDocumentLoader : public nsISupports 
{
public:
    NS_IMETHOD SetDocumentFactory(nsIDocumentLoaderFactory* aFactory) = 0;

    NS_IMETHOD LoadURL(const nsString& aURLSpec, 
                       const char* aCommand,
                       nsIViewerContainer* aContainer,
                       nsIPostData* aPostData = nsnull,
                       nsISupports* aExtraInfo = nsnull,
                       nsIStreamObserver* anObserver = nsnull) = 0;
};




/* 057b04d0-0ccf-11d2-beba-00805f8a66dc */
#define NS_DOCUMENTLOADER_CID   \
{ 0x057b04d0, 0x0ccf, 0x11d2, \
  {0xbe, 0xba, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }

extern "C" NS_WEB nsresult NS_NewDocumentLoaderFactory(nsIFactory** aFactory);

#endif /* nsIDocumentLoader_h___ */

