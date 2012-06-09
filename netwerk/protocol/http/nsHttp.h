/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttp_h__
#define nsHttp_h__

#if defined(MOZ_LOGGING)
#define FORCE_PR_LOG
#endif

// e10s mess: IPDL-generatd headers include chromium which both #includes
// prlog.h, and #defines LOG in conflict with this file.
// Solution: (as described in bug 545995)
// 1) ensure that this file is #included before any IPDL-generated files and
//    anything else that #includes prlog.h, so that we can make sure prlog.h
//    sees FORCE_PR_LOG if needed.
// 2) #include IPDL boilerplate, and then undef LOG so our LOG wins.
// 3) nsNetModule.cpp does its own crazy stuff with #including prlog.h
//    multiple times; allow it to define ALLOW_LATE_NSHTTP_H_INCLUDE to bypass
//    check.
#if defined(PR_LOG) && !defined(ALLOW_LATE_NSHTTP_H_INCLUDE)
#error "If nsHttp.h #included it must come before any IPDL-generated files or other files that #include prlog.h"
#endif
#include "mozilla/net/NeckoChild.h"
#undef LOG

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

// a transaction with this caps flag will, upon opening a new connection,
// bypass the local DNS cache
#define NS_HTTP_REFRESH_DNS          (1<<3)

// a transaction with this caps flag will not pass SSL client-certificates
// to the server (see bug #466080), but is may also be used for other things
#define NS_HTTP_LOAD_ANONYMOUS       (1<<4)

// a transaction with this caps flag keeps timing information
#define NS_HTTP_TIMING_ENABLED       (1<<5)

// a transaction with this caps flag will not only not use an existing
// persistent connection but it will close outstanding ones to the same
// host. Used by a forced reload to reset the connection states.
#define NS_HTTP_CLEAR_KEEPALIVES     (1<<6)

// Disallow the use of the SPDY protocol. This is meant for the contexts
// such as HTTP upgrade which are nonsensical for SPDY, it is not the
// SPDY configuration variable.
#define NS_HTTP_DISALLOW_SPDY        (1<<7)

//-----------------------------------------------------------------------------
// some default values
//-----------------------------------------------------------------------------

#define NS_HTTP_DEFAULT_PORT  80
#define NS_HTTPS_DEFAULT_PORT 443

#define NS_HTTP_HEADER_SEPS ", \t"

//-----------------------------------------------------------------------------
// http atoms...
//-----------------------------------------------------------------------------

struct nsHttpAtom
{
    operator const char *() const { return _val; }
    const char *get() const { return _val; }

    void operator=(const char *v) { _val = v; }
    void operator=(const nsHttpAtom &a) { _val = a._val; }

    // private
    const char *_val;
};

struct nsHttp
{
    static nsresult CreateAtomTable();
    static void DestroyAtomTable();

    // The mutex is valid any time the Atom Table is valid
    // This mutex is used in the unusual case that the network thread and
    // main thread might access the same data
    static mozilla::Mutex *GetLock();

    // will dynamically add atoms to the table if they don't already exist
    static nsHttpAtom ResolveAtom(const char *);
    static nsHttpAtom ResolveAtom(const nsACString &s)
    {
        return ResolveAtom(PromiseFlatCString(s).get());
    }

    // returns true if the specified token [start,end) is valid per RFC 2616
    // section 2.2
    static bool IsValidToken(const char *start, const char *end);

    static inline bool IsValidToken(const nsCString &s) {
        const char *start = s.get();
        return IsValidToken(start, start + s.Length());
    }

    // find the first instance (case-insensitive comparison) of the given
    // |token| in the |input| string.  the |token| is bounded by elements of
    // |separators| and may appear at the beginning or end of the |input|
    // string.  null is returned if the |token| is not found.  |input| may be
    // null, in which case null is returned.
    static const char *FindToken(const char *input, const char *token,
                                 const char *separators);

    // This function parses a string containing a decimal-valued, non-negative
    // 64-bit integer.  If the value would exceed LL_MAXINT, then false is
    // returned.  Otherwise, this function returns true and stores the
    // parsed value in |result|.  The next unparsed character in |input| is
    // optionally returned via |next| if |next| is non-null.
    //
    // TODO(darin): Replace this with something generic.
    //
    static bool ParseInt64(const char *input, const char **next,
                             PRInt64 *result);

    // Variant on ParseInt64 that expects the input string to contain nothing
    // more than the value being parsed.
    static inline bool ParseInt64(const char *input, PRInt64 *result) {
        const char *next;
        return ParseInt64(input, &next, result) && *next == '\0';
    }

    // Declare all atoms
    //
    // The atom names and values are stored in nsHttpAtomList.h and are brought
    // to you by the magic of C preprocessing.  Add new atoms to nsHttpAtomList
    // and all support logic will be auto-generated.
    //
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
    return PRUint32( t_usec / PR_USEC_PER_SEC );
}

#define NowInSeconds() PRTimeToSeconds(PR_Now())

// round q-value to one decimal place; return most significant digit as uint.
#define QVAL_TO_UINT(q) ((unsigned int) ((q + 0.0005) * 1000.0))

#define HTTP_LWS " \t"
#define HTTP_HEADER_VALUE_SEPS HTTP_LWS ","

#endif // nsHttp_h__
