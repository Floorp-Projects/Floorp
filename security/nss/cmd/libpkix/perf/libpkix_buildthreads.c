/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * libpkixBuildThreads.c
 *
 * libpkix Builder Performance Evaluation application (multi-threaded)
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

#include "pkix.h"
#include "pkix_tools.h"
#include "pkix_pl_cert.h"

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

#undef pkixTempResult
#define PERF_DECREF(obj)                                                                \
    {                                                                                   \
        PKIX_Error *pkixTempResult = NULL;                                              \
        if (obj) {                                                                      \
            pkixTempResult = PKIX_PL_Object_DecRef((PKIX_PL_Object *)(obj), plContext); \
            obj = NULL;                                                                 \
        }                                                                               \
    }

static void finish(char *message, int code);

typedef struct ThreadDataStr tData;

struct ThreadDataStr {
    CERTCertificate *anchor;
    char *eecertName;
    PRIntervalTime duration;
    CERTCertDBHandle *handle;
    PRUint32 iterations;
};

#define PKIX_LOGGER_ON 1

#ifdef PKIX_LOGGER_ON

char *logLevels[] = {
    "None",
    "Fatal Error",
    "Error",
    "Warning",
    "Debug",
    "Trace"
};

static PKIX_Error *
loggerCallback(
    PKIX_Logger *logger,
    PKIX_PL_String *message,
    PKIX_UInt32 logLevel,
    PKIX_ERRORCLASS logComponent,
    void *plContext)
{
    char *msg = NULL;
    static int callCount = 0;

    msg = PKIX_String2ASCII(message, plContext);
    printf("Logging %s (%s): %s\n",
           logLevels[logLevel],
           PKIX_ERRORCLASSNAMES[logComponent],
           msg);
    PR_Free((void *)msg);

    return (NULL);
}

#endif /* PKIX_LOGGER_ON */

static void
ThreadEntry(void *data)
{
    tData *tdata = (tData *)data;
    PRIntervalTime duration = tdata->duration;
    PRIntervalTime start = PR_IntervalNow();

    PKIX_List *anchors = NULL;
    PKIX_ProcessingParams *procParams = NULL;
    PKIX_BuildResult *buildResult = NULL;
    CERTCertificate *nsseecert;
    PKIX_PL_Cert *eeCert = NULL;
    PKIX_CertStore *certStore = NULL;
    PKIX_List *certStores = NULL;
    PKIX_ComCertSelParams *certSelParams = NULL;
    PKIX_CertSelector *certSelector = NULL;
    PKIX_PL_Date *nowDate = NULL;
    void *state = NULL;       /* only relevant with non-blocking I/O */
    void *nbioContext = NULL; /* only relevant with non-blocking I/O */

    PR_ASSERT(duration);
    if (!duration) {
        return;
    }

    do {

        /* libpkix code */

        /* keep more update time, testing cache */
        PKIX_PL_Date_Create_UTCTime(NULL, &nowDate, plContext);

        /* CertUsage is 0x10 and no NSS arena */
        /* We haven't determined how we obtain the value of wincx */

        nsseecert = CERT_FindCertByNicknameOrEmailAddr(tdata->handle,
                                                       tdata->eecertName);
        if (!nsseecert)
            finish("Unable to find eecert.\n", 1);

        pkix_pl_Cert_CreateWithNSSCert(nsseecert, &eeCert, plContext);

        PKIX_List_Create(&anchors, plContext);

        /*
         * This code is retired.
         *      pkix_pl_Cert_CreateWithNSSCert
         *              (tdata->anchor, &anchorCert, NULL);
         *      PKIX_TrustAnchor_CreateWithCert(anchorCert, &anchor, NULL);
         *      PKIX_List_AppendItem(anchors, (PKIX_PL_Object *)anchor, NULL);
         */

        PKIX_ProcessingParams_Create(anchors, &procParams, plContext);

        PKIX_ProcessingParams_SetRevocationEnabled(procParams, PKIX_TRUE, plContext);

        PKIX_ProcessingParams_SetDate(procParams, nowDate, plContext);

        /* create CertSelector with target certificate in params */

        PKIX_ComCertSelParams_Create(&certSelParams, plContext);

        PKIX_ComCertSelParams_SetCertificate(certSelParams, eeCert, plContext);

        PKIX_CertSelector_Create(NULL, NULL, &certSelector, plContext);

        PKIX_CertSelector_SetCommonCertSelectorParams(certSelector, certSelParams, plContext);

        PKIX_ProcessingParams_SetTargetCertConstraints(procParams, certSelector, plContext);

        PKIX_PL_Pk11CertStore_Create(&certStore, plContext);

        PKIX_List_Create(&certStores, plContext);
        PKIX_List_AppendItem(certStores, (PKIX_PL_Object *)certStore, plContext);
        PKIX_ProcessingParams_SetCertStores(procParams, certStores, plContext);

        PKIX_BuildChain(procParams,
                        &nbioContext,
                        &state,
                        &buildResult,
                        NULL,
                        plContext);

        /*
                 * As long as we use only CertStores with blocking I/O, we
                 * know we must be done at this point.
                 */

        if (!buildResult) {
            (void)fprintf(stderr, "libpkix BuildChain failed.\n");
            PORT_Assert(0);
            return;
        }

        tdata->iterations++;

        PERF_DECREF(nowDate);
        PERF_DECREF(anchors);
        PERF_DECREF(procParams);
        PERF_DECREF(buildResult);
        PERF_DECREF(certStore);
        PERF_DECREF(certStores);
        PERF_DECREF(certSelParams);
        PERF_DECREF(certSelector);
        PERF_DECREF(eeCert);

    } while ((PR_IntervalNow() - start) < duration);
}

