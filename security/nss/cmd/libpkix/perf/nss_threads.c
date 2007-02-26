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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sun Microsystems
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

void ThreadEntry(void* data)
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

void Test(CERTCertificate* cert, PRIntervalTime duration, PRUint32 threads)
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


void finish(char* message, int code)
{
        (void) printf(message);
        exit(code);
}

void usage(char* progname)
{
        (void) printf("Usage : %s <duration> <threads> <certnickname>\n\n",
                    progname);
        finish("", 0);
}

int main(int argc, char** argv)
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
        rv = NSS_Initialize(".", "", "",
                            "secmod.db", NSS_INIT_READONLY);
        if (SECSuccess != rv)
                {
                        finish("Unable to initialize NSS.\n", 1);
                }
        handle = CERT_GetDefaultCertDB();
        PR_ASSERT(handle);
        cert = CERT_FindCertByNicknameOrEmailAddr(handle, argv[3]);
        if (!cert)
                {
                        finish("Unable to find certificate.\n", 1);
                }
        Test(cert, duration, threads);

        return (0);
}
