/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    uint32_t bytesRead, totalRead;

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

        rv = NS_OpenURI(getter_AddRefs(c[i].channel), c[i].uri, nullptr, nullptr);
        RETURN_IF_FAILED(rv, "NS_OpenURI");

        nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface(c[i].channel);
        if (httpChannel)
            httpChannel->SetOpenHasEventQueue(false);

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

    NS_ShutdownXPCOM(nullptr);
    return 0;
}
