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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsReadableUtils.h"
#include "nsNetCID.h"

// Define CIDs...
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);
static NS_DEFINE_CID(kStdURLCID,                 NS_STANDARDURL_CID);

char* gFileIO = 0;

nsresult writeoutto(const char* i_pURL, char** o_Result, PRBool bUseStd = PR_TRUE)
{
    if (!o_Result || !i_pURL)
        return NS_ERROR_FAILURE;
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
            return NS_ERROR_FAILURE;
        }
        pURL = url;
        pURL->SetSpec((char*)i_pURL);
    }
    else 
    {
        nsCOMPtr<nsIIOService> pService = 
                 do_GetService(kIOServiceCID, &result);
        if (NS_FAILED(result)) 
        {
            cout << "Service failed!" << endl;
            return NS_ERROR_FAILURE;
        }   

        result = pService->NewURI(i_pURL, nsnull, getter_AddRefs(pURL));
    }
    nsCString output;
    if (NS_SUCCEEDED(result))
    {
        nsCOMPtr<nsIURL> tURL = do_QueryInterface(pURL);
        nsXPIDLCString temp;
        PRInt32 port;

        tURL->GetScheme(getter_Copies(temp));
        output += temp ? (const char*)temp : "";
        output += ',';
        tURL->GetUsername(getter_Copies(temp));
        output += temp ? (const char*)temp : "";
        output += ',';
        tURL->GetPassword(getter_Copies(temp));
        output += temp ? (const char*)temp : "";
        output += ',';
        tURL->GetHost(getter_Copies(temp));
        output += temp ? (const char*)temp : "";
        output += ',';
        tURL->GetPort(&port);
        output.AppendInt(port);
        output += ',';
        tURL->GetDirectory(getter_Copies(temp));
        output += temp ? (const char*)temp : "";
        output += ',';
        tURL->GetFileBaseName(getter_Copies(temp));
        output += temp ? (const char*)temp : "";
        output += ',';
        tURL->GetFileExtension(getter_Copies(temp));
        output += temp ? (const char*)temp : "";
        output += ',';
        tURL->GetParam(getter_Copies(temp));
        output += temp ? (const char*)temp : "";
        output += ',';
        tURL->GetQuery(getter_Copies(temp));
        output += temp ? (const char*)temp : "";
        output += ',';
        tURL->GetRef(getter_Copies(temp));
        output += temp ? (const char*)temp : "";
        output += ',';
        tURL->GetSpec(getter_Copies(temp));
        output += temp ? (const char*)temp : "";
        *o_Result = ToNewCString(output);
    } else {
        output = "Can not create URL";
        *o_Result = ToNewCString(output);
    }
    return NS_OK;
}

nsresult writeout(const char* i_pURL, PRBool bUseStd =PR_TRUE)
{
    char* temp = 0;
    if (!i_pURL) return NS_ERROR_FAILURE;
    int rv = writeoutto(i_pURL, &temp, bUseStd);
    cout << i_pURL << endl << temp << endl;
    delete[] temp;
    return rv;
}

/* construct a url and print out its elements separated by commas and
   the whole spec */
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
    char* prevResult = nsnull;
    char* tempurl = nsnull;

    while (testfile.getline(temp,512))
    {
        if ((*temp == '#') || (PL_strlen(temp)==0))
            continue;

        if (0 == count%3)
        {
            if (prevResult) delete[] prevResult;
            cout << "Testing:  " << temp << endl;
            writeoutto(temp, &prevResult, bUseStd);
        }
        else if (1 == count%3) {
            if (tempurl) delete[] tempurl;
            tempurl = nsCRT::strdup(temp);
        } else { 
            if (!prevResult)
                cout << "no results to compare to!" << endl;
            else 
            {
                PRInt32 res;
                cout << "Result:   " << prevResult << endl;
                if (bUseStd) {
                    cout << "Expected: " << tempurl << endl;
                    res = PL_strcmp(tempurl, prevResult);
                } else {
                    cout << "Expected: " << temp << endl;
                    res = PL_strcmp(temp, prevResult);
                }

                if (res == 0)
                    cout << "\tPASSED" << endl << endl;
                else 
                {
                    cout << "\tFAILED" << endl << endl;
                    failed++;
                }
            }
        }
        count++;
    }
    if (failed>0) {
        cout << failed << " tests FAILED out of " << count/3 << endl;
        return NS_ERROR_FAILURE;
    } else {
        cout << "All " << count/3 << " tests PASSED." << endl;
        return NS_OK;
    }
}

