/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#define out PR_LOG_ALWAYS


/* Used by NET_NTrace() */
PRIVATE void net_Trace(char *msg) {
#if defined(XP_WIN32) || defined(XP_OS2)
	if(MKLib_trace_flag) {
		FE_Trace(msg);
		FE_Trace("\n");
	}
#endif
	PR_LOG(NETLIB, out, (msg));
}


/* Called to trace a message */
void NET_NTrace(char *msg, int32 length) {
	char * new_string = XP_ALLOC(length+1);
	if(!new_string)
		return;
	strncpy(new_string, msg, length);
	new_string[length] = '\0';

#ifdef XP_WIN
	/* AFX can't handle anystring larger than 512 bytes */
	if(length > 511)
		new_string[511] = '\0';
#endif

	net_Trace(new_string);
	FREE(new_string);
}


/* #define'd in mktrace.h to TRACEMSG */
void _MK_TraceMsg(char *fmt, ...) {
	va_list ap;
	char buf[512];

	va_start(ap, fmt);
	PR_vsnprintf(buf, sizeof(buf), fmt, ap);

#if defined(DEBUG) && defined(XP_WIN)
	if(MKLib_trace_flag) {
		FE_Trace(buf);
		FE_Trace("\n");
	}
#else
	PR_LOG(NETLIB, out, (buf));
#endif
}

#endif /* DEBUG || NETLIB_TRACE_ON */
