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
#include <assert.h>

#include "plstr.h"

#include "nsIComponentManager.h"
#include "nsINetService.h"
#include "nsIURL.h"
#include "nsCOMPtr.h"

// Do we still need these?
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

nsresult testURL(const char* pURL=0);

int main(int argc, char **argv)
{
    nsresult result = NS_OK;

    if (argc < 2) {
        printf("urltest: <URL> \n");
        return 0;
    }

    result = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                          "components");
	if (NS_FAILED(result)) return result;

    //Create the nsINetService...
    NS_WITH_SERVICE(nsINetService, pService, kNetServiceCID, &result);
	if (NS_FAILED(result)) return result;

    result = testURL(argv[1]);
    return 0;
}


nsresult testURL(const char* i_pURL)
{
    const char* temp;
	nsresult result = NS_OK;
    const int tests = 8;
	/* 
		If you add a test case then make sure you also add the expected
	   	result in the resultset as well. 
	*/
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

	const char* resultset[tests];
	{
		"http,username:password,hostname.com,80,/pathname/more/path",
		",username,host,8080,/path",
		"http,,gagan,-1,/",
		",,host,0,/netlib",
		",,,-1,",
		"mailbox,,,-1,/foo",
		",username:pass,hostname.edu,80,/pathname",
		"http,username:password,hostname,80,/pathname"
	}

    for (int i = 0; i< tests; ++i)
    {
        nsCOMPtr<nsIURL> pURL;
		
		cout << "Analyzing " << url[i] << endl;
		NS_NewURL(url[i], do_QueryInterface(pURL));
		if (pURL)
		{
			char* temp;
			PRInt32 port;
			pURL->GetScheme(&temp);
			cout << "Got " << temp << ',';
			pURL->GetPreHost(&temp);
			cout << temp << ',';
			pURL->GetHost(&temp);
			cout << temp << ',';
			pURL->GetPort(&port);
			cout << port << ',';
			pURL->GetPath(&temp);
			cout << temp << endl;
		}
		cout << "Expect " << resultset[i] << endl;
    }
  
    return 0;
}
