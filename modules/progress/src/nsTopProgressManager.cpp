/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsTopProgressManager.h"
#include "nsTransfer.h"
#include "xp.h"                 // for FE_* callbacks
#include "xpgetstr.h"
#include "prprf.h"
#include "prmem.h"
#include "plstr.h"

#define OBJECT_TABLE_INIT_SIZE 32


////////////////////////////////////////////////////////////////////////
//
// Debugging garbage
//
//

#if 0 /* defined(DEBUG) */
#define TRACE_PROGRESS(args) pm_TraceProgress args

static void
pm_TraceProgress(const char* fmtstr, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmtstr);
    PR_vsnprintf(buf, sizeof(buf), fmtstr, ap);
    va_end(ap);

#if defined(XP_WIN)
    OutputDebugString(buf);
#elif defined(XP_UNIX)
#elif defined(XP_MAC)
#endif
}

#else /* defined(DEBUG) */
#define TRACE_PROGRESS(args)
#endif /* defined(DEBUG) */




////////////////////////////////////////////////////////////////////////
//
// Hash table allocation routines
//
//

static PR_CALLBACK void*
AllocTable(void* pool, PRSize size)
{
    return PR_MALLOC(size);
}

static PR_CALLBACK void
FreeTable(void* pool, void* item)
{
    PR_DELETE(item);
}

static PR_CALLBACK PLHashEntry*
AllocEntry(void* pool, const void* key)
{
    return PR_NEW(PLHashEntry);
}

static PR_CALLBACK void
FreeEntry(void* pool, PLHashEntry* he, PRUintn flag)
{
    if (flag == HT_FREE_VALUE) {
        if (he->value) {
            nsTransfer* obj = (nsTransfer*) he->value;
            obj->Release();
        }
    }
    else if (flag == HT_FREE_ENTRY) {
        // we don't own the key, so leave it alone...

        if (he->value) {
            nsTransfer* obj = (nsTransfer*) he->value;
            obj->Release();
        }
        PR_DELETE(he);
    }
}


static PLHashAllocOps AllocOps = {
    AllocTable,
    FreeTable,
    AllocEntry,
    FreeEntry
};


static PLHashNumber
pm_HashURL(const void* key)
{
    return (PLHashNumber) key;
}


static int
pm_CompareURLs(const void* v1, const void* v2)
{
    return v1 == v2;
}


////////////////////////////////////////////////////////////////////////
//
// Hash table iterators
//
//

static PRIntn
pm_AggregateTransferInfo(PLHashEntry* he, PRIntn i, void* closure)
{
    AggregateTransferInfo* info = (AggregateTransferInfo*) closure;

    ++(info->ObjectCount);
    if (he->value) {
        nsTransfer* transfer = (nsTransfer*) he->value;

        if (transfer->IsComplete())
            ++(info->CompleteCount);

        if (transfer->IsSuspended())
            ++(info->SuspendedCount);

        info->MSecRemaining    += transfer->GetMSecRemaining();
        info->BytesReceived    += transfer->GetBytesReceived();
        info->ContentLength    += transfer->GetContentLength();

        if (! transfer->IsComplete() &&
            transfer->GetContentLength() == transfer->GetBytesReceived())
            ++(info->UnknownLengthCount);
    }

    return HT_ENUMERATE_NEXT;
}


////////////////////////////////////////////////////////////////////////
//
// nsTopProgressManager
//
//


////////////////////////////////////////////////////////////////////////


nsTopProgressManager::nsTopProgressManager(MWContext* context)
    : nsProgressManager(context),
      fActualStart(PR_Now()),
      fProgressBarStart(PR_Now()),
      fDefaultStatus(NULL)
{
    fURLs = PL_NewHashTable(OBJECT_TABLE_INIT_SIZE,
                            pm_HashURL,
                            pm_CompareURLs,
                            PL_CompareValues,
                            &AllocOps,
                            NULL);

    // Start the progress manager
    fTimeout = FE_SetTimeout(nsTopProgressManager::TimeoutCallback, (void*) this, 500);
    PR_ASSERT(fTimeout);

    // to avoid "strobe" mode...
    fProgress = 1;
    FE_SetProgressBarPercent(fContext, fProgress);
}


