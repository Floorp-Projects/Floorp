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
#include "mkutils.h"
#include "mktrace.h"
#include "timing.h"
#include "prprf.h"
#include "prlog.h"
#include "prtime.h"

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
 * Ensure that the log module actually exists before trying to use it.
 * @return <tt>FALSE</tt> if something goes wrong.
 */
static PRBool
EnsureLogModule(void)
{
    if (gTimingLog == NULL) {
        if ((gTimingLog = PR_NewLogModule(gTimingModuleName)) != NULL) {
            /* Off to start with */
            gTimingLog->level = PR_LOG_NONE;
        }
    }

    return (gTimingLog != NULL) ? PR_TRUE : PR_FALSE;
}

/**
 * Write a message to the log. You should use the TIMING_MSG() macro,
 * rather than calling this directly.
 */
PUBLIC void
TimingWriteMessage(const char* fmtstr, ...)
{
    char line[256];
    va_list ap;

    if (! EnsureLogModule())
        return;

    if (gTimingLog->level == PR_LOG_NONE)
        return;

    va_start(ap, fmtstr);

    {
        PRExplodedTime now;
        PRUint32 nb;

        PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &now);

        /* Print out "YYYYMMDD.HHMMSS.UUUUUU: " */
        nb = PR_snprintf(line, sizeof(line) - 1,
                         "%04d%02d%02d.%02d%02d%02d.%06d: ",
                         now.tm_year, now.tm_month + 1, now.tm_mday,
                         now.tm_hour, now.tm_min, now.tm_sec,
                         now.tm_usec);

        /* ...followed by the "real" message */
        nb += PR_vsnprintf(line + nb, sizeof(line) - nb - 1, fmtstr, ap);
    }

    PR_LOG(gTimingLog, PR_LOG_NOTICE, (line));
}


/**
 * Enable or disable the timing log.
 */
PUBLIC void
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
PUBLIC PRBool
TimingIsEnabled(void)
{
    if (! EnsureLogModule())
        return PR_FALSE;

    return (gTimingLog->level == PR_LOG_NONE) ? PR_FALSE : PR_TRUE;
}