static void
Test(
    CERTCertificate *anchor,
    char *eecertName,
    PRIntervalTime duration,
    CERTCertDBHandle *handle,
    PRUint32 threads)
{
    tData data;
    tData **alldata;
    PRIntervalTime starttime, endtime, elapsed;
    PRUint32 msecs;
    float total = 0;
    PRThread **pthreads = NULL;
    PRUint32 i = 0;

    data.duration = duration;
    data.anchor = anchor;
    data.eecertName = eecertName;
    data.handle = handle;

    data.iterations = 0;

    starttime = PR_IntervalNow();
    pthreads = (PRThread **)PR_Malloc(threads * sizeof(PRThread *));
    alldata = (tData **)PR_Malloc(threads * sizeof(tData *));
    for (i = 0; i < threads; i++) {
        alldata[i] = (tData *)PR_Malloc(sizeof(tData));
        *alldata[i] = data;
        pthreads[i] =
            PR_CreateThread(PR_USER_THREAD,
                            ThreadEntry,
                            (void *)alldata[i],
                            PR_PRIORITY_NORMAL,
                            PR_GLOBAL_THREAD,
                            PR_JOINABLE_THREAD,
                            0);
    }

    for (i = 0; i < threads; i++) {
        tData *args = alldata[i];
        PR_JoinThread(pthreads[i]);
        total += args->iterations;
        PR_Free((void *)args);
    }

    PR_Free((void *)pthreads);
    PR_Free((void *)alldata);
    endtime = PR_IntervalNow();

    endtime = PR_IntervalNow();
    elapsed = endtime - starttime;
    msecs = PR_IntervalToMilliseconds(elapsed);
    total /= msecs;
    total *= 1000;
    (void)fprintf(stdout, "%f operations per second.\n", total);
}

static void
finish(char *message, int code)
{
    (void)printf(message);
    exit(code);
}

static void
usage(char *progname)
{
    (void)printf("Usage : %s <-d certStoreDirectory> <duration> <threads> "
                 "<anchorNickname> <eecertNickname>\n\n",
                 progname);
    finish("", 0);
}

int
libpkix_buildthreads(int argc, char **argv)
{
    CERTCertDBHandle *handle = NULL;
    CERTCertificate *eecert = NULL;
    PRIntervalTime duration = PR_SecondsToInterval(1);
    PRUint32 threads = 1;
    PKIX_UInt32 actualMinorVersion;
    PKIX_UInt32 j = 0;
    PKIX_Logger *logger = NULL;
    void *wincx = NULL;

    /* if (argc != 5) -- when TrustAnchor used to be on command line */
    if (argc != 4) {
        usage(argv[0]);
    }
    if (atoi(argv[1]) > 0) {
        duration = PR_SecondsToInterval(atoi(argv[1]));
    }
    if (atoi(argv[2]) > 0) {
        threads = atoi(argv[2]);
    }

    PKIX_PL_NssContext_Create(certificateUsageEmailSigner, PKIX_FALSE,
                              NULL, &plContext);

    handle = CERT_GetDefaultCertDB();
    PR_ASSERT(handle);

#ifdef PKIX_LOGGER_ON

    /* set logger to log trace and up */
    PKIX_SetLoggers(NULL, plContext);
    PKIX_Logger_Create(loggerCallback, NULL, &logger, plContext);
    PKIX_Logger_SetMaxLoggingLevel(logger, PKIX_LOGGER_LEVEL_WARNING, plContext);
    PKIX_AddLogger(logger, plContext);

#endif /* PKIX_LOGGER_ON */

    /*
         * This code is retired
         *      anchor = CERT_FindCertByNicknameOrEmailAddr(handle, argv[3]);
         *      if (!anchor) finish("Unable to find anchor.\n", 1);
         *
         *      eecert = CERT_FindCertByNicknameOrEmailAddr(handle, argv[4]);

         *      if (!eecert) finish("Unable to find eecert.\n", 1);
         *
         *      Test(anchor, eecert, duration, threads);
         */

    Test(NULL, argv[3], duration, handle, threads);

    PERF_DECREF(logger);

    PKIX_Shutdown(plContext);

    return (0);
}
