/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include <stdio.h>
#include <assert.h>

#ifdef XP_PC
#include <windows.h>
#endif

#include "plstr.h"

#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsINetService.h"

#include "nsString.h"

int urlLoaded;
PRBool bTraceEnabled;

#include "nsIPostToServer.h"

NS_DEFINE_IID(kIPostToServerIID, NS_IPOSTTOSERVER_IID);


/* XXX: Don't include net.h... */
extern "C" {
extern int  NET_PollSockets();
extern void NET_ToggleTrace();
};

class TestConsumer : public nsIStreamListener
{

public:
    NS_DECL_ISUPPORTS
    
    TestConsumer();

    NS_IMETHOD GetBindInfo(void);
    NS_IMETHOD OnProgress(PRInt32 Progress, PRInt32 ProgressMax, const nsString& aMsg);
    NS_IMETHOD OnStartBinding(const char *aContentType);
    NS_IMETHOD OnDataAvailable(nsIInputStream *pIStream, PRInt32 length);
    NS_IMETHOD OnStopBinding(PRInt32 status, const nsString& aMsg);

protected:
    ~TestConsumer();
};


TestConsumer::TestConsumer()
{
    NS_INIT_REFCNT();
}


NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
NS_IMPL_ISUPPORTS(TestConsumer,kIStreamListenerIID);


TestConsumer::~TestConsumer()
{
    if (bTraceEnabled) {
        printf("\nTestConsumer is being deleted...\n");
    }
}


NS_IMETHODIMP TestConsumer::GetBindInfo(void)
{
    if (bTraceEnabled) {
        printf("\n+++ TestConsumer::GetBindInfo\n");
    }

    return 0;
}

NS_IMETHODIMP TestConsumer::OnProgress(PRInt32 Progress, PRInt32 ProgressMax, 
                                       const nsString& aMsg)
{
    if (bTraceEnabled) {
        if (aMsg.Length()) {
            printf("\n+++ TestConsumer::OnProgress: status ");
            fputs(aMsg, stdout);
            fputs("\n", stdout);
        } else {
            printf("\n+++ TestConsumer::OnProgress: %d of total %d\n", Progress, ProgressMax);
        }
    }

    return 0;
}

NS_IMETHODIMP TestConsumer::OnStartBinding(const char *aContentType)
{
    if (bTraceEnabled) {
        printf("\n+++ TestConsumer::OnStartBinding: Content type: %s\n", aContentType);
    }

    return 0;
}


NS_IMETHODIMP TestConsumer::OnDataAvailable(nsIInputStream *pIStream, PRInt32 length) 
{
    PRInt32 len;

    if (bTraceEnabled) {
        printf("\n+++ TestConsumer::OnDataAvailable: %d bytes available...\n", length);
    }

    do {
        PRInt32 err;
        char buffer[80];
        int i;

        len = pIStream->Read(&err, buffer, 0, 80);
        for (i=0; i<len; i++) {
            putchar(buffer[i]);
        }
    } while (len > 0);

    return 0;
}


NS_IMETHODIMP TestConsumer::OnStopBinding(PRInt32 status, const nsString& aMsg)
{
    if (bTraceEnabled) {
        printf("\n+++ TestConsumer::OnStopBinding... status: %d\n", status);
    }

    /* The document has been loaded, so drop out of the message pump... */
    urlLoaded = 1;
    return 0;
}



int main(int argc, char **argv)
{
#ifdef XP_PC
    MSG msg;
#endif
    nsString url_address;
    char buf[256];
    nsIStreamListener *pConsumer;
    nsIURL *pURL;
    nsresult result;

    if (argc < 2) {
        printf("test: -trace <URL>\n");
        return 0;
    }

    urlLoaded = 0;

    // Turn on netlib tracing...
    if (PL_strcasecmp(argv[1], "-trace") == 0) {
        NET_ToggleTrace();
        url_address = argv[2];
        bTraceEnabled = PR_TRUE;
    } else {
        url_address = argv[1];
        bTraceEnabled = PR_FALSE;
    }

    if (bTraceEnabled) {
        url_address.ToCString(buf, 256);
        printf("loading URL: %s...\n", buf);
    }

    pConsumer = new TestConsumer;
    pConsumer->AddRef();

    // Create the URL object...
    pURL = NULL;
    result = NS_NewURL(&pURL, url_address);
    if (NS_OK != result) {
        return 1;
    }

#if 0
    nsIPostToServer *pPoster;
    result = pURL->QueryInterface(kIPostToServerIID, (void**)&pPoster);
    if (result == NS_OK) {
        pPoster->SendFile("foo.txt");
    }
    NS_IF_RELEASE(pPoster);
#endif

    // Start the URL load...
    result = pURL->Open(pConsumer);

    /* If the open failed, then do not drop into the message loop... */
    if (NS_OK != result) {
        urlLoaded = 1;
    }

    // Enter the message pump to allow the URL load to proceed.
    while ( !urlLoaded ) {
#ifdef XP_PC
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
#endif

        (void) NET_PollSockets();
    }

    pURL->Release();

    return 0;

}