nsTopProgressManager::~nsTopProgressManager(void)
{
    if (fDefaultStatus) {
        PL_strfree(fDefaultStatus);
        fDefaultStatus = NULL;
    }

    if (fURLs) {
        PL_HashTableDestroy(fURLs);
        fURLs = NULL;
    }

    if (fTimeout) {
        FE_ClearTimeout(fTimeout);
        fTimeout = NULL;
    }

    // XXX Needs to go to allxpstr.h
    FE_Progress(fContext, "Done.");
}


////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsTopProgressManager::OnStartBinding(const URL_Struct* url)
{
    PR_ASSERT(url);
    if (! url)
        return NS_ERROR_NULL_POINTER;

    PR_ASSERT(fURLs);
    if (! fURLs)
        return NS_ERROR_NULL_POINTER;

    TRACE_PROGRESS(("OnStartBinding(%s)\n", url->address));

    PL_HashTableAdd(fURLs, url, new nsTransfer(url));
    return NS_OK;
}

NS_IMETHODIMP
nsTopProgressManager::OnProgress(const URL_Struct* url,
                                 PRUint32 bytesReceived,
                                 PRUint32 contentLength)
{
    // Some sanity checks...
    PR_ASSERT(url);
    if (! url)
        return NS_ERROR_NULL_POINTER;

    TRACE_PROGRESS(("OnProgress(%s, %ld, %ld)\n", url->address, bytesReceived, contentLength));

    PR_ASSERT(fURLs);
    if (! fURLs)
        return NS_ERROR_NULL_POINTER;

    nsTransfer* transfer = (nsTransfer*) PL_HashTableLookup(fURLs, url);

    PR_ASSERT(transfer);
    if (!transfer)
        return NS_ERROR_NULL_POINTER;

    transfer->SetProgress(bytesReceived, contentLength);
    return NS_OK;
}

NS_IMETHODIMP
nsTopProgressManager::OnStatus(const URL_Struct* url, const char* message)
{
    TRACE_PROGRESS(("OnStatus(%s, %s)\n", (url ? url->address : NULL), message));

    // There are cases when transfer may be null, and that's ok.
    if (url) {
        PR_ASSERT(fURLs);
        if (! fURLs)
            return NS_ERROR_NULL_POINTER;

        nsTransfer* transfer = (nsTransfer*) PL_HashTableLookup(fURLs, url);

        PR_ASSERT(transfer);
        if (transfer)
            transfer->SetStatus(message);
    }

    if (fDefaultStatus)
        PL_strfree(fDefaultStatus);

    fDefaultStatus = PL_strdup(message);
    return NS_OK;
}


NS_IMETHODIMP
nsTopProgressManager::OnSuspend(const URL_Struct* url)
{
    PR_ASSERT(url);
    if (! url)
        return NS_ERROR_NULL_POINTER;

    TRACE_PROGRESS(("OnSuspend(%s)\n", url->address));

    PR_ASSERT(fURLs);
    if (! fURLs)
        return NS_ERROR_NULL_POINTER;

    nsTransfer* transfer = (nsTransfer*) PL_HashTableLookup(fURLs, url);

    PR_ASSERT(transfer);
    if (!transfer)
        return NS_ERROR_NULL_POINTER;

    transfer->Suspend();
    
    return NS_OK;
}



NS_IMETHODIMP
nsTopProgressManager::OnResume(const URL_Struct* url)
{
    PR_ASSERT(url);
    if (! url)
        return NS_ERROR_NULL_POINTER;

    TRACE_PROGRESS(("OnResume(%s)\n", url->address));

    PR_ASSERT(fURLs);
    if (! fURLs)
        return NS_ERROR_NULL_POINTER;

    nsTransfer* transfer = (nsTransfer*) PL_HashTableLookup(fURLs, url);

    PR_ASSERT(transfer);
    if (!transfer)
        return NS_ERROR_NULL_POINTER;

    transfer->Resume();
    
    return NS_OK;
}


