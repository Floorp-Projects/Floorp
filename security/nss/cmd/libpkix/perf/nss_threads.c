/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * nssThreads.c
 *
 * NSS Performance Evaluation application (multi-threaded)
 *
 */

#include <stdio.h>
#include <string.h>

#include "secutil.h"

#include "nspr.h"
#include "prtypes.h"
#include "prtime.h"
#include "prlong.h"

#include "pk11func.h"
#include "secasn1.h"
#include "cert.h"
#include "cryptohi.h"
#include "secoid.h"
#include "certdb.h"
#include "nss.h"

typedef struct ThreadDataStr tData;

struct ThreadDataStr {
    CERTCertificate* cert;
    PRIntervalTime duration;
    PRUint32 iterations;
};

static void ThreadEntry(void* data)
{
        tData* tdata = (tData*) data;
        PRIntervalTime duration = tdata->duration;
        PRTime now = PR_Now();
        PRIntervalTime start = PR_IntervalNow();

        PR_ASSERT(duration);
        if (!duration)
                {
                        return;
                }
        do {
                SECStatus rv = CERT_VerifyCertificate
                        (CERT_GetDefaultCertDB(),
                        tdata->cert,
                        PR_TRUE,
                        certificateUsageEmailSigner,
                        now,
                        NULL,
                        NULL,
                        NULL);
                if (rv != SECSuccess)
                        {
                                (void) fprintf(stderr, "Validation failed.\n");
                                PORT_Assert(0);
                                return;
                        }
                tdata->iterations ++;
        } while ((PR_IntervalNow() - start) < duration);
}

static void Test(CERTCertificate* cert, PRIntervalTime duration, PRUint32 threads)
{
        tData data;
        tData** alldata;
        PRIntervalTime starttime, endtime, elapsed;
        PRUint32 msecs;
        float total = 0;
        PRThread** pthreads = NULL;
        PRUint32 i = 0;

        data.duration = duration;
        data.cert = cert;
        data.iterations = 0;

        starttime = PR_IntervalNow();
        pthreads = (PRThread**)PR_Malloc(threads*sizeof (PRThread*));
        alldata = (tData**)PR_Malloc(threads*sizeof (tData*));
        for (i = 0; i < threads; i++)
                {
                        alldata[i] = (tData*)PR_Malloc(sizeof (tData));
                        *alldata[i] = data;
                        pthreads[i] =
                                PR_CreateThread(PR_USER_THREAD,
                                                ThreadEntry,
                                                (void*) alldata[i],
                                                PR_PRIORITY_NORMAL,
                                                PR_GLOBAL_THREAD,
                                                PR_JOINABLE_THREAD,
                                                0);

                }
        for (i = 0; i < threads; i++)
                {
                        tData* args = alldata[i];
                        PR_JoinThread(pthreads[i]);
                        total += args->iterations;
                        PR_Free((void*)args);
                }
        PR_Free((void*) pthreads);
        PR_Free((void*) alldata);
        endtime = PR_IntervalNow();

        endtime = PR_IntervalNow();
        elapsed = endtime - starttime;
        msecs = PR_IntervalToMilliseconds(elapsed);
        total /= msecs;
        total *= 1000;
        (void) fprintf(stdout, "%f operations per second.\n", total);
}


static void finish(char* message, int code)
{
        (void) printf(message);
        exit(code);
}

static void usage(char* progname)
{
        (void) printf("Usage : %s <duration> <threads> <certnickname>\n\n",
                    progname);
        finish("", 0);
}

int nss_threads(int argc, char** argv)
{
        SECStatus rv = SECSuccess;
        CERTCertDBHandle *handle = NULL;
        CERTCertificate* cert = NULL;
        PRIntervalTime duration = PR_SecondsToInterval(1);
        PRUint32 threads = 1;
        if (argc != 4)
                {
                        usage(argv[0]);
                }
        if (atoi(argv[1]) > 0)
                {
                        duration = PR_SecondsToInterval(atoi(argv[1]));
                }
        if (atoi(argv[2]) > 0)
                {
                        threads = atoi(argv[2]);
                }

        handle = CERT_GetDefaultCertDB();
        PR_ASSERT(handle);
        cert = CERT_FindCertByNicknameOrEmailAddr(handle, argv[3]);
        if (!cert)
                {
                        finish("Unable to find certificate.\n", 1);
                }
        Test(cert, duration, threads);

        CERT_DestroyCertificate(cert);
        return (0);
}
