/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#define FORCE_PR_LOG /* So that this works in an optimized build */

#include "mkutils.h"
#include "mktrace.h"
#include "timing.h"
#include "prprf.h"
#include "plstr.h"
#include "prlog.h"
#include "prsystem.h"
#include "prtime.h"
#include "plhash.h"

/* If you want to trace netlib, set this to 1, or use CTRL-ALT-T
 * stroke (preferred method) to toggle it on and off */
int MKLib_trace_flag=0;

PUBLIC void NET_ToggleTrace(void) {
    if(MKLib_trace_flag)
		MKLib_trace_flag = 0;
	else
		MKLib_trace_flag = 1;	
}

/* Map netlib trace messages into nspr logger so that they are
 * thread safe. */
#if defined(DEBUG) || defined(NETLIB_TRACE_ON)

PRLogModuleInfo* NETLIB=NULL;

/* Used by NET_NTrace() */
PRIVATE void net_Trace(char *msg) {
	/* Only trace if the user explicitly told us to. */
	if(MKLib_trace_flag) {
        /* PR_LogPrint prints to stderr which doesn't exist in a windows app.
         * Use the win32 routine OutputDebugString to get text to goto the 
         * debug console. */
#if defined(WIN32) && defined(DEBUG)
		OutputDebugString(msg);
		OutputDebugString("\n");
#else
        PR_LogPrint(msg);
#endif        
    } else {
		PR_LOG(NETLIB, PR_LOG_ALWAYS, (msg));
	}
}


/* Called to trace a message */
void NET_NTrace(char *msg, int32 length) {
	char * new_string = PR_Malloc(length+1);
	if(!new_string)
		return;
	strncpy(new_string, msg, length);
	new_string[length] = '\0';

	net_Trace(new_string);
	FREE(new_string);
}


/* #define'd in mktrace.h to TRACEMSG */
void _MK_TraceMsg(char *fmt, ...) {
	va_list ap;
	char buf[512];

	va_start(ap, fmt);
	PR_vsnprintf(buf, sizeof(buf), fmt, ap);

    net_Trace(buf);
}

#endif /* DEBUG || NETLIB_TRACE_ON */


/************************************************
 *
 *  Runtime performance tracing stubs.
 *
 */


static PRLogModuleInfo* gTimingLog   = NULL;
static const char* gTimingModuleName = "nsTiming";

/**
 * Rev this if the output format changes significantly
 */
#define TIMING_VERSION 0x10000

/**
 * Ensure that the log module actually exists before trying to use it.
 * @return <tt>FALSE</tt> if something goes wrong.
 */
static PRBool
EnsureLogModule(void)
{
    if (gTimingLog == NULL) {
        if ((gTimingLog = PR_NewLogModule(gTimingModuleName)) == NULL)
            return PR_FALSE;

        /* _On_ to start with -- if the env is set up */
        gTimingLog->level = PR_LOG_NOTICE;

        {
            /* Dump the session info */
            char hostname[SYS_INFO_BUFFER_LENGTH];
            char sysname[SYS_INFO_BUFFER_LENGTH];
            char release[SYS_INFO_BUFFER_LENGTH];
            char arch[SYS_INFO_BUFFER_LENGTH];

            if (PR_GetSystemInfo(PR_SI_HOSTNAME, hostname, sizeof(hostname)) != PR_SUCCESS)
                hostname[0] = '\0';

            if (PR_GetSystemInfo(PR_SI_SYSNAME, sysname, sizeof(sysname)) != PR_SUCCESS)
                sysname[0] = '\0';

            /* PR_SI_RELEASE only works for unix and it will still return success. */
#ifdef XP_UNIX
            if (PR_GetSystemInfo(PR_SI_RELEASE, release, sizeof(release)) != PR_SUCCESS)
                release[0] = '\0';
#else
            release[0] = '\0';
#endif

            if (PR_GetSystemInfo(PR_SI_ARCHITECTURE, arch, sizeof(arch)) != PR_SUCCESS)
                arch[0] = '\0';

            TimingWriteMessage("begin-session,%08x,%s,%s,%s,%s,%s,%ld",
                               TIMING_VERSION, hostname, sysname,
                               release, arch,
#ifdef DEBUG
                               "debug",
#else
                               "opt",
#endif
                               0 /* XXX build number */
                               );

        }
    }

    return (gTimingLog != NULL) ? PR_TRUE : PR_FALSE;
}

/**
 * Write a message to the log. You should use the TIMING_MSG() macro,
 * rather than calling this directly.
 */
PR_IMPLEMENT(void)
TimingWriteMessage(const char* fmtstr, ...)
{
    char line[256];
    va_list ap;
    PRUint32 nb;

    if (! EnsureLogModule())
        return;

    if (gTimingLog->level == PR_LOG_NONE)
        return;

    {
        PRExplodedTime now;
        PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &now);

        /* Print out "YYYYMMDD.HHMMSS.UUUUUU: " */
        nb = PR_snprintf(line, sizeof(line) - 1,
                         "%04d%02d%02d.%02d%02d%02d.%06d: ",
                         now.tm_year, now.tm_month + 1, now.tm_mday,
                         now.tm_hour, now.tm_min, now.tm_sec,
                         now.tm_usec);
    }

    /* ...followed by the "real" message */
    va_start(ap, fmtstr);
    nb += PR_vsnprintf(line + nb, sizeof(line) - nb - 1, fmtstr, ap);

    PR_LOG(gTimingLog, PR_LOG_NOTICE, ("%s", line));
}


/**
 * Enable or disable the timing log.
 */
