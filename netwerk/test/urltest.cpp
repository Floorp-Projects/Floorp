/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

/*
    A test file to check default URL parsing.
    -Gagan Saksena 03/25/99
*/

#include <stdio.h>
#include <fstream.h>
//using namespace std;

#include "plstr.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "iostream.h"
#include "nsXPIDLString.h"
#include "nsString.h"

// Define CIDs...
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);
static NS_DEFINE_CID(kStdURLCID,                 NS_STANDARDURL_CID);

char* gFileIO = 0;

int writeoutto(const char* i_pURL, char** o_Result, PRBool bUseStd = PR_TRUE)
{
    if (!o_Result || !i_pURL)
        return -1;
    *o_Result = 0;
    nsCOMPtr<nsIURI> pURL;
    nsresult result = NS_OK;

    if (bUseStd) 
    {
        nsIURI* url;
        result = nsComponentManager::CreateInstance(kStdURLCID, nsnull, 
                NS_GET_IID(nsIURI), (void**)&url);
        if (NS_FAILED(result))
        {
            cout << "CreateInstance failed" << endl;
            return 1;
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
            return 1;
        }   

        result = pService->NewURI(i_pURL, nsnull, getter_AddRefs(pURL));
    }
    if (NS_SUCCEEDED(result))
    {
        nsCOMPtr<nsIURL> tURL = do_QueryInterface(pURL);
        nsXPIDLCString temp;
        PRInt32 port;

        nsCString output;
        tURL->GetScheme(getter_Copies(temp));
        output += temp ? (const char*)temp : "";
        output += ',';
        tURL->GetPreHost(getter_Copies(temp));
        output += temp ? (const char*)temp : "";
        output += ',';
        tURL->GetHost(getter_Copies(temp));
        output += temp ? (const char*)temp : "";
        output += ',';
        tURL->GetPort(&port);
        output.AppendInt(port);
        output += ',';
        tURL->GetQuery(getter_Copies(temp));
        output += temp ? (const char*)temp : "";
        output += ',';
        tURL->GetPath(getter_Copies(temp));
        output += temp ? (const char*)temp : "";

        *o_Result = output.ToNewCString();
    } 
    else  {
        cout << "Can not create URL" << endl; 
        return -1;
    }
    return 1;
}

int writeout(const char* i_pURL, PRBool bUseStd =PR_TRUE)
{
    char* temp = 0;
    if (!i_pURL) return -1;
    int rv = writeoutto(i_pURL, &temp, bUseStd);
    if (rv < 0)
        return rv;
    cout << i_pURL << endl << temp << endl;
    delete[] temp;
    return rv;
}

/* construct a url and print out its five elements separated by commas */
nsresult testURL(const char* i_pURL, PRBool bUseStd=PR_TRUE)
{

    if (i_pURL)
        return writeout(i_pURL, bUseStd);

    if (!gFileIO)
        return NS_ERROR_FAILURE;

    ifstream testfile(gFileIO);
    if (!testfile) 
    {
        cerr << "Cannot open testfile: " << gFileIO << endl;
        return NS_ERROR_FAILURE;
    }

    char temp[512];
    int count=0;
    int failed=0;
    char* prevResult =0;

    while (testfile.getline(temp,512))
    {
        if ((*temp == '#') || (PL_strlen(temp)==0))
            continue;

        if (0 == count%2)
        {
            if (prevResult) delete[] prevResult;
            writeoutto(temp, &prevResult, bUseStd);
        }
        else 
        {
            if (!prevResult)
                cout << "no results to compare to!" << endl;
            else 
            {
                cout << prevResult << endl;
                cout << temp << endl;
                if (PL_strcmp(temp, prevResult) == 0)
                    cout << "\tPASSED" << endl;
                else 
                {
                    cout << "\tFAILED" << endl;
                    failed++;
                }
            }
        }
        count++;
   }
    if (failed>0)
        cout << failed << " tests FAILED out of " << count << endl;
    else
        cout << "All " << count << " tests PASSED." << endl;

    return 0;
}