NS_IMETHODIMP
nsTopProgressManager::OnStopBinding(const URL_Struct* url,
                                    PRInt32 status,
                                    const char* message)
{
    PR_ASSERT(url);
    if (! url)
        return NS_ERROR_NULL_POINTER;

    TRACE_PROGRESS(("OnStatus(%s, %d, %s)\n", url->address, status, message));

    PR_ASSERT(fURLs);
    if (! fURLs)
        return NS_ERROR_NULL_POINTER;

    nsTransfer* transfer = (nsTransfer*) PL_HashTableLookup(fURLs, url);

    PR_ASSERT(transfer);
    if (!transfer)
        return NS_ERROR_NULL_POINTER;

    transfer->MarkComplete(status);
    transfer->SetStatus(message);

    return NS_OK;
}



////////////////////////////////////////////////////////////////////////

void
nsTopProgressManager::TimeoutCallback(void* closure)
{
    nsTopProgressManager* self = (nsTopProgressManager*) closure;
    self->Tick();
}

void
nsTopProgressManager::Tick(void)
{
    TRACE_PROGRESS(("nsProgressManager.Tick: aggregating information for active objects\n"));

    AggregateTransferInfo info = { 0, 0, 0, 0, 0, 0, 0 };
    PL_HashTableEnumerateEntries(fURLs, pm_AggregateTransferInfo, (void*) &info);

    TRACE_PROGRESS(("nsProgressManager.Tick: %ld of %ld objects complete, "
                    "%ldms left, "
                    "%ld of %ld bytes xferred\n",
                    info.CompleteCount, info.ObjectCount,
                    info.MSecRemaining,
                    info.BytesReceived, info.ContentLength));

    PR_ASSERT(info.ObjectCount > 0);
    if (info.ObjectCount == 0)
        return;

    UpdateProgressBar(info);
    UpdateStatusMessage(info);

    // Check to see if we're done.
    if (info.CompleteCount == info.ObjectCount) {
        TRACE_PROGRESS(("Complete: %ld/%ld objects loaded\n",
                        info.CompleteCount,
                        info.ObjectCount));

        // XXX needs to go to allxpstr.h
        FE_Progress(fContext, " ");

        PL_HashTableDestroy(fURLs);
        fURLs = NULL;

        fTimeout = NULL;
    }
    else {
        // Reset the timeout to fire again...
        fTimeout = FE_SetTimeout(nsTopProgressManager::TimeoutCallback,
                                 (void*) this, 500);
    }
}



void
nsTopProgressManager::UpdateProgressBar(AggregateTransferInfo& info)
{
    if (info.MSecRemaining == 0)
        return;

    if (info.SuspendedCount > 0) {
        // turn on the strobe...
        FE_SetProgressBarPercent(fContext, 0);
        return;
    }

    nsInt64 dt = nsTime(PR_Now()) - fProgressBarStart;
    PRUint32 elapsed = dt / nsInt64((PRUint32) PR_USEC_PER_MSEC);
                             
    // Compute the percent complete, that is, elapsed / (elapsed + remaining)
    double p = ((double) elapsed) / ((double) (elapsed + info.MSecRemaining));
    PRUint32 pctComplete = (PRUint32) (100.0 * p);

#define MONOTONIC_PROGRESS_BAR
#if defined(MONOTONIC_PROGRESS_BAR)
    // This hackery is a kludge to make the progress bar
    // monotonically increase rather than slipping backwards as we
    // discover that there's more content to download. It works by
    // adjusting the progress manager's start time backwards to
    // make the elapsed time (time we've waited so far) seem
    // larger in proportion to the amount of time that appears to
    // be left.
    if (pctComplete < fProgress) {
        PRUint32 newElapsed =
            (PRUint32) ((p * ((double) info.MSecRemaining))
                        / (1.0 - p));

        PRInt32 dMSec = newElapsed - elapsed;
        if (dMSec > 0)
            fProgressBarStart -= nsInt64(dMSec * ((PRUint32) PR_USEC_PER_MSEC));

        // Progress bar hasn't changed -- don't bother updating it.
        return;
    }
#endif

    fProgress = pctComplete;
    FE_SetProgressBarPercent(fContext, fProgress);
}

