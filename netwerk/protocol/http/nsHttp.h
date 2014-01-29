/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttp_h__
#define nsHttp_h__

#include <stdint.h>
#include "prtime.h"
#include "nsString.h"
#include "nsError.h"

// http version codes
#define NS_HTTP_VERSION_UNKNOWN  0
#define NS_HTTP_VERSION_0_9      9
#define NS_HTTP_VERSION_1_0     10
#define NS_HTTP_VERSION_1_1     11
#define NS_HTTP_VERSION_2_0     20

namespace mozilla {

class Mutex;

namespace net {
    enum {
        SPDY_VERSION_2_REMOVED = 2,
        SPDY_VERSION_3 = 3,
        SPDY_VERSION_31 = 4,

        // leave room for official versions. telem goes to 48
        // 24 was a internal spdy/3.1
        // 25 was spdy/4a2
        // 26 was http/2-draft08 and http/2-draft07 (they were the same)
        HTTP2_VERSION_DRAFT09 = 27
    };

typedef uint8_t nsHttpVersion;

#define NS_HTTP2_DRAFT_VERSION HTTP2_VERSION_DRAFT09
#define NS_HTTP2_DRAFT_TOKEN "HTTP-draft-09/2.0"

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

// a transaction with this flag blocks the initiation of other transactons
// in the same load group until it is complete
#define NS_HTTP_LOAD_AS_BLOCKING     (1<<6)

// Disallow the use of the SPDY protocol. This is meant for the contexts
// such as HTTP upgrade which are nonsensical for SPDY, it is not the
// SPDY configuration variable.
#define NS_HTTP_DISALLOW_SPDY        (1<<7)

// a transaction with this flag loads without respect to whether the load
// group is currently blocking on some resources
#define NS_HTTP_LOAD_UNBLOCKED       (1<<8)

// These flags allow a transaction to use TLS false start with
// weaker security profiles based on past history
#define NS_HTTP_ALLOW_RSA_FALSESTART (1<<9)

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
    static Mutex *GetLock();

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
    // 64-bit integer.  If the value would exceed INT64_MAX, then false is
    // returned.  Otherwise, this function returns true and stores the
    // parsed value in |result|.  The next unparsed character in |input| is
    // optionally returned via |next| if |next| is non-null.
    //
    // TODO(darin): Replace this with something generic.
    //
    static bool ParseInt64(const char *input, const char **next,
                             int64_t *result);

    // Variant on ParseInt64 that expects the input string to contain nothing
    // more than the value being parsed.
    static inline bool ParseInt64(const char *input, int64_t *result) {
        const char *next;
        return ParseInt64(input, &next, result) && *next == '\0';
    }

    // Return whether the HTTP status code represents a permanent redirect
    static bool IsPermanentRedirect(uint32_t httpStatus);

    // Return whether upon a redirect code of httpStatus for method, the
    // request method should be rewritten to GET.
    static bool ShouldRewriteRedirectToGET(uint32_t httpStatus, nsHttpAtom method);

    // Return whether the specified method is safe as per RFC 2616,
    // Section 9.1.1.
    static bool IsSafeMethod(nsHttpAtom method);

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

static inline uint32_t
PRTimeToSeconds(PRTime t_usec)
{
    return uint32_t( t_usec / PR_USEC_PER_SEC );
}

#define NowInSeconds() PRTimeToSeconds(PR_Now())

// Round q-value to 2 decimal places; return 2 most significant digits as uint.
#define QVAL_TO_UINT(q) ((unsigned int) ((q + 0.005) * 100.0))

#define HTTP_LWS " \t"
#define HTTP_HEADER_VALUE_SEPS HTTP_LWS ","

} // namespace mozilla::net
} // namespace mozilla

#endif // nsHttp_h__