int makeAbsTest(const char* i_BaseURI, const char* relativePortion)
{
    if (!i_BaseURI)
        return -1;
    
    // build up the base URL
    nsCOMPtr<nsIURI> baseURL;
    nsresult status = nsComponentManager::CreateInstance(kStdURLCID, nsnull, 
        NS_GET_IID(nsIURI), getter_AddRefs(baseURL));
    if (NS_FAILED(status))
    {
        cout << "CreateInstance failed" << endl;
        return status;
    }
    status = baseURL->SetSpec((char*)i_BaseURI);
    if (NS_FAILED(status)) return status;


    // get the new spec
    nsXPIDLCString newURL;
    status = baseURL->Resolve(relativePortion, getter_Copies(newURL));
    if (NS_FAILED(status)) return status;

    nsXPIDLCString temp;
    baseURL->GetSpec(getter_Copies(temp));

    cout << "Analyzing " << temp << endl;
    cout << "With " << relativePortion << endl;
    
    cout << "Got    " <<  newURL << endl;
    return 0;
}

int doMakeAbsTest(const char* i_URL = 0, const char* i_relativePortion=0)
{
    if (i_URL && i_relativePortion)
    {
        return makeAbsTest(i_URL, i_relativePortion);
    }

    // Run standard tests. These tests are based on the ones described in 
    // rfc1808
    /* Section 5.1.  Normal Examples

      g:h        = <URL:g:h>
      g          = <URL:http://a/b/c/g>
      ./g        = <URL:http://a/b/c/g>
      g/         = <URL:http://a/b/c/g/>
      /g         = <URL:http://a/g>
      //g        = <URL:http://g>
      ?y         = <URL:http://a/b/c/d;p?y>
      g?y        = <URL:http://a/b/c/g?y>
      g?y/./x    = <URL:http://a/b/c/g?y/./x>
      #s         = <URL:http://a/b/c/d;p?q#s>
      g#s        = <URL:http://a/b/c/g#s>
      g#s/./x    = <URL:http://a/b/c/g#s/./x>
      g?y#s      = <URL:http://a/b/c/g?y#s>
      ;x         = <URL:http://a/b/c/d;x>
      g;x        = <URL:http://a/b/c/g;x>
      g;x?y#s    = <URL:http://a/b/c/g;x?y#s>
      .          = <URL:http://a/b/c/>
      ./         = <URL:http://a/b/c/>
      ..         = <URL:http://a/b/>
      ../        = <URL:http://a/b/>
      ../g       = <URL:http://a/b/g>
      ../..      = <URL:http://a/>
      ../../     = <URL:http://a/>
      ../../g    = <URL:http://a/g>

    */

    const char baseURL[] = "http://a/b/c/d;p?q#f";

    struct test {
        const char* relativeURL;
        const char* expectedResult;
    };

    test tests[] = {
        // Tests from rfc1808, section 5.1
        { "g:h",                "g:h" },
        { "g",                  "http://a/b/c/g" },
        { "./g",                "http://a/b/c/g" },
        { "g/",                 "http://a/b/c/g/" },
        { "/g",                 "http://a/g" },
        { "//g",                "http://g" },
        { "?y",                 "http://a/b/c/d;p?y" },
        { "g?y",                "http://a/b/c/g?y" },
        { "g?y/./x",            "http://a/b/c/g?y/./x" },
        { "#s",                 "http://a/b/c/d;p?q#s" },
        { "g#s",                "http://a/b/c/g#s" },
        { "g#s/./x",            "http://a/b/c/g#s/./x" },
        { "g?y#s",              "http://a/b/c/g?y#s" },
        { ";x",                 "http://a/b/c/d;x" },
        { "g;x",                "http://a/b/c/g;x" },
        { "g;x?y#s",            "http://a/b/c/g;x?y#s" },
        { ".",                  "http://a/b/c/" },
        { "./",                 "http://a/b/c/" },
        { "..",                 "http://a/b/" },
        { "../",                "http://a/b/" },
        { "../g",               "http://a/b/g" },
        { "../..",              "http://a/" },
        { "../../",             "http://a/" },
        { "../../g",            "http://a/g" },

        // Our additional tests...
        { "#my::anchor",        "http://a/b/c/d;p?q#my::anchor" },
        { "get?baseRef=viewcert.jpg", "http://a/b/c/get?baseRef=viewcert.jpg" },

        // Make sure relative query's work right even if the query
        // string contains absolute urls or other junk.
        { "?http://foo",        "http://a/b/c/d;p?http://foo" },
        { "g?http://foo",       "http://a/b/c/g?http://foo" },
        { "g/h?http://foo",     "http://a/b/c/g/h?http://foo" },
        { "g/h/../H?http://foo","http://a/b/c/g/H?http://foo" },
        { "g/h/../H?http://foo?baz", "http://a/b/c/g/H?http://foo?baz" },
        { "g/h/../H?http://foo;baz", "http://a/b/c/g/H?http://foo;baz" },
        { "g/h/../H?http://foo#bar", "http://a/b/c/g/H?http://foo#bar" },
        { "g/h/../H;baz?http://foo", "http://a/b/c/g/H;baz?http://foo" },
        { "g/h/../H;baz?http://foo#bar", "http://a/b/c/g/H;baz?http://foo#bar" },
        { "g/h/../H;baz?C:\\temp", "http://a/b/c/g/H;baz?C:\\temp" },
    };

    const int numTests = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0 ; i<numTests ; ++i)
    {
        makeAbsTest(baseURL, tests[i].relativeURL);
        cout << "Expect " << tests[i].expectedResult << endl << endl;
    }
    return 0;
}

