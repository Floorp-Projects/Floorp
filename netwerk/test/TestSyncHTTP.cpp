/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include <nsCOMPtr.h>
#include <nsString.h>
#include <nsIURI.h>
#include <nsIChannel.h>
#include <nsIHTTPChannel.h>
#include <nsIInputStream.h>
#include <nsNetUtil.h>

/*
 * Test synchronous HTTP.
 */

#define RETURN_IF_FAILED(rv, what) \
    PR_BEGIN_MACRO \
    if (NS_FAILED(rv)) { \
        printf(what ": failed - %08x\n", rv); \
        return -1; \
    } \
    PR_END_MACRO

struct TestContext {
    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsIInputStream> inputStream;
    PRTime t1, t2;
    PRUint32 bytesRead, totalRead;

    TestContext()
        : t1(0), t2(0), bytesRead(0), totalRead(0)
        { printf("TestContext [this=%p]\n", (void*)this); }
   ~TestContext()
        { printf("~TestContext [this=%p]\n", (void*)this); }
};

int
main(int argc, char **argv)
{
    nsresult rv;
    TestContext *c;
    int i, nc=0, npending=0;
    char buf[256];

    if (argc < 2) {
        printf("Usage: TestSyncHTTP <url-list>\n");
        return -1;
    }

    c = new TestContext[argc-1];

    for (i=0; i<(argc-1); ++i, ++nc) {
        rv = NS_NewURI(getter_AddRefs(c[i].uri), argv[i+1]);
        RETURN_IF_FAILED(rv, "NS_NewURI");

        rv = NS_OpenURI(getter_AddRefs(c[i].channel), c[i].uri, nsnull, nsnull);
        RETURN_IF_FAILED(rv, "NS_OpenURI");

        nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface(c[i].channel);
        if (httpChannel)
            httpChannel->SetOpenHasEventQueue(PR_FALSE);

        // initialize these fields for reading
        c[i].bytesRead = 1;
        c[i].totalRead = 0;
    }

    for (i=0; i<nc; ++i) {
        c[i].t1 = PR_Now();

        rv = c[i].channel->Open(getter_AddRefs(c[i].inputStream));
        RETURN_IF_FAILED(rv, "nsIChannel::OpenInputStream");
    }

    npending = nc;
    while (npending) {
        for (i=0; i<nc; ++i) {
            //
            // read the response content...
            //
            if (c[i].bytesRead > 0) {
                rv = c[i].inputStream->Read(buf, sizeof buf, &c[i].bytesRead);
                RETURN_IF_FAILED(rv, "nsIInputStream::Read");
                c[i].totalRead += c[i].bytesRead;

                if (c[i].bytesRead == 0) {
                    c[i].t2 = PR_Now();
                    printf("finished GET of: %s\n", argv[i+1]);
                    printf("total read: %u bytes\n", c[i].totalRead);
                    printf("total read time: %0.3f\n",
                            ((double) (c[i].t2 - c[i].t1))/1000000.0);
                    npending--;
                }
            }
        }
    }

    delete[] c;

    NS_ShutdownXPCOM(nsnull);
    return 0;
}
