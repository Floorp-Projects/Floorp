/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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


/*=============================================================================
 * This test program is designed to test netlib's implementation of nsITransport.
 * In particular, it is currently geared towards testing their socket implemnation.
 * When the test program starts up, you are prompted for a port and domain 
 * (I may have these hard coded right now to be nsmail-2 and port 143).
 * After entering this information, we'll build a connection to the host name.
 * You can then enter raw protocol text (i.e. "1 capability") and watch the data
 * that comes back from the socket. After data is returned, you can enter another
 * line of protocol.
*===============================================================================*/

#include <stdio.h>
#include <assert.h>

#ifdef XP_PC
#include <windows.h>
#endif

#include "plstr.h"
#include "plevent.h"

#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsITransport.h"
#include "nsIURL.h"
#include "nsINetService.h"
#include "nsIComponentManager.h"
#include "nsString.h"


int urlLoaded;
PRBool bTraceEnabled;
PRBool bLoadAsync;

#include "nsIPostToServer.h"
#include "nsINetService.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsXPComCIID.h"

#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#define XPCOM_DLL  "xpcom32.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define NETLIB_DLL "libnetlib.so"
#endif
#endif

static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

static NS_DEFINE_IID(kIInputStreamIID, NS_IINPUTSTREAM_IID);

static NS_DEFINE_IID(kIPostToServerIID, NS_IPOSTTOSERVER_IID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

/* XXX: Don't include net.h... */
extern "C" {
extern void NET_ToggleTrace();
};

class TestConsumer : public nsIStreamListener
{

public:
    NS_DECL_ISUPPORTS
    
	PRBool m_runningURL;
	PRBool m_waitingForData;

    TestConsumer(nsITransport * transport);

	void LoadURL(nsIURL * urlToLoad);
	nsresult Write(const char * writeData);

    NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* info);
    NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 Progress, PRUint32 ProgressMax);
    NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg);
    NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
    NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *pIStream, PRUint32 length);
    NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult status, const PRUnichar* aMsg);

protected:
    ~TestConsumer();

	nsITransport	  * m_transport;
	nsIURL			  * m_url;
	nsIOutputStream	  * m_outputStream;
	nsIStreamListener * m_outputConsumer;
};


TestConsumer::TestConsumer(nsITransport * transport)
{
	NS_PRECONDITION(transport != nsnull, "invalid transport");

    NS_INIT_REFCNT();
	if (transport)
	{
		m_transport = transport;
		NS_ADDREF(m_transport);

		// register ourselves as a stream listener for the transport interface
		m_transport->SetInputStreamConsumer(this);
		m_transport->GetOutputStreamConsumer(&m_outputConsumer);
		m_transport->GetOutputStream(&m_outputStream);
	}
	else
	{
		m_outputStream = NULL;
		m_outputConsumer = NULL;
	}

	m_url = NULL;
}

void TestConsumer::LoadURL(nsIURL * urlToLoad)
{
	// give the transport layer the url to load
	m_transport->Open(urlToLoad);
	m_runningURL = PR_TRUE;
	m_waitingForData = PR_TRUE;

	if (m_url)
	{
		NS_RELEASE(m_url);
		m_url = NULL;
	}

	m_url = urlToLoad;
	NS_ADDREF(urlToLoad);
}


NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
NS_IMPL_ISUPPORTS(TestConsumer,kIStreamListenerIID);


TestConsumer::~TestConsumer()
{
    if (bTraceEnabled) {
        printf("\n+++ TestConsumer is being deleted...\n");
    }

	if (m_url)
		NS_RELEASE(m_url);
}

nsresult TestConsumer::Write(const char * writeData)
{
	PRUint32 numBytes = 0;
	nsresult rv = NS_OK;

	// write the data out through our output stream.
	NS_PRECONDITION(m_outputStream && m_outputConsumer, "transport not configured for output");
	if (writeData && *writeData)
	{
		rv = m_outputStream->Write(writeData, PL_strlen(writeData), &numBytes);
		if (NS_SUCCEEDED(rv))
		{
			nsIInputStream * inputStream = NULL;
			rv = m_outputStream->QueryInterface(kIInputStreamIID, (void **) &inputStream);
			if (inputStream && NS_SUCCEEDED(rv))
			{
				rv = m_outputConsumer->OnDataAvailable(m_url, inputStream, PL_strlen(writeData));
				m_waitingForData = PR_TRUE;
				NS_RELEASE(inputStream); 
			}
		}
	}

	return rv;
}

NS_IMETHODIMP TestConsumer::GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* info)
{
    if (bTraceEnabled) {
        printf("\n+++ TestConsumer::GetBindInfo: URL: %p\n", aURL);
    }

    return 0;
}

NS_IMETHODIMP TestConsumer::OnProgress(nsIURL* aURL, PRUint32 Progress, 
                                       PRUint32 ProgressMax)
{
    if (bTraceEnabled) {
        printf("\n+++ TestConsumer::OnProgress: URL: %p - %d of total %d\n", aURL, Progress, ProgressMax);
    }

    return 0;
}

NS_IMETHODIMP TestConsumer::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
    if (bTraceEnabled) {
        printf("\n+++ TestConsumer::OnStatus: ");
        nsString str(aMsg);
        char* c = str.ToNewCString();
        fputs(c, stdout);
        free(c);
        fputs("\n", stdout);
    }

    return 0;
}

