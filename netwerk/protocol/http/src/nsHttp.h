/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#ifndef nsHttp_h__
#define nsHttp_h__

#include "plstr.h"
#include "prlog.h"
#include "prtime.h"
#include "nsISupportsUtils.h"

#if defined(PR_LOGGING)
//
// Log module for HTTP Protocol logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsHttp:5
//    set NSPR_LOG_FILE=http.log
//
// this enables PR_LOG_ALWAYS level information and places all output in
// the file http.log
//
extern PRLogModuleInfo *gHttpLog;
#endif

// http logging
#define LOG1(args) PR_LOG(gHttpLog, 1, args)
#define LOG2(args) PR_LOG(gHttpLog, 2, args)
#define LOG3(args) PR_LOG(gHttpLog, 3, args)
#define LOG4(args) PR_LOG(gHttpLog, 4, args)
#define LOG(args) LOG4(args)

// http default buffer geometry
#define NS_HTTP_SEGMENT_SIZE 4096
#define NS_HTTP_BUFFER_SIZE  4096*4 // 16k maximum

// http version codes
#define NS_HTTP_VERSION_UNKNOWN  0
#define NS_HTTP_VERSION_0_9      9
#define NS_HTTP_VERSION_1_0     10
#define NS_HTTP_VERSION_1_1     11

typedef PRUint8 nsHttpVersion;

// http connection capabilities
#define NS_HTTP_ALLOW_KEEPALIVE  (1<<0)
#define NS_HTTP_ALLOW_PIPELINING (1<<1)

//-----------------------------------------------------------------------------
// http atoms...
//-----------------------------------------------------------------------------

struct nsHttpAtom
{
    operator const char *() { return _val; }
    const char *get() { return _val; }

    void operator=(const char *v) { _val = v; }
    void operator=(const nsHttpAtom &a) { _val = a._val; }

    // private
    const char *_val;
};

struct nsHttp
{
    static void DestroyAtomTable();

    // will dynamically add atoms to the table if they don't already exist
    static nsHttpAtom ResolveAtom(const char *);

    /* Declare all atoms
     *
     * The atom names and values are stored in nsHttpAtomList.h and
     * are brought to you by the magic of C preprocessing
     *
     * Add new atoms to nsHttpAtomList and all support logic will be auto-generated
     */
#define HTTP_ATOM(_name, _value) static nsHttpAtom _name;
#include "nsHttpAtomList.h"
#undef HTTP_ATOM
};

//-----------------------------------------------------------------------------
// utilities...
//-----------------------------------------------------------------------------

static inline nsresult
DupString(const char *src, char **dst)
{
    NS_ENSURE_ARG_POINTER(dst);
    *dst = PL_strdup(src);
    return *dst ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

static inline PRUint32
PRTimeToSeconds(PRTime t_usec)
{
    PRTime usec_per_sec;
    PRUint32 t_sec;
    LL_I2L(usec_per_sec, PR_USEC_PER_SEC);
    LL_DIV(t_usec, t_usec, usec_per_sec);
    LL_L2I(t_sec, t_usec);
    return t_sec;
}

#define NowInSeconds() PRTimeToSeconds(PR_Now())

// ripped from glib.h
#undef  CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#endif // nsHttp_h__