nsresult makeAbsTest(const char* i_BaseURI, const char* relativePortion,
                     const char* expectedResult)
{
    if (!i_BaseURI)
        return NS_ERROR_FAILURE;
    
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
    cout << "With      " << relativePortion << endl;
    
    cout << "Got       " <<  newURL << endl;
    if (expectedResult) {
        cout << "Expect    " << expectedResult << endl;
        int res = PL_strcmp(newURL, expectedResult);
        if (res == 0) {
            cout << "\tPASSED" << endl << endl;
            return NS_OK;
        } else {
            cout << "\tFAILED" << endl << endl;
            return NS_ERROR_FAILURE;
        }
    }
    return NS_OK;
}

int doMakeAbsTest(const char* i_URL = 0, const char* i_relativePortion=0)
{
    if (i_URL && i_relativePortion)
    {
        return makeAbsTest(i_URL, i_relativePortion, nsnull);
    }

    // Run standard tests. These tests are based on the ones described in 
    // rfc2396 with the exception of the handling of ?y which is wrong as
    // notified by on of the RFC authors.

    /* Section C.1.  Normal Examples

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
      ;x         = <URL:http://a/b/c/;x>
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

    struct test {
        const char* baseURL;
        const char* relativeURL;
        const char* expectedResult;
    };

    test tests[] = {
        // Tests from rfc2396, section C.1 with the exception of the
        // handling of ?y
        { "http://a/b/c/d;p?q#f",     "g:h",         "g:h" },
        { "http://a/b/c/d;p?q#f",     "g",           "http://a/b/c/g" },
        { "http://a/b/c/d;p?q#f",     "./g",         "http://a/b/c/g" },
        { "http://a/b/c/d;p?q#f",     "g/",          "http://a/b/c/g/" },
        { "http://a/b/c/d;p?q#f",     "/g",          "http://a/g" },
        { "http://a/b/c/d;p?q#f",     "//g",         "http://g" },
        { "http://a/b/c/d;p?q#f",     "?y",          "http://a/b/c/d;p?y" },
        { "http://a/b/c/d;p?q#f",     "g?y",         "http://a/b/c/g?y" },
        { "http://a/b/c/d;p?q#f",     "g?y/./x",     "http://a/b/c/g?y/./x" },
        { "http://a/b/c/d;p?q#f",     "#s",          "http://a/b/c/d;p?q#s" },
        { "http://a/b/c/d;p?q#f",     "g#s",         "http://a/b/c/g#s" },
        { "http://a/b/c/d;p?q#f",     "g#s/./x",     "http://a/b/c/g#s/./x" },
        { "http://a/b/c/d;p?q#f",     "g?y#s",       "http://a/b/c/g?y#s" },
        { "http://a/b/c/d;p?q#f",     ";x",          "http://a/b/c/;x" },
        { "http://a/b/c/d;p?q#f",     "g;x",         "http://a/b/c/g;x" },
        { "http://a/b/c/d;p?q#f",     "g;x?y#s",     "http://a/b/c/g;x?y#s" },
        { "http://a/b/c/d;p?q#f",     ".",           "http://a/b/c/" },
        { "http://a/b/c/d;p?q#f",     "./",          "http://a/b/c/" },
        { "http://a/b/c/d;p?q#f",     "..",          "http://a/b/" },
        { "http://a/b/c/d;p?q#f",     "../",         "http://a/b/" },
        { "http://a/b/c/d;p?q#f",     "../g",        "http://a/b/g" },
        { "http://a/b/c/d;p?q#f",     "../..",       "http://a/" },
        { "http://a/b/c/d;p?q#f",     "../../",      "http://a/" },
        { "http://a/b/c/d;p?q#f",     "../../g",     "http://a/g" },

        // Our additional tests...
        { "http://a/b/c/d;p?q#f",     "#my::anchor", "http://a/b/c/d;p?q#my::anchor" },
        { "http://a/b/c/d;p?q#f",     "get?baseRef=viewcert.jpg", "http://a/b/c/get?baseRef=viewcert.jpg" },

        // Make sure relative query's work right even if the query
        // string contains absolute urls or other junk.
        { "http://a/b/c/d;p?q#f",     "?http://foo",        "http://a/b/c/d;p?http://foo" },
        { "http://a/b/c/d;p?q#f",     "g?http://foo",       "http://a/b/c/g?http://foo" },
        {"http://a/b/c/d;p?q#f",      "g/h?http://foo",     "http://a/b/c/g/h?http://foo" },
        { "http://a/b/c/d;p?q#f",     "g/h/../H?http://foo","http://a/b/c/g/H?http://foo" },
        { "http://a/b/c/d;p?q#f",     "g/h/../H?http://foo?baz", "http://a/b/c/g/H?http://foo?baz" },
        { "http://a/b/c/d;p?q#f",     "g/h/../H?http://foo;baz", "http://a/b/c/g/H?http://foo;baz" },
        { "http://a/b/c/d;p?q#f",     "g/h/../H?http://foo#bar", "http://a/b/c/g/H?http://foo#bar" },
        { "http://a/b/c/d;p?q#f",     "g/h/../H;baz?http://foo", "http://a/b/c/g/H;baz?http://foo" },
        { "http://a/b/c/d;p?q#f",     "g/h/../H;baz?http://foo#bar", "http://a/b/c/g/H;baz?http://foo#bar" },
        { "http://a/b/c/d;p?q#f",     "g/h/../H;baz?C:\\temp", "http://a/b/c/g/H;baz?C:\\temp" },
        { "http://a/b/c/d;p?q#f",     "", "http://a/b/c/d;p?q" },
        { "http://a/b/c/d;p?q#f",     "#", "http://a/b/c/d;p?q#" },
        { "http://a/b/c;p/d;p?q#f",   "../g;p" , "http://a/b/g;p" },

    };

    const int numTests = sizeof(tests) / sizeof(tests[0]);
    int failed = 0;
    nsresult rv;
    for (int i = 0 ; i<numTests ; ++i)
    {
        rv = makeAbsTest(tests[i].baseURL, tests[i].relativeURL,
                         tests[i].expectedResult);
        if (NS_FAILED(rv))
            failed++;
    }
    if (failed>0) {
        cout << failed << " tests FAILED out of " << numTests << endl;
        return NS_ERROR_FAILURE;
    } else {
        cout << "All " << numTests << " tests PASSED." << endl;
        return NS_OK;
    }
}

nsresult NS_AutoregisterComponents()
{
  nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);
  return rv;
}

void printusage(void)
{
    cout << "urltest [-std] [-file <filename>] <URL> " <<
        " [-abs <relative>]" << endl << endl
        << "\t-std  : Generate results using nsStdURL. "  << endl
        << "\t-file : Read URLs from file. "  << endl
        << "\t-abs  : Make an absolute URL from the base (<URL>) and the" << endl
        << "\t\trelative path specified. If -abs is given without " << endl
        << "\t\ta base URI standard RFC 2396 relative URL tests" << endl
        << "\t\tare performed. Implies -std." << endl
        << "\t<URL> : The string representing the URL." << endl;
}

int main(int argc, char **argv)
{
    int rv = -1;
    nsresult result = NS_OK;

    if (argc < 2) {
        printusage();
        return NS_OK;
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
            if (i+1 >= argc)
            {
                printusage();
                return NS_OK;
            }
        }
        else if (PL_strcasecmp(argv[i], "-abs") == 0)
        {
            if (!gFileIO) 
            {
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
                return NS_OK;
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