PR_IMPLEMENT(void)
TimingSetEnabled(PRBool enabled)
{
    if (! EnsureLogModule())
        return;

    if (enabled) {
        if (gTimingLog->level == PR_LOG_NONE) {
            gTimingLog->level = PR_LOG_NOTICE;
            TimingWriteMessage("(tracing enabled)");
        }
    } else {
        if (gTimingLog->level != PR_LOG_NONE) {
            TimingWriteMessage("(tracing disabled)");
            gTimingLog->level = PR_LOG_NONE;
        }
    }
}

/**
 * Query whether the timing log is enabled.
 */
PR_IMPLEMENT(PRBool)
TimingIsEnabled(void)
{
    if (! EnsureLogModule())
        return PR_FALSE;

    return (gTimingLog->level == PR_LOG_NONE) ? PR_FALSE : PR_TRUE;
}



/*************************************************************************
 *
 * Clocking functions
 *
 */

/*
 * Allocation routines
 */

static PR_CALLBACK void*
_timingClockAllocTable(void* pool, PRSize size)
{
    return PR_MALLOC(size);
}

static PR_CALLBACK void
_timingClockFreeTable(void* pool, void* item)
{
    PR_DELETE(item);
}

static PR_CALLBACK PLHashEntry*
_timingClockAllocEntry(void* pool, const void* key)
{
    return PR_NEW(PLHashEntry);
}

static PR_CALLBACK void
_timingClockFreeEntry(void* pool, PLHashEntry* he, PRUintn flag)
{
    if (flag == HT_FREE_VALUE) {
        if (he->value)
            PR_DELETE(he->value);
    }
    else if (flag == HT_FREE_ENTRY) {
        if (he->key)
            PL_strfree((char*) he->key);
        if (he->value)
            PR_DELETE(he->value);
        PR_DELETE(he);
    }
}


static PLHashAllocOps _timingClockAllocOps = {
    _timingClockAllocTable,
    _timingClockFreeTable,
    _timingClockAllocEntry,
    _timingClockFreeEntry
};


/**
 * The initial size of the clock table
 */
#define CLOCK_TABLE_INIT_SIZE 13


/**
 * The clock table.
 */
static PLHashTable *_timingClockTable = NULL;


/**
 * Ensure that the clock table exists and is initialized.
 */
static PRBool
EnsureTimingTable()
{
    if (! _timingClockTable) {
        _timingClockTable =
            PL_NewHashTable(CLOCK_TABLE_INIT_SIZE,
                            PL_HashString,
                            PL_CompareStrings,
                            PL_CompareValues,
                            &_timingClockAllocOps,
                            NULL);
    }

    return (_timingClockTable != NULL);
}


/**
 * Start a clock
 */
PR_IMPLEMENT(void)
TimingStartClock(const char* clock)
{
    char* key = NULL;
    PRTime* value = NULL;

    if (! TimingIsEnabled())
        return;

    if (! EnsureTimingTable())
        return;

    if (PL_HashTableLookup(_timingClockTable, clock) != NULL)
        /* The clock is already running... */
        return;

    if ((key = PL_strdup(clock)) == NULL)
        goto error;

    if ((value = PR_NEW(PRTime)) == NULL)
        goto error;

    *value = PR_Now();
    PL_HashTableAdd(_timingClockTable, key, value);
    return;

error:
    if (key)
        PL_strfree(key);
    PR_DELETE(value);
}


PR_EXTERN(PRBool)
TimingStopClock(PRTime* result, const char* clock)
{
    PRTime* start;
    PRTime  stop = PR_Now();

    if (! TimingIsEnabled())
        return PR_FALSE;

    if (! EnsureTimingTable())
        return PR_FALSE;

    if ((start = PL_HashTableLookup(_timingClockTable, clock)) != NULL) {
        if (result != NULL)
            LL_SUB(*result, stop, *start);
        PL_HashTableRemove(_timingClockTable, clock);
        return PR_TRUE;
    } else {
        *result = LL_ZERO;
        return PR_FALSE;
    }
}


PR_EXTERN(PRBool)
TimingIsClockRunning(const char* clock)
{
     if (! TimingIsEnabled())
        return PR_FALSE;

    if (! EnsureTimingTable())
        return PR_FALSE;

    if (PL_HashTableLookup(_timingClockTable, clock) != NULL) {
        return PR_TRUE;
    } else {
        return PR_FALSE;
    }
}


PR_EXTERN(char*)
TimingElapsedTimeToString(PRTime time, char* buffer, PRUint32 size)
{
    PRTime tmUSec;
    PRTime tmSec;
    PRTime tmMin;

    {
        PRTime n;
        LL_UI2L(n, 1000000L);
        LL_MOD(tmUSec, time, n);
        LL_DIV(time, time, n);
    }

    {
        PRTime n;
        LL_UI2L(n, 60L);
        LL_MOD(tmSec, time, n);
        LL_DIV(time, time, n);

        LL_MOD(tmMin, time, n);
        LL_DIV(time, time, n);
    }

    {
        PRUint32 nHours;
        PRUint32 nMin;
        PRUint32 nSec;
        PRUint32 nUSec;

        LL_L2UI(nHours, time);
        LL_L2UI(nMin,   tmMin);
        LL_L2UI(nSec,   tmSec);
        LL_L2UI(nUSec,  tmUSec);

        /* Print out "HHMMSS.UUUUUU" */
        PR_snprintf(buffer, size, "%02d%02d%02d.%06d",
                    nHours, nMin, nSec, nUSec);
    }

    return buffer;
}
