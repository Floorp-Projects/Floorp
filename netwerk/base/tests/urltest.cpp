/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
    TODO- big cleanup! -Gagan Saksena 03/25/99
*/

#include <stdio.h>
#include <assert.h>

#ifdef WIN32
#include <windows.h>
#endif

#include "plstr.h"
#include "nsIEventQueue.h"

#include "nsIComponentManager.h"

#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsINetService.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsString.h"
#include "nsReadableUtils.h"

#include "nsIURL.h"
#include "nsINetService.h"

#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#define XPCOM_DLL  "xpcom32.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define NETLIB_DLL "libnetlib.so"
#define XPCOM_DLL  "libxpcom.so"
#endif
#endif

// Define CIDs...
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

// Define IIDs...
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
//NS_DEFINE_IID(kIPostToServerIID, NS_IPOSTTOSERVER_IID);

#ifdef XP_UNIX
extern "C" char *fe_GetConfigDir(void) {
  printf("XXX: return /tmp for fe_GetConfigDir\n");
  return strdup("/tmp");
}
#endif /* XP_UNIX */

#if 0 // Enable after the stream listener interface is cleared up.
class TestConsumer : public nsIStreamListener
{

public:
    NS_DECL_ISUPPORTS
    
    TestConsumer();

    NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* info);
    NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 Progress, PRUint32 ProgressMax);
    NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg);
    NS_IMETHOD OnStartRequest(nsIURL* aURL, const char *aContentType);
    NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *pIStream, PRUint32 length);
    NS_IMETHOD OnStopRequest(nsIURL* aURL, nsresult status, const PRUnichar* aMsg);

protected:
    ~TestConsumer();
};


TestConsumer::TestConsumer()
{
    NS_INIT_REFCNT();
}


NS_IMPL_ISUPPORTS1(TestConsumer, nsIStreamListener)


TestConsumer::~TestConsumer()
{
    if (bTraceEnabled) {
        printf("\n+++ TestConsumer is being deleted...\n");
    }
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
        nsAutoString str(aMsg);
        char* c = ToNewCString(str);
        fputs(c, stdout);
        free(c);
        fputs("\n", stdout);
    }

    return 0;
}

NS_IMETHODIMP TestConsumer::OnStartRequest(nsIURL* aURL, const char *aContentType)
{
    if (bTraceEnabled) {
        printf("\n+++ TestConsumer::OnStartRequest: URL: %p, Content type: %s\n", aURL, aContentType);
    }

    return 0;
}


NS_IMETHODIMP TestConsumer::OnDataAvailable(nsIURL* aURL, nsIInputStream *pIStream, PRUint32 length) 
{
    PRUint32 len;

    if (bTraceEnabled) {
        printf("\n+++ TestConsumer::OnDataAvailable: URL: %p, %d bytes available...\n", aURL, length);
    }

    do {
        nsresult err;
        char buffer[80];
        PRUint32 i;

        err = pIStream->Read(buffer, 0, 80, &len);
        if (err == NS_OK) {
            for (i=0; i<len; i++) {
                putchar(buffer[i]);
            }
        }
    } while (len > 0);

    return 0;
}


NS_IMETHODIMP TestConsumer::OnStopRequest(nsIURL* aURL, nsresult status, const PRUnichar* aMsg)
{
    if (bTraceEnabled) {
        printf("\n+++ TestConsumer::OnStopRequest... URL: %p status: %d\n", aURL, status);
    }

    if (NS_FAILED(status)) {
      const char* url;
      aURL->GetSpec(&url);

      printf("Unable to load URL %s\n", url);
    }
    /* The document has been loaded, so drop out of the message pump... */
    urlLoaded = 1;
    return 0;
}

#endif // 0 - enabled after streamlistener is fixed.

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

int testURL(const char* pURL=0);

int main(int argc, char **argv)
{
    nsAutoString url_address;
//    char buf[256];
//    nsIStreamListener *pConsumer;
    nsIEventQueueService* pEventQService;
//    nsIURL *pURL;
    nsresult result;
    int i;

    if (argc < 2) {
        printf("urltest: <URL> \n");
        return 0;
    }

    nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, "./components");

    testURL(argv[1]);
    return 0;
#if 0
    nsRepository::RegisterComponent(
		kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterComponent(
		kNetServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);

    // Create the Event Queue for this thread...
    pEventQService = nsnull;
    result = nsServiceManager::GetService(kEventQueueServiceCID,
                                          kIEventQueueServiceIID,
                                          (nsISupports **)&pEventQService);
    if (NS_SUCCEEDED(result)) {
      // XXX: What if this fails?
      result = pEventQService->CreateThreadEventQueue();
    }

    for (i=1; i < argc; i++) {
        if (PL_strcasecmp(argv[i], "-all") == 0) {
            testURL(0);
            continue;
        } 

        testURL(argv[i]);
    }

    if (nsnull != pEventQService) {
        pEventQService->DestroyThreadEventQueue();
        nsServiceManager::ReleaseService(kEventQueueServiceCID, pEventQService);
    }
    return 0;
#endif
}


int testURL(const char* i_pURL)
{
    const char* temp;
	if (i_pURL)
	{
		nsIURL* pURL = CreateURL(i_pURL);
		pURL->DebugString(&temp);
		cout << temp <<endl;
		
        nsIInputStream* is = 0;
        if (NS_OK == pURL->GetStream(&is))
        {
            ReadStreamSynchronously(is);
        }

        pURL->Release();
		return 0;
	}

    const int tests = 8;
    const char* url[tests] = 
    {
        "http://username:password@hostname.com:80/pathname/./more/stuff/../path",
        "username@host:8080/path",
        "http://gagan/",
        "host:port/netlib", //port should now be 0
        "", //empty string
        "mailbox:///foo", // No host specified path should be /foo
        "user:pass@hostname.edu:80/pathname", //this is always user:pass and not http:user
        "username:password@hostname:80/pathname"
    };

    for (int i = 0; i< tests; ++i)
    {
        nsIURL* pURL = CreateURL(url[i]);
        pURL->DebugString(&temp);
        cout << temp << endl;
        pURL->Release();
    }
  
    return 0;
}
