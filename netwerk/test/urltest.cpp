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

/*
    A test file to check default URL parsing.
	-Gagan Saksena 03/25/99
*/

#include <stdio.h>

#include "plstr.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "iostream.h"

// Define CIDs...
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);
static NS_DEFINE_CID(kStdURLCID, 				 NS_STANDARDURL_CID);

int writeout(const char* i_pURL, PRBool bUseStd =PR_TRUE)
{
    if (i_pURL)
    {
	    cout << "Analyzing " << i_pURL << endl;

	    nsCOMPtr<nsIURI> pURL;
        nsresult result = NS_OK;
		
		if (bUseStd) 
		{
			nsIURI* url;
			result = nsComponentManager::CreateInstance(kStdURLCID, nsnull, 
				nsCOMTypeInfo<nsIURI>::GetIID(), (void**)&url);
			if (NS_FAILED(result))
			{
				cout << "CreateInstance failed" << endl;
				return result;
			}
			pURL = url;
			pURL->SetSpec((char*)i_pURL);
		}
		else 
		{
			NS_WITH_SERVICE(nsIIOService, pService, kIOServiceCID, &result);
			if (NS_FAILED(result)) 
			{
				cout << "Service failed!" << endl;
				return result;
			}	

			result = pService->NewURI(i_pURL, nsnull, getter_AddRefs(pURL));
		}
	    if (NS_SUCCEEDED(result))
	    {
		    char* temp;
		    PRInt32 port;
		    pURL->GetScheme(&temp);
		    cout << "Got    " << (temp ? temp : "") << ',';
		    pURL->GetPreHost(&temp);
		    cout << (temp ? temp : "") << ',';
		    pURL->GetHost(&temp);
		    cout << (temp ? temp : "") << ',';
		    pURL->GetPort(&port);
		    cout << port << ',';
		    pURL->GetPath(&temp);
		    cout << (temp ? temp : "") << endl;
            nsCRT::free(temp);
	    } else {
		    cout << "Can not create URL" << endl; 
	    }
        return NS_OK;
    }
    return -1;
}

nsresult testURL(const char* i_pURL, PRBool bUseStd=PR_TRUE)
{
    const int tests = 9;

	/* 
		If you add a test case then make sure you also add the expected
	   	result in the resultset as well. 
	*/

	if (i_pURL)
	{
        writeout(i_pURL, bUseStd);
	}
	else
	{
		const char* url[tests] = 
		{
			"http://username:password@hostname.com:80/pathname/./more/stuff/../path",
			"username@host:8080/path",
			"http://gagan/",
			"host:port/netlib", //port should now be 0
			"", //empty string
			"mailbox:///foo", // No host specified path should be /foo
			"user:pass@hostname.edu:80/pathname", //this is always user:pass and not http:user
			"http://username:password@hostname:80/pathname",
			"resource:/pathname"
		};

		const char* resultset[tests] =
		{
			"http,username:password,hostname.com,80,/pathname/more/path",
			",username,host,8080,/path",
			"http,,gagan,-1,/",
			",,host,0,/netlib",
			",,,-1,",
			"mailbox,,,-1,/foo",
			",user:pass,hostname.edu,80,/pathname",
			"http,username:password,hostname,80,/pathname",
			"resource,,,-1,/pathname"
		};

		// These tests will fail to create a URI from NS_NewURI calls...
		// because of a missing scheme: in front. This set assumes
		// an only working the handler is available.
		PRBool failWithURI[tests] =
		{
			PR_FALSE,
			PR_TRUE,
			PR_FALSE,
			PR_TRUE,
			PR_TRUE,
			PR_TRUE,
			PR_TRUE,
			PR_FALSE,
			PR_FALSE
		};
		nsresult stat;
		for (int i = 0; i< tests; ++i)
		{
			cout << "--------------------" << endl;
			if (!bUseStd)
				cout << "Should" << (failWithURI[i] ? " not " : " ")
					<< "create URL" << endl;
			stat = writeout(url[i], bUseStd);
			if (NS_FAILED(stat))
				return stat;
			if (bUseStd || !failWithURI[i])
				cout << "Expect " << resultset[i] << endl << endl;
		}
	}
	return 0;
}

nsresult NS_AutoregisterComponents()
{
  nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);
  return rv;
}

int main(int argc, char **argv)
{
    nsresult result = NS_OK;

    if (argc < 2) {
        printf("urltest [-std] [<URL> | -all]\n");
        return 0;
    }

    result = NS_AutoregisterComponents();
	if (NS_FAILED(result)) return result;
	
	PRBool bStdTest= PR_FALSE;
	PRBool bTestAll= PR_FALSE;

    for (int i=1; i<argc; i++) {
        if (PL_strcasecmp(argv[i], "-std") == 0) 
		{
			bStdTest = PR_TRUE;
			continue;
		}

        if (PL_strcasecmp(argv[i], "-all") == 0) 
		{
			bTestAll = PR_TRUE;
			continue;
		}
	}
	return bTestAll ? testURL(0,bStdTest) : testURL(argv[argc-1],bStdTest);
}
