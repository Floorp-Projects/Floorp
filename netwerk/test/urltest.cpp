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

int writeout(const char* i_pURL)
{
    if (i_pURL)
    {
	    cout << "Analyzing " << i_pURL << endl;
	    nsCOMPtr<nsIURI> pURL;

        nsresult result = NS_OK;
        NS_WITH_SERVICE(nsIIOService, pService, kIOServiceCID, &result);
        if (NS_FAILED(result)) return result;

        if (!pService)
            return -1;
	    pService->NewURI(i_pURL, nsnull, getter_AddRefs(pURL));

	    if (pURL)
	    {
		    char* temp;
		    PRInt32 port;
		    pURL->GetScheme(&temp);
		    cout << "Got " << (temp ? temp : "") << ',';
		    pURL->GetPreHost(&temp);
		    cout << (temp ? temp : "") << ',';
		    pURL->GetHost(&temp);
		    cout << (temp ? temp : "") << ',';
		    pURL->GetPort(&port);
		    cout << port << ',';
		    pURL->GetPath(&temp);
		    cout << (temp ? temp : "") << endl;
	    } else {
		    cout << "Can not create URL" << endl; 
	    }
        return 0;
    }
    return -1;
}

nsresult testURL(const char* i_pURL)
{
	nsresult result = NS_OK;
    const int tests = 8;

	/* 
		If you add a test case then make sure you also add the expected
	   	result in the resultset as well. 
	*/

	if (i_pURL)
	{
        writeout(i_pURL);
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
			"http://username:password@hostname:80/pathname"
		};

		const char* resultset[tests] =
		{
			"http,username:password,hostname.com,80,/pathname/more/path",
			",username,host,8080,/path",
			"http,,gagan,-1,/",
			",,host,0,/netlib",
			",,,-1,",
			"mailbox,,,-1,/foo",
			",username:pass,hostname.edu,80,/pathname",
			"http,username:password,hostname,80,/pathname"
		};

		for (int i = 0; i< tests; ++i)
		{
            writeout(url[i]);
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
        printf("urltest <URL> \n");
        return 0;
    }

    result = NS_AutoregisterComponents();
	if (NS_FAILED(result)) return result;

	if (PL_strncasecmp(argv[1], "-all", 4) == 0)
	{
		result = testURL(0);	
	}
	else
		result = testURL(argv[1]);

    return result;
}
