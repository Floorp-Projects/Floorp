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

XP_BEGIN_PROTOS
extern void NET_NTrace(char *msg, int32 length);
extern void _MK_TraceMsg(char *fmt, ...);
XP_END_PROTOS

#define TRACEMSG(msg)  _MK_TraceMsg msg
#else
#define TRACEMSG(msg)  
#endif /* DEBUG || NETLIB_TRACE_ON */

#endif /* MKTRACE_H */
