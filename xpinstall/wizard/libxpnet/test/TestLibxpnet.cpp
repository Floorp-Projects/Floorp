/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Samir Gehani <sgehani@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef macintosh
#include <console.h>
#endif

#include "nsFTPConn.h"
#include "nsHTTPConn.h"

const int kProxySrvrLen = 1024;
const char kHTTP[8] = "http://";
const char kFTP[7] = "ftp://";
const char kLoclFile[7] = "zzzFTP";
static int sTotalSize = 0;

int
ProgressCB(int aBytesSoFar, int aTotalFinalSize)
{
    static int bTotalSizeFound = FALSE;
    
    if (!bTotalSizeFound && aTotalFinalSize > 0)
    {
        sTotalSize = aTotalFinalSize;
        bTotalSizeFound = TRUE;
    }
    
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
    fprintf(stderr, "usage: %s <URL>\n\t\t[<ProxyServer> ", prog);
    fprintf(stderr, "<ProxyPort> [<ProxyUserName> <ProxyPassword>]]\n");
    fprintf(stderr, "\t\t[-r<ResumePos> or -rg]\n");
    
#ifdef macintosh
    int fin = getchar();
#endif
}

int
main(int argc, char **argv)
{
    char *proxyUser = 0, *proxyPswd = 0;
    char proxyURL[kProxySrvrLen];
    int rv = 0, resPos = 0;
    time_t startTime, endTime;
    double dlTime = 0;  /* download time */
    float dlRate = 0;   /* download rate */
    int bResumeOrGet = 0;
    
#ifdef macintosh
    argc = ccommand(&argv);
#endif

    if (argc < 2)
    {
        usage(argv[0]);
        exit(1);
    }

    /* get resume pos if -r arg passed in */
    for (int i = 1; i < argc; ++i)
    {
        /* resume or get */
        if (strncmp(argv[i], "-rg", 3) == 0)
        {
            bResumeOrGet = 1;
        }

        /* resume from pos */
        else if (strncmp(argv[i], "-r", 2) == 0)
        {
            resPos = atoi(argv[i] + 2);
            printf("resPos = %d\n", resPos);

            break;
        }        
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

        startTime = time(NULL);
        if (bResumeOrGet)
        {
            rv = conn->ResumeOrGet(ProgressCB, NULL); // use leaf from URL
        }
        else
        {
            rv = conn->Get(ProgressCB, NULL, resPos); // use leaf from URL
        }
        endTime = time(NULL);
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

            startTime = time(NULL);
            if (bResumeOrGet)
            {
                rv = conn->ResumeOrGet(ProgressCB, NULL);
            }
            else
            {
                rv = conn->Get(ProgressCB, NULL, resPos);
            }
            endTime = time(NULL);
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
            startTime = time(NULL);
            if (bResumeOrGet)
            {
                rv = conn->ResumeOrGet(path, file, nsFTPConn::BINARY, 1, 
                     ProgressCB);
            }
            else
            {
                rv = conn->Get(path, file, nsFTPConn::BINARY, resPos, 1, 
                     ProgressCB);
            }
            endTime = time(NULL);
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
            return 1;
        }
    }
    
    /* compute rate */
    if (sTotalSize > 0)
    {
        dlTime = difftime(endTime, startTime);
        if (dlTime > 0)
            dlRate = sTotalSize/dlTime;
    }
    printf("Download rate = %f\tTotal size = %d\tTotal time = %f\n", 
            dlRate, sTotalSize, dlTime);

#ifdef macintosh
    int fin = getchar();
#endif

    return 0;
}
