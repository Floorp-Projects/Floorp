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

#if defined(MOZ_LOGGING)
#define FORCE_PR_LOG
#endif

#include "plstr.h"
#include "prlog.h"
#include "prtime.h"
#include "nsISupportsUtils.h"
#include "nsPromiseFlatString.h"
#include "nsURLHelper.h"
#include "netCore.h"

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

#define LOG1_ENABLED() PR_LOG_TEST(gHttpLog, 1)
#define LOG2_ENABLED() PR_LOG_TEST(gHttpLog, 2)
#define LOG3_ENABLED() PR_LOG_TEST(gHttpLog, 3)
#define LOG4_ENABLED() PR_LOG_TEST(gHttpLog, 4)
#define LOG_ENABLED() LOG4_ENABLED()

// http default buffer geometry
#define NS_HTTP_SEGMENT_SIZE  4096
#define NS_HTTP_SEGMENT_COUNT 16   // 64k maximum
#define NS_HTTP_MAX_ODA_SIZE  (NS_HTTP_SEGMENT_SIZE * 4) // 16k

// http version codes
#define NS_HTTP_VERSION_UNKNOWN  0
#define NS_HTTP_VERSION_0_9      9
#define NS_HTTP_VERSION_1_0     10
#define NS_HTTP_VERSION_1_1     11

typedef PRUint8 nsHttpVersion;

//-----------------------------------------------------------------------------
// http connection capabilities
//-----------------------------------------------------------------------------

#define NS_HTTP_ALLOW_KEEPALIVE      (1<<0)
#define NS_HTTP_ALLOW_PIPELINING     (1<<1)

// a transaction with this caps flag will continue to own the connection,
// preventing it from being reclaimed, even after the transaction completes.
#define NS_HTTP_STICKY_CONNECTION    (1<<2)

//-----------------------------------------------------------------------------
// some default values
//-----------------------------------------------------------------------------

// hard upper limit on the number of requests that can be pipelined
#define NS_HTTP_MAX_PIPELINED_REQUESTS 8 

#define NS_HTTP_DEFAULT_PORT  80
#define NS_HTTPS_DEFAULT_PORT 443

#define NS_HTTP_HEADER_SEPS ", \t"

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
    static nsHttpAtom ResolveAtom(const nsACString &s) { return ResolveAtom(PromiseFlatCString(s).get()); }

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

// nsCRT::strdup likes to convert nsnull to "" 
#define strdup_if(s) (s ? nsCRT::strdup(s) : nsnull)

// round q-value to one decimal place; return most significant digit as uint.
#define QVAL_TO_UINT(q) ((unsigned int) ((q + 0.05) * 10.0))

#define HTTP_LWS " \t"

#endif // nsHttp_h__