////////////////////////////////////////////////////////////////////////
//
// The bulk of the following code was pulled over from lib/xp/xp_thermo.c
//

#ifdef XP_MAC
#include "allxpstr.h"
#else

PR_BEGIN_EXTERN_C
extern int XP_THERMO_BYTE_FORMAT;
extern int XP_THERMO_KBYTE_FORMAT;
extern int XP_THERMO_HOURS_FORMAT;
extern int XP_THERMO_MINUTES_FORMAT;
extern int XP_THERMO_SECONDS_FORMAT;
extern int XP_THERMO_SINGULAR_FORMAT;
extern int XP_THERMO_PLURAL_FORMAT;
extern int XP_THERMO_PERCENTAGE_FORMAT;
extern int XP_THERMO_UH;
extern int XP_THERMO_PERCENT_FORM;
extern int XP_THERMO_PERCENT_RATE_FORM;
extern int XP_THERMO_RAW_COUNT_FORM;
extern int XP_THERMO_BYTE_RATE_FORMAT;
extern int XP_THERMO_K_RATE_FORMAT;
extern int XP_THERMO_M_RATE_FORMAT;
extern int XP_THERMO_STALLED_FORMAT;
extern int XP_THERMO_RATE_REMAINING_FORM;
extern int XP_THERMO_RATE_FORM;
PR_END_EXTERN_C

#endif // XP_MAC

#define KILOBYTE		(1024L)
#define MINUTE			(60L)
#define HOUR			(MINUTE * MINUTE)

#define IS_PLURAL(x)		(((x) == 1) ? "" : XP_GetString(XP_THERMO_PLURAL_FORMAT)) /* L10N? */

#define	ENOUGH_TIME_TO_GUESS	10 /* in seconds */
#define TIME_UNTIL_DETAILS       5

////////////////////////////////////////////////////////////////////////

static void
formatRate(char* buf, PRUint32 len, double bytes_per_sec)
{
    if (bytes_per_sec > 0) {
        if (bytes_per_sec < KILOBYTE)
            PR_snprintf(buf, len, XP_GetString(XP_THERMO_BYTE_RATE_FORMAT),
                        (PRUint32) bytes_per_sec);
        else
            PR_snprintf(buf, len, XP_GetString(XP_THERMO_K_RATE_FORMAT),
                        (bytes_per_sec / ((double) KILOBYTE)));
    }
}



static void
formatKnownContentLength(char* buf,
                         PRUint32 len,
                         PRUint32 bytesReceived,
                         PRUint32 contentLength,
                         PRUint32 elapsed)
{
    char rate[32];
    *rate = 0;

    // the transfer rate
    double bytes_per_sec = 0;
    if (elapsed > 0)
        bytes_per_sec = ((double) bytesReceived) / ((double) elapsed);

    formatRate(rate, sizeof(rate), bytes_per_sec);
    

    // format the content length
    char length[32];
    *length = 0;

    if (contentLength < KILOBYTE)
        PR_snprintf(length, sizeof(length), XP_GetString(XP_THERMO_BYTE_FORMAT), contentLength);
    else
        PR_snprintf(length, sizeof(length), XP_GetString(XP_THERMO_KBYTE_FORMAT), contentLength / KILOBYTE);
	

    // the percentage complete
    char percent[32];

    PRUint32 p = (bytesReceived * 100) / contentLength;
    if (p >= 100 && bytesReceived != contentLength)
        p = 99;

    PR_snprintf(percent, sizeof(percent), XP_GetString(XP_THERMO_PERCENTAGE_FORMAT), p);

    // the amount of time remaining
    char tleft[32];
    *tleft = 0;

    if (bytes_per_sec >= KILOBYTE && elapsed >= ENOUGH_TIME_TO_GUESS) {
        PRUint32 secs_left =
            (PRUint32) (((double) (contentLength - bytesReceived)) / bytes_per_sec);

        if (secs_left >= HOUR) {
            PR_snprintf(tleft, sizeof(tleft),
                        XP_GetString(XP_THERMO_HOURS_FORMAT),
                        secs_left / HOUR,
                        (secs_left / MINUTE) % MINUTE,
                        secs_left % MINUTE);
        }
        else if (secs_left >= MINUTE) {
            PR_snprintf(tleft, sizeof(tleft),
                        XP_GetString(XP_THERMO_MINUTES_FORMAT),
                        secs_left / MINUTE,
                        secs_left % MINUTE);
        }
        else if (secs_left > 0) {
            PR_snprintf(tleft, sizeof(tleft),
                        XP_GetString(XP_THERMO_SECONDS_FORMAT),
                        secs_left,
                        IS_PLURAL(secs_left));
        }
    }

    if (*tleft) {
        /* "%s of %s (at %s, %s remaining)" */
        PR_snprintf(buf, len,
                    XP_GetString(XP_THERMO_RATE_REMAINING_FORM),
                    percent, length, rate, tleft);
    }
    else if (*rate) {
        /* "%s of %s (at %s)" */
        PR_snprintf(buf, len,
                    XP_GetString(XP_THERMO_RATE_FORM),
                    percent, length, rate);
    }
    else {
        /* "%s of %s" */
        PR_snprintf(buf, len,
                    XP_GetString(XP_THERMO_PERCENT_FORM),
                    percent, length);
    }
}


