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
#include "nsILoadAttribs.h"

/* Forward declarations... */
class nsString;
class nsIURL;
class nsIFactory;
class nsIPostData;
class nsIContentViewer;
class nsIContentViewerContainer;
class nsIStreamListener;
class nsIStreamObserver;
class nsIDocumentLoaderObserver;

/* f43ba260-0737-11d2-beb9-00805f8a66dc */
#define NS_IDOCUMENTLOADERFACTORY_IID   \
{ 0xf43ba260, 0x0737, 0x11d2, \
  {0xbe, 0xb9, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }

/*
 * The factory API for creating a content viewer for a given
 * content-type and command.
 */
class nsIDocumentLoaderFactory : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IDOCUMENTLOADERFACTORY_IID; return iid; }

    NS_IMETHOD CreateInstance(nsIURL* aURL,
                              const char* aContentType, 
                              const char *aCommand,
                              nsIContentViewerContainer* aContainer,
                              nsISupports* aExtraInfo,
                              nsIStreamListener** aDocListenerResult,
                              nsIContentViewer** aDocViewerResult) = 0;
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
    static const nsIID& GetIID() { static nsIID iid = NS_IDOCUMENTLOADER_IID; return iid; }

    NS_IMETHOD LoadDocument(const nsString& aURLSpec, 
                            const char* aCommand,
                            nsIContentViewerContainer* aContainer,
                            nsIPostData* aPostData = nsnull,
                            nsISupports* aExtraInfo = nsnull,
                            nsIStreamObserver* anObserver = nsnull,
                            nsURLReloadType type = nsURLReload,
                            const PRUint32 aLocalIP = 0) = 0;

    NS_IMETHOD LoadSubDocument(const nsString& aURLSpec,
                               nsISupports* aExtraInfo = nsnull,
                               nsURLReloadType type = nsURLReload,
                               const PRUint32 aLocalIP = 0) = 0;

    NS_IMETHOD Stop(void) = 0;

    NS_IMETHOD IsBusy(PRBool& aResult) = 0;

    NS_IMETHOD CreateDocumentLoader(nsIDocumentLoader** anInstance) = 0;

    NS_IMETHOD SetDocumentFactory(nsIDocumentLoaderFactory* aFactory) = 0;

		NS_IMETHOD GetDocumentFactory(nsIDocumentLoaderFactory** aResult) = 0;

    NS_IMETHOD AddObserver(nsIDocumentLoaderObserver *aObserver) = 0;

    NS_IMETHOD RemoveObserver(nsIDocumentLoaderObserver *aObserver) = 0;

    NS_IMETHOD SetContainer(nsIContentViewerContainer* aContainer) = 0;

    NS_IMETHOD GetContainer(nsIContentViewerContainer** aResult) = 0;
};

/* 057b04d0-0ccf-11d2-beba-00805f8a66dc */
#define NS_DOCUMENTLOADER_SERVICE_CID   \
{ 0x057b04d0, 0x0ccf, 0x11d2, \
  {0xbe, 0xba, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }

#endif /* nsIDocumentLoader_h___ */
