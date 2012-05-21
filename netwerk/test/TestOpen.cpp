/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "nsCOMPtr.h"
#include "nsStringAPI.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIInputStream.h"
#include "nsNetUtil.h"

#include <stdio.h>

/*
 * Test synchronous Open.
 */

#define RETURN_IF_FAILED(rv, what) \
    PR_BEGIN_MACRO \
    if (NS_FAILED(rv)) { \
        printf(what ": failed - %08x\n", rv); \
        return -1; \
    } \
    PR_END_MACRO

int
main(int argc, char **argv)
{
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    nsresult rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    if (NS_FAILED(rv)) return rv;

    char buf[256];

    if (argc != 3) {
        printf("Usage: TestOpen url filename\nLoads a URL using ::Open, writing it to a file\n");
        return -1;
    }

    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIInputStream> stream;

    rv = NS_NewURI(getter_AddRefs(uri), argv[1]);
    RETURN_IF_FAILED(rv, "NS_NewURI");

    rv = NS_OpenURI(getter_AddRefs(stream), uri);
    RETURN_IF_FAILED(rv, "NS_OpenURI");

    FILE* outfile = fopen(argv[2], "wb");
    if (!outfile) {
      printf("error opening %s\n", argv[2]);
      return 1;
    }

    PRUint32 read;
    while (NS_SUCCEEDED(stream->Read(buf, sizeof(buf), &read)) && read) {
      fwrite(buf, 1, read, outfile);
    }
    printf("Done\n");

    fclose(outfile);

    NS_ShutdownXPCOM(nsnull);
    return 0;
}
