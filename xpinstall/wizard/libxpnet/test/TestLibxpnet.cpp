/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Samir Gehani <sgehani@netscape.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nsFTPConn.h"
#include "nsHTTPConn.h"

const int kProxySrvrLen = 1024;
const char kHTTP[8] = "http://";
const char kFTP[7] = "ftp://";
const char kLoclFile[7] = "zzzFTP";

int
ProgressCB(int aBytesSoFar, int aTotalFinalSize)
{
    printf(".");
    return 0;
}

void
spew(char *funcName, int rv)
{
    printf("%s returned %d\n", funcName, rv);
}

void
usage(char *prog)
{
    fprintf(stderr, "usage: %s <URL> [ProxyServer ", prog);
    fprintf(stderr, "ProxyPort [ProxyUserName ProxyPassword]\n");
}

int
main(int argc, char **argv)
{
    char *proxyUser = 0, *proxyPswd = 0;
    char proxyURL[kProxySrvrLen];
    int rv = 0;

    if (argc < 2)
    {
        usage(argv[0]);
        exit(1);
    }

    /* has a proxy server been specified? */
    if (argc >= 4)
    {
        memset(proxyURL, 0, kProxySrvrLen);
        sprintf(proxyURL, "http://%s:%s", argv[2], argv[3]);

        if (argc >=6)
        {
            proxyUser = argv[4];
            proxyPswd = argv[5];
        }

        nsHTTPConn *conn = new nsHTTPConn(proxyURL);
        conn->SetProxyInfo(argv[1], proxyUser, proxyPswd);
        printf("Proxy URL: %s\n", argv[1]);
        if (proxyUser && proxyPswd)
        {
            printf("Proxy User: %s\n", proxyUser);
            printf("Proxy Pswd: %s\n", proxyPswd);
        }

        rv = conn->Open();
        spew("nsHTTPConn::Open", rv);

        rv = conn->Get(ProgressCB, NULL); // use leaf from URL
        printf("\n"); // newline after progress completes
        spew("nsHTTPConn::Get", rv);
    
        rv = conn->Close();
        spew("nsHTTPConn::Close", rv);
    }
    else
    {
        /* is this an HTTP URL? */
        if (strncmp(argv[1], kHTTP, strlen(kHTTP)) == 0)
        {
            nsHTTPConn *conn = new nsHTTPConn(argv[1]);
            
            rv = conn->Open();
            spew("nsHTTPConn::Open", rv);

            rv = conn->Get(ProgressCB, NULL);
            printf("\n"); // newline after progress completes
            spew("nsHTTPConn::Get", rv);

            rv = conn->Close();
            spew("nsHTTPConn::Close", rv);
        }

        /* or is this an FTP URL? */
        else if (strncmp(argv[1], kFTP, strlen(kFTP)) == 0)
        {
            char *host = 0, *path = 0, *file = (char*) kLoclFile;
            int port = 21;

            rv = nsHTTPConn::ParseURL(kFTP, argv[1], &host, &port, &path);
            spew("nsHTTPConn::ParseURL", rv);

            nsFTPConn *conn = new nsFTPConn(host);

            rv = conn->Open();
            spew("nsFTPConn::Open", rv);

            if (strrchr(path, '/') != (path + strlen(path)))
                file = strrchr(path, '/') + 1; // set to leaf name
            rv = conn->Get(path, file, nsFTPConn::BINARY, 1, ProgressCB);
            printf("\n"); // newline after progress completes
            spew("nsFTPConn::Get", rv);

            rv = conn->Close();
            spew("nsFTPConn::Close", rv);

            if (host)
                free(host);
            if (path)
                free(path);
        }

        /* or we don't understand the args */
        else
        {
            fprintf(stderr, "Like, uhm, dude!  I don't get you.  ");
            fprintf(stderr, "See usage...\n");
            usage(argv[0]);
        }
    }

    return 0;
}