static void
formatUnknownContentLength(char* buf, PRUint32 len, PRUint32 bytesReceived, PRUint32 elapsed)
{
    char rate[32];
    *rate = 0;

    // the transfer rate
    double bytes_per_sec = 0;
    if (elapsed > 0)
        bytes_per_sec = ((double) bytesReceived) / ((double) elapsed);

    formatRate(rate, sizeof(rate), bytes_per_sec);

    // the number of bytes received
    char bytes_received[32];

    if (bytesReceived < KILOBYTE)
        PR_snprintf(bytes_received, sizeof(bytes_received),
                    XP_GetString(XP_THERMO_UH),
                    bytesReceived, IS_PLURAL(bytesReceived));
    else
        PR_snprintf(bytes_received, sizeof(bytes_received),
                    XP_GetString(XP_THERMO_KBYTE_FORMAT),
                    bytesReceived / KILOBYTE);

    if (*rate) {
        /* "%s read (at %s)" */
        PR_snprintf(buf, len, XP_GetString(XP_THERMO_PERCENT_RATE_FORM), bytes_received, rate);
    }
    else {
        PR_snprintf(buf, len, XP_GetString(XP_THERMO_RAW_COUNT_FORM), bytes_received);
    }
}


void
nsTopProgressManager::UpdateStatusMessage(AggregateTransferInfo& info)
{
    // Compute how much time has elapsed
    nsInt64 dt = nsTime(PR_Now()) - fActualStart;
    PRUint32 elapsed = dt / nsInt64((PRUint32) PR_USEC_PER_SEC);

    char buf[256];
    *buf = 0;

    if (info.ObjectCount == 1 || info.CompleteCount == 0) {
        // If we only have one object that we're transferring, or if
        // nothing has completed yet, show the default status message
        PL_strncpy(buf, fDefaultStatus, sizeof(buf));
    }

    if (elapsed > TIME_UNTIL_DETAILS) {
        char details[256];
        *details = 0;

        if (!info.UnknownLengthCount && info.ContentLength > 0) {
            formatKnownContentLength(details, sizeof(details),
                                     info.BytesReceived,
                                     info.ContentLength,
                                     elapsed);
        }
        else if (info.BytesReceived > 0) {
            formatUnknownContentLength(details, sizeof(details),
                                       info.BytesReceived,
                                       elapsed);
        }

        if (*details) {
            // XXX needs to go to allxpstr.h
            if (*buf)
                PL_strcatn(buf, sizeof(buf), ", ");

            PL_strcatn(buf, sizeof(buf), details);
        }
    }

    FE_Progress(fContext, buf);
}