nsresult NS_AutoregisterComponents()
{
  nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);
  return rv;
}

void printusage(void)
{
    cout << "urltest [-std] [-all] [-file <filename>] <URL> " <<
        " [-abs <relative>]" << endl << endl
        << "\t-std  : Generate results using nsStdURL. "  << endl
        << "\t-file : Read URLs from file. "  << endl
        << "\t-all  : Run all standard tests. Ignores <URL> then." << endl
        << "\t-abs  : Make an absolute URL from the base (<URI>) and the" << endl
        << "\t\trelative path specified. Can be used with -all. " 
        << "Implies -std" << endl
        << "\t<URL> : The string representing the URL." << endl;
}

int main(int argc, char **argv)
{
    int rv = -1;
    nsresult result = NS_OK;

    if (argc < 2) {
        printusage();
        return 0;
    }

    result = NS_AutoregisterComponents();
    if (NS_FAILED(result)) return result;

    // end of all messages from register components...
    cout << "------------------" << endl << endl; 

    PRBool bStdTest= PR_FALSE;
    PRBool bMakeAbs= PR_FALSE;
    char* relativePath = 0;
    char* url = 0;
    for (int i=1; i<argc; i++) {
        if (PL_strcasecmp(argv[i], "-std") == 0) 
        {
            bStdTest = PR_TRUE;
        }
        else if (PL_strcasecmp(argv[i], "-abs") == 0)
        {
            if (!gFileIO) 
            {
                if (i+1 >= argc)
                {
                    printusage(); 
                    return 0;
                }
                relativePath = argv[i+1]; 
                i++;
            }
            bMakeAbs = PR_TRUE;
        }
        else if (PL_strcasecmp(argv[i], "-file") == 0)
        {
            if (i+1 >= argc)
            {
                printusage();
                return 0;
            }
            gFileIO = argv[i+1];
            i++;
        }
        else
        {
            url = argv[i];
        }
    }
    PRTime startTime = PR_Now();
    if (bMakeAbs)
    {
        rv = (url && relativePath) ?  doMakeAbsTest(url, relativePath) : 
            doMakeAbsTest();
    }
    else
    {
        rv = gFileIO ? testURL(0, bStdTest) : testURL(url, bStdTest);
    }
    if (gFileIO)
    {
        PRTime endTime = PR_Now();
        printf("Elapsed time: %d micros.\n", (PRInt32) 
            (endTime - startTime));
    }
    return rv;
}
