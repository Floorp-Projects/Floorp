/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef MKTRACE_H
#define MKTRACE_H

extern int MKLib_trace_flag;

extern void NET_ToggleTrace(void);

/* define NETLIB_TRACE_ON to trace netlib in optimized build */

#if defined(DEBUG) || defined(NETLIB_TRACE_ON)

extern PRLogModuleInfo* NETLIB;

#ifdef TRACEMSG
#undef TRACEMSG
#endif

PR_BEGIN_EXTERN_C
extern void NET_NTrace(char *msg, int32 length);
extern void ns_MK_TraceMsg(char *fmt, ...);
PR_END_EXTERN_C

#define TRACEMSG(msg)  ns_MK_TraceMsg msg
#else
#define TRACEMSG(msg)  
#endif /* DEBUG || NETLIB_TRACE_ON */

#endif /* MKTRACE_H */
