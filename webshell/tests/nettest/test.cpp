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
#include "plstr.h"

#include "nsIStreamNotification.h"
#include "nsIInputStream.h"
#include "nsINetService.h"

int urlLoaded;
PRBool bTraceEnabled;


/* XXX: Don't include net.h... */
extern "C" {
extern int  NET_PollSockets();
extern void NET_ToggleTrace();
};

class TestConsumer : public nsIStreamNotification
{

public:
    NS_DECL_ISUPPORTS
    
    TestConsumer();

    NS_IMETHOD GetBindInfo(void);
    NS_IMETHOD OnProgress(void);
    NS_IMETHOD OnStartBinding(void);
    NS_IMETHOD OnDataAvailable(nsIInputStream *pIStream);
    NS_IMETHOD OnStopBinding(void);

protected:
    ~TestConsumer();
};


TestConsumer::TestConsumer()
{
    NS_INIT_REFCNT();
}


NS_DEFINE_IID(kIStreamNotificationIID, NS_ISTREAMNOTIFICATION_IID);
NS_IMPL_ISUPPORTS(TestConsumer,kIStreamNotificationIID);


TestConsumer::~TestConsumer()
{
    if (bTraceEnabled) {
        printf("TestConsumer is being deleted...\n");
    }
}


NS_IMETHODIMP TestConsumer::GetBindInfo(void)
{
    if (bTraceEnabled) {
        printf("+++ TestConsumer::GetBindInfo\n");
    }

    return 0;
}

NS_IMETHODIMP TestConsumer::OnProgress(void)
{
    printf("+++ TestConsumer::OnProgress\n");

    return 0;
}

NS_IMETHODIMP TestConsumer::OnStartBinding(void)
{
    if (bTraceEnabled) {
        printf("+++ TestConsumer::OnStartBinding\n");
    }

    return 0;
}


NS_IMETHODIMP TestConsumer::OnDataAvailable(nsIInputStream *pIStream) 
{
    PRInt32 len;

    if (bTraceEnabled) {
        printf("+++ TestConsumer::OnDataAvailable\n");
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


NS_IMETHODIMP TestConsumer::OnStopBinding(void)
{
    if (bTraceEnabled) {
        printf("+++ TestConsumer::OnStopBinding\n");
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
    char *url_address;
    nsIStreamNotification *pConsumer;
    nsINetService *pNetlib;
    nsresult result;

    if (argc < 2) {
        printf("test: -trace <URL>\n");
        return 0;
    }

    urlLoaded = 0;

    /* Create an instance of the INetService... (aka Netlib) */
    if (NS_OK != NS_NewINetService(&pNetlib, NULL)) {
        return 1;
    }

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
        printf("loading URL: %s...\n", url_address);
    }

    pConsumer = new TestConsumer;
    pConsumer->AddRef();

    // Start the URL load...
    result = pNetlib->OpenStream(url_address, pConsumer);

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

        if (NET_PollSockets() == PR_FALSE) urlLoaded = 1;
    }

    pNetlib->Release();

    return 0;

}