NS_IMETHODIMP TestConsumer::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
    if (bTraceEnabled) {
        printf("\n+++ TestConsumer::OnStartBinding: URL: %p, Content type: %s\n", aURL, aContentType);
    }

    return 0;
}


NS_IMETHODIMP TestConsumer::OnDataAvailable(nsIURL* aURL, nsIInputStream *pIStream, PRUint32 length) 
{
    PRUint32 totalBytesRead, len = 0;

    if (bTraceEnabled) {
        printf("\n+++ TestConsumer::OnDataAvailable: URL: %p, %d bytes available...\n", aURL, length);
    }

    do {

        nsresult err;
        char buffer[1000];
        PRUint32 i;

		PRUint32 numToRead = length > 1000 ? 1000 : length;
        err = pIStream->Read(buffer, numToRead, &len);
		totalBytesRead += len;
        if (err == NS_OK) {
            for (i=0; i<len; i++) {
                putchar(buffer[i]);
            }
        }
    } while (totalBytesRead < length && (len > 0) );

	m_waitingForData = PR_FALSE;

    return 0;
}


NS_IMETHODIMP TestConsumer::OnStopBinding(nsIURL* aURL, nsresult status, const PRUnichar* aMsg)
{
    if (bTraceEnabled) {
        printf("\n+++ TestConsumer::OnStopBinding... URL: %p status: %d\n", aURL, status);
    }

    /* The document has been loaded, so drop out of the message pump... */
    urlLoaded = 1;

	m_runningURL = PR_FALSE;
    return 0;
}


nsresult ReadStreamSynchronously(nsIInputStream* aIn)
{
    nsresult rv;
    char buffer[1024];

    if (nsnull != aIn) {
        PRUint32 len;

        do {
            PRUint32 i;

            rv = aIn->Read(buffer, sizeof(buffer), &len);
            for (i=0; i<len; i++) {
                putchar(buffer[i]);
            }
        } while (len > 0);
    }
    return NS_OK;
}



int main()
{
    char buf[256];
    nsIStreamListener *pConsumer;
	nsINetService * pNetService;
    nsresult result;
    int i;

    nsComponentManager::RegisterComponent(kNetServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);
	nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);

	// Create the Event Queue for this thread...
    nsIEventQueueService *pEventQService = nsnull;
    result = nsServiceManager::GetService(kEventQueueServiceCID,
                                          kIEventQueueServiceIID,
                                          (nsISupports **)&pEventQService);
	if (NS_SUCCEEDED(result)) {
      // XXX: What if this fails?
      result = pEventQService->CreateThreadEventQueue();
    }

    bTraceEnabled = PR_FALSE;
    bLoadAsync    = PR_TRUE;

	NET_ToggleTrace();
    bTraceEnabled = PR_TRUE;

	// ask the net lib service for a nsINetStream:
	result = NS_NewINetService(&pNetService, NULL);
	if (NS_FAILED(result) || !pNetService)
	{
		printf("unable to initialize net serivce. \n");
		return 1;
	}

	        // Create the URL object...
     nsIURL * pURL = NULL;

	 // prompt the user for port and host name 
	 PRUint32 portToUse = 143; // default to imap...
	 char hostName[100];
	 char urlToRun[200];
	 char portString[100];
	 portString[0] = '\0';
	 hostName[0] = '\0';
	 urlToRun[0] = '\0';

	 PL_strncpy(hostName, "nsmail-2.mcom.com", 100);
	 PL_strncpy(urlToRun, "sockstub://", 200);

	 printf("Enter port to use [%d]: ", portToUse);
	 scanf("%[^\n]", portString);
	 if (portString && *portString)
	 {
		 PRUint32 temp = atoi(portString);
		 if (temp > 0)
			portToUse = temp;
	 }
	 scanf("%c", portString);  // eat the extra CR

	 // now prompt for the host name....
	 printf("Enter host name to use [%s]: ", hostName);
	 scanf("%[^\n]", &hostName);
	 PL_strcat(urlToRun, hostName);
	 scanf("%c", portString);  // eat the extra CR

	 result = NS_NewURL(&pURL, urlToRun);

	 // create a transport socket...
	 nsITransport * transport = NULL;
	 pNetService->CreateSocketTransport(&transport, portToUse, "nsmail-2.mcom.com");
	 
	 if (transport)
	 {
		 TestConsumer * pTestConsumer = NULL;

		 pTestConsumer = new TestConsumer(transport);
		 NS_ADDREF(pTestConsumer);
		 if (pTestConsumer)
		 {
			 pTestConsumer->LoadURL(pURL);
			 char userText[1000];
			 while (pTestConsumer->m_runningURL)
			 {
				 // if we are waiting for data don't read the next command
				 if (!pTestConsumer->m_waitingForData)
				 {
					 userText[0] = '\0';
					 scanf("%[^\n]", &userText);
					 if (*userText)
					 {
						 if (userText[0])  // append a line return
						 {
							PRUint32 length = PL_strlen(userText);
							userText[length] = '\n';
							userText[length+1] = '\0';
						 }
						 if (PL_strncasecmp("exit", userText, 4) == 0)
							 pTestConsumer->m_runningURL = 0;
						 else
							 pTestConsumer->Write(userText);
					 }
					 else
						 scanf("%c", userText);
				 }

			     MSG msg;

				if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}


			 }

			 NS_RELEASE(pTestConsumer);
		 }

		 NS_RELEASE(transport);
	 }

	// shut down:
	NS_RELEASE(pNetService);
    
    return 0;
}
