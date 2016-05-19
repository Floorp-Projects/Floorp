/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCMessageUtils.h"

#include "nsStandardURL.h"
#include "nsCRT.h"
#include "nsEscape.h"
#include "nsIFile.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIIDNService.h"
#include "mozilla/Logging.h"
#include "nsAutoPtr.h"
#include "nsIURLParser.h"
#include "nsNetCID.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ipc/URIUtils.h"
#include <algorithm>
#include "mozilla/dom/EncodingUtils.h"
#include "nsContentUtils.h"

using mozilla::dom::EncodingUtils;
using namespace mozilla::ipc;

static NS_DEFINE_CID(kThisImplCID, NS_THIS_STANDARDURL_IMPL_CID);
static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);

nsIIDNService *nsStandardURL::gIDN = nullptr;
bool nsStandardURL::gInitialized = false;
bool nsStandardURL::gEscapeUTF8 = true;
bool nsStandardURL::gAlwaysEncodeInUTF8 = true;
char nsStandardURL::gHostLimitDigits[] = { '/', '\\', '?', '#', 0 };

//
// setenv NSPR_LOG_MODULES nsStandardURL:5
//
static mozilla::LazyLogModule gStandardURLLog("nsStandardURL");

// The Chromium code defines its own LOG macro which we don't want
#undef LOG
#define LOG(args)     MOZ_LOG(gStandardURLLog, mozilla::LogLevel::Debug, args)
#undef LOG_ENABLED
#define LOG_ENABLED() MOZ_LOG_TEST(gStandardURLLog, mozilla::LogLevel::Debug)

//----------------------------------------------------------------------------

#define ENSURE_MUTABLE() \
  PR_BEGIN_MACRO \
    if (!mMutable) { \
        NS_WARNING("attempt to modify an immutable nsStandardURL"); \
        return NS_ERROR_ABORT; \
    } \
  PR_END_MACRO

//----------------------------------------------------------------------------

static nsresult
EncodeString(nsIUnicodeEncoder *encoder, const nsAFlatString &str, nsACString &result)
{
    nsresult rv;
    int32_t len = str.Length();
    int32_t maxlen;

    rv = encoder->GetMaxLength(str.get(), len, &maxlen);
    if (NS_FAILED(rv))
        return rv;

    char buf[256], *p = buf;
    if (uint32_t(maxlen) > sizeof(buf) - 1) {
        p = (char *) malloc(maxlen + 1);
        if (!p)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = encoder->Convert(str.get(), &len, p, &maxlen);
    if (NS_FAILED(rv))
        goto end;
    if (rv == NS_ERROR_UENC_NOMAPPING) {
        NS_WARNING("unicode conversion failed");
        rv = NS_ERROR_UNEXPECTED;
        goto end;
    }
    p[maxlen] = 0;
    result.Assign(p);

    len = sizeof(buf) - 1;
    rv = encoder->Finish(buf, &len);
    if (NS_FAILED(rv))
        goto end;
    buf[len] = 0;
    result.Append(buf);

end:
    encoder->Reset();

    if (p != buf)
        free(p);
    return rv;
}

//----------------------------------------------------------------------------
// nsStandardURL::nsPrefObserver
//----------------------------------------------------------------------------

#define NS_NET_PREF_ESCAPEUTF8         "network.standard-url.escape-utf8"
#define NS_NET_PREF_ALWAYSENCODEINUTF8 "network.standard-url.encode-utf8"

NS_IMPL_ISUPPORTS(nsStandardURL::nsPrefObserver, nsIObserver)

NS_IMETHODIMP nsStandardURL::
nsPrefObserver::Observe(nsISupports *subject,
                        const char *topic,
                        const char16_t *data)
{
    if (!strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
        nsCOMPtr<nsIPrefBranch> prefBranch( do_QueryInterface(subject) );
        if (prefBranch) {
            PrefsChanged(prefBranch, NS_ConvertUTF16toUTF8(data).get());
        } 
    }
    return NS_OK;
}

//----------------------------------------------------------------------------
// nsStandardURL::nsSegmentEncoder
//----------------------------------------------------------------------------

nsStandardURL::
nsSegmentEncoder::nsSegmentEncoder(const char *charset)
    : mCharset(charset)
{
}

int32_t nsStandardURL::
nsSegmentEncoder::EncodeSegmentCount(const char *str,
                                     const URLSegment &seg,
                                     int16_t mask,
                                     nsAFlatCString &result,
                                     bool &appended,
                                     uint32_t extraLen)
{
    // extraLen is characters outside the segment that will be 
    // added when the segment is not empty (like the @ following
    // a username).
    appended = false;
    if (!str)
        return 0;
    int32_t len = 0;
    if (seg.mLen > 0) {
        uint32_t pos = seg.mPos;
        len = seg.mLen;

        // first honor the origin charset if appropriate. as an optimization,
        // only do this if the segment is non-ASCII.  Further, if mCharset is
        // null or the empty string then the origin charset is UTF-8 and there
        // is nothing to do.
        nsAutoCString encBuf;
        if (mCharset && *mCharset && !nsCRT::IsAscii(str + pos, len)) {
            // we have to encode this segment
            if (mEncoder || InitUnicodeEncoder()) {
                NS_ConvertUTF8toUTF16 ucsBuf(Substring(str + pos, str + pos + len));
                if (NS_SUCCEEDED(EncodeString(mEncoder, ucsBuf, encBuf))) {
                    str = encBuf.get();
                    pos = 0;
                    len = encBuf.Length();
                }
                // else some failure occurred... assume UTF-8 is ok.
            }
        }

        // escape per RFC2396 unless UTF-8 and allowed by preferences
        int16_t escapeFlags = (gEscapeUTF8 || mEncoder) ? 0 : esc_OnlyASCII;

        uint32_t initLen = result.Length();

        // now perform any required escaping
        if (NS_EscapeURL(str + pos, len, mask | escapeFlags, result)) {
            len = result.Length() - initLen;
            appended = true;
        }
        else if (str == encBuf.get()) {
            result += encBuf; // append only!!
            len = encBuf.Length();
            appended = true;
        }
        len += extraLen;
    }
    return len;
}

const nsACString &nsStandardURL::
nsSegmentEncoder::EncodeSegment(const nsASingleFragmentCString &str,
                                int16_t mask,
                                nsAFlatCString &result)
{
    const char *text;
    bool encoded;
    EncodeSegmentCount(str.BeginReading(text), URLSegment(0, str.Length()), mask, result, encoded);
    if (encoded)
        return result;
    return str;
}

bool nsStandardURL::
nsSegmentEncoder::InitUnicodeEncoder()
{
    NS_ASSERTION(!mEncoder, "Don't call this if we have an encoder already!");
    // "replacement" won't survive another label resolution
    nsDependentCString label(mCharset);
    if (label.EqualsLiteral("replacement")) {
      mEncoder = EncodingUtils::EncoderForEncoding(label);
      return true;
    }
    nsAutoCString encoding;
    if (!EncodingUtils::FindEncodingForLabelNoReplacement(label, encoding)) {
      return false;
    }
    mEncoder = EncodingUtils::EncoderForEncoding(encoding);
    return true;
}

#define GET_SEGMENT_ENCODER_INTERNAL(name, useUTF8) \
    nsSegmentEncoder name(useUTF8 ? nullptr : mOriginCharset.get())

#define GET_SEGMENT_ENCODER(name) \
    GET_SEGMENT_ENCODER_INTERNAL(name, gAlwaysEncodeInUTF8)

#define GET_QUERY_ENCODER(name) \
    GET_SEGMENT_ENCODER_INTERNAL(name, false)

//----------------------------------------------------------------------------
// nsStandardURL <public>
//----------------------------------------------------------------------------

#ifdef DEBUG_DUMP_URLS_AT_SHUTDOWN
static PRCList gAllURLs;
#endif

nsStandardURL::nsStandardURL(bool aSupportsFileURL, bool aTrackURL)
    : mDefaultPort(-1)
    , mPort(-1)
    , mHostA(nullptr)
    , mHostEncoding(eEncoding_ASCII)
    , mSpecEncoding(eEncoding_Unknown)
    , mURLType(URLTYPE_STANDARD)
    , mMutable(true)
    , mSupportsFileURL(aSupportsFileURL)
{
    LOG(("Creating nsStandardURL @%p\n", this));

    if (!gInitialized) {
        gInitialized = true;
        InitGlobalObjects();
    }

    // default parser in case nsIStandardURL::Init is never called
    mParser = net_GetStdURLParser();

#ifdef DEBUG_DUMP_URLS_AT_SHUTDOWN
    if (NS_IsMainThread()) {
        if (aTrackURL) {
            PR_APPEND_LINK(&mDebugCList, &gAllURLs);
        } else {
            PR_INIT_CLIST(&mDebugCList);
        }
    }
#endif
}

nsStandardURL::~nsStandardURL()
{
    LOG(("Destroying nsStandardURL @%p\n", this));

    if (mHostA) {
        free(mHostA);
    }
#ifdef DEBUG_DUMP_URLS_AT_SHUTDOWN
    if (NS_IsMainThread()) {
        if (!PR_CLIST_IS_EMPTY(&mDebugCList)) {
            PR_REMOVE_LINK(&mDebugCList);
        }
    }
#endif
}

#ifdef DEBUG_DUMP_URLS_AT_SHUTDOWN
struct DumpLeakedURLs {
    DumpLeakedURLs() {}
    ~DumpLeakedURLs();
};

DumpLeakedURLs::~DumpLeakedURLs()
{
    MOZ_ASSERT(NS_IsMainThread());
    if (!PR_CLIST_IS_EMPTY(&gAllURLs)) {
        printf("Leaked URLs:\n");
        for (PRCList *l = PR_LIST_HEAD(&gAllURLs); l != &gAllURLs; l = PR_NEXT_LINK(l)) {
            nsStandardURL *url = reinterpret_cast<nsStandardURL*>(reinterpret_cast<char*>(l) - offsetof(nsStandardURL, mDebugCList));
            url->PrintSpec();
        }
    }
}
#endif

void
nsStandardURL::InitGlobalObjects()
{
    nsCOMPtr<nsIPrefBranch> prefBranch( do_GetService(NS_PREFSERVICE_CONTRACTID) );
    if (prefBranch) {
        nsCOMPtr<nsIObserver> obs( new nsPrefObserver() );
        prefBranch->AddObserver(NS_NET_PREF_ESCAPEUTF8, obs.get(), false);
        prefBranch->AddObserver(NS_NET_PREF_ALWAYSENCODEINUTF8, obs.get(), false);

        PrefsChanged(prefBranch, nullptr);
    }

#ifdef DEBUG_DUMP_URLS_AT_SHUTDOWN
    PR_INIT_CLIST(&gAllURLs);
#endif
}

void
nsStandardURL::ShutdownGlobalObjects()
{
    NS_IF_RELEASE(gIDN);

#ifdef DEBUG_DUMP_URLS_AT_SHUTDOWN
    if (gInitialized) {
        // This instanciates a dummy class, and will trigger the class
        // destructor when libxul is unloaded. This is equivalent to atexit(),
        // but gracefully handles dlclose().
        static DumpLeakedURLs d;
    }
#endif
}

//----------------------------------------------------------------------------
// nsStandardURL <private>
//----------------------------------------------------------------------------

void
nsStandardURL::Clear()
{
    mSpec.Truncate();

    mPort = -1;

    mScheme.Reset();
    mAuthority.Reset();
    mUsername.Reset();
    mPassword.Reset();
    mHost.Reset();
    mHostEncoding = eEncoding_ASCII;

    mPath.Reset();
    mFilepath.Reset();
    mDirectory.Reset();
    mBasename.Reset();

    mExtension.Reset();
    mQuery.Reset();
    mRef.Reset();

    InvalidateCache();
}

void
nsStandardURL::InvalidateCache(bool invalidateCachedFile)
{
    if (invalidateCachedFile)
        mFile = 0;
    if (mHostA) {
        free(mHostA);
        mHostA = nullptr;
    }
    mSpecEncoding = eEncoding_Unknown;
}

nsresult
nsStandardURL::NormalizeIDN(const nsCSubstring &host, nsCString &result)
{
    // If host is ACE, then convert to UTF-8.  Else, if host is already UTF-8,
    // then make sure it is normalized per IDN.

    // this function returns true if normalization succeeds.

    // NOTE: As a side-effect this function sets mHostEncoding.  While it would
    // be nice to avoid side-effects in this function, the implementation of
    // this function is already somewhat bound to the behavior of the
    // callsites.  Anyways, this function exists to avoid code duplication, so
    // side-effects abound :-/

    NS_ASSERTION(mHostEncoding == eEncoding_ASCII, "unexpected default encoding");

    bool isASCII;
    if (!gIDN) {
        nsCOMPtr<nsIIDNService> serv(do_GetService(NS_IDNSERVICE_CONTRACTID));
        if (serv) {
            NS_ADDREF(gIDN = serv.get());
        }
    }

    result.Truncate();
    nsresult rv = NS_ERROR_UNEXPECTED;
    if (gIDN) {
        rv = gIDN->ConvertToDisplayIDN(host, &isASCII, result);
        if (NS_SUCCEEDED(rv) && !isASCII) {
          mHostEncoding = eEncoding_UTF8;
        }
    }

    return rv;
}

bool
nsStandardURL::ValidIPv6orHostname(const char *host, uint32_t length)
{
    if (!host || !*host) {
        // Should not be NULL or empty string
        return false;
    }

    if (length != strlen(host)) {
        // Embedded null
        return false;
    }

    bool openBracket = host[0] == '[';
    bool closeBracket = host[length - 1] == ']';

    if (openBracket && closeBracket) {
        return net_IsValidIPv6Addr(host + 1, length - 2);
    }

    if (openBracket || closeBracket) {
        // Fail if only one of the brackets is present
        return false;
    }

    const char *end = host + length;
    if (end != net_FindCharInSet(host, end, "\t\n\v\f\r #/:?@[\\]")) {
        // We still allow % because it is in the ID of addons.
        // Any percent encoded ASCII characters that are not allowed in the
        // hostname are not percent decoded, and will be parsed just fine.
        return false;
    }

    return true;
}

void
nsStandardURL::CoalescePath(netCoalesceFlags coalesceFlag, char *path)
{
    net_CoalesceDirs(coalesceFlag, path);
    int32_t newLen = strlen(path);
    if (newLen < mPath.mLen) {
        int32_t diff = newLen - mPath.mLen;
        mPath.mLen = newLen;
        mDirectory.mLen += diff;
        mFilepath.mLen += diff;
        ShiftFromBasename(diff);
    }
}

uint32_t
nsStandardURL::AppendSegmentToBuf(char *buf, uint32_t i, const char *str,
                                  const URLSegment &segInput, URLSegment &segOutput,
                                  const nsCString *escapedStr,
                                  bool useEscaped, int32_t *diff)
{
    MOZ_ASSERT(segInput.mLen == segOutput.mLen);

    if (diff) *diff = 0;

    if (segInput.mLen > 0) {
        if (useEscaped) {
            MOZ_ASSERT(diff);
            segOutput.mLen = escapedStr->Length();
            *diff = segOutput.mLen - segInput.mLen;
            memcpy(buf + i, escapedStr->get(), segOutput.mLen);
        } else {
            memcpy(buf + i, str + segInput.mPos, segInput.mLen);
        }
        segOutput.mPos = i;
        i += segOutput.mLen;
    } else {
        segOutput.mPos = i;
    }
    return i;
}

uint32_t
nsStandardURL::AppendToBuf(char *buf, uint32_t i, const char *str, uint32_t len)
{
    memcpy(buf + i, str, len);
    return i + len;
}

// basic algorithm:
//  1- escape url segments (for improved GetSpec efficiency)
//  2- allocate spec buffer
//  3- write url segments
//  4- update url segment positions and lengths
nsresult
nsStandardURL::BuildNormalizedSpec(const char *spec)
{
    // Assumptions: all member URLSegments must be relative the |spec| argument
    // passed to this function.

    // buffers for holding escaped url segments (these will remain empty unless
    // escaping is required).
    nsAutoCString encUsername, encPassword, encHost, encDirectory,
      encBasename, encExtension, encQuery, encRef;
    bool useEncUsername, useEncPassword, useEncHost = false,
      useEncDirectory, useEncBasename, useEncExtension, useEncQuery, useEncRef;
    nsAutoCString portbuf;

    //
    // escape each URL segment, if necessary, and calculate approximate normalized
    // spec length.
    //
    // [scheme://][username[:password]@]host[:port]/path[?query_string][#ref]

    uint32_t approxLen = 0;

    // the scheme is already ASCII
    if (mScheme.mLen > 0)
        approxLen += mScheme.mLen + 3; // includes room for "://", which we insert always

    // encode URL segments; convert UTF-8 to origin charset and possibly escape.
    // results written to encXXX variables only if |spec| is not already in the
    // appropriate encoding.
    {
        GET_SEGMENT_ENCODER(encoder);
        GET_QUERY_ENCODER(queryEncoder);
        // Items using an extraLen of 1 don't add anything unless mLen > 0
        // Username@
        approxLen += encoder.EncodeSegmentCount(spec, mUsername,  esc_Username,      encUsername,  useEncUsername, 1);
        // :password - we insert the ':' even if there's no actual password if "user:@" was in the spec
        if (mPassword.mLen >= 0)
            approxLen += 1 + encoder.EncodeSegmentCount(spec, mPassword,  esc_Password,      encPassword,  useEncPassword);
        // mHost is handled differently below due to encoding differences
        MOZ_ASSERT(mPort >= -1, "Invalid negative mPort");
        if (mPort != -1 && mPort != mDefaultPort)
        {
            // :port
            portbuf.AppendInt(mPort);
            approxLen += portbuf.Length() + 1;
        }

        approxLen += 1; // reserve space for possible leading '/' - may not be needed
        // Should just use mPath?  These are pessimistic, and thus waste space
        approxLen += encoder.EncodeSegmentCount(spec, mDirectory, esc_Directory,     encDirectory, useEncDirectory, 1);
        approxLen += encoder.EncodeSegmentCount(spec, mBasename,  esc_FileBaseName,  encBasename,  useEncBasename);
        approxLen += encoder.EncodeSegmentCount(spec, mExtension, esc_FileExtension, encExtension, useEncExtension, 1);

        // These next ones *always* add their leading character even if length is 0
        // Handles items like "http://#"
        // ?query
        if (mQuery.mLen >= 0)
            approxLen += 1 + queryEncoder.EncodeSegmentCount(spec, mQuery, esc_Query,        encQuery,     useEncQuery);
        // #ref

        if (mRef.mLen >= 0) {
            if (nsContentUtils::EncodeDecodeURLHash()) {
                approxLen += 1 + encoder.EncodeSegmentCount(spec, mRef, esc_Ref,
                                                            encRef, useEncRef);
            } else {
                approxLen += 1 + mRef.mLen;
                useEncRef = false;
            }
        }
    }

    // do not escape the hostname, if IPv6 address literal, mHost will
    // already point to a [ ] delimited IPv6 address literal.
    // However, perform Unicode normalization on it, as IDN does.
    mHostEncoding = eEncoding_ASCII;
    // Note that we don't disallow URLs without a host - file:, etc
    if (mHost.mLen > 0) {
        nsAutoCString tempHost;
        NS_UnescapeURL(spec + mHost.mPos, mHost.mLen, esc_AlwaysCopy | esc_Host, tempHost);
        if (tempHost.Contains('\0'))
            return NS_ERROR_MALFORMED_URI;  // null embedded in hostname
        if (tempHost.Contains(' '))
            return NS_ERROR_MALFORMED_URI;  // don't allow spaces in the hostname
        nsresult rv = NormalizeIDN(tempHost, encHost);
        if (NS_FAILED(rv)) {
            return rv;
        }

        // NormalizeIDN always copies, if the call was successful.
        useEncHost = true;
        approxLen += encHost.Length();

        if (!ValidIPv6orHostname(encHost.BeginReading(), encHost.Length())) {
            return NS_ERROR_MALFORMED_URI;
        }
    }

    // We must take a copy of every single segment because they are pointing to
    // the |spec| while we are changing their value, in case we must use
    // encoded strings.
    URLSegment username(mUsername);
    URLSegment password(mPassword);
    URLSegment host(mHost);
    URLSegment path(mPath);
    URLSegment filepath(mFilepath);
    URLSegment directory(mDirectory);
    URLSegment basename(mBasename);
    URLSegment extension(mExtension);
    URLSegment query(mQuery);
    URLSegment ref(mRef);

    //
    // generate the normalized URL string
    //
    // approxLen should be correct or 1 high
    if (!mSpec.SetLength(approxLen+1, mozilla::fallible)) // buf needs a trailing '\0' below
        return NS_ERROR_OUT_OF_MEMORY;
    char *buf;
    mSpec.BeginWriting(buf);
    uint32_t i = 0;
    int32_t diff = 0;

    if (mScheme.mLen > 0) {
        i = AppendSegmentToBuf(buf, i, spec, mScheme, mScheme);
        net_ToLowerCase(buf + mScheme.mPos, mScheme.mLen);
        i = AppendToBuf(buf, i, "://", 3);
    }

    // record authority starting position
    mAuthority.mPos = i;

    // append authority
    if (mUsername.mLen > 0) {
        i = AppendSegmentToBuf(buf, i, spec, username, mUsername,
                               &encUsername, useEncUsername, &diff);
        ShiftFromPassword(diff);
        if (password.mLen >= 0) {
            buf[i++] = ':';
            i = AppendSegmentToBuf(buf, i, spec, password, mPassword,
                                   &encPassword, useEncPassword, &diff);
            ShiftFromHost(diff);
        }
        buf[i++] = '@';
    }
    if (host.mLen > 0) {
        i = AppendSegmentToBuf(buf, i, spec, host, mHost, &encHost, useEncHost,
                               &diff);
        ShiftFromPath(diff);

        net_ToLowerCase(buf + mHost.mPos, mHost.mLen);
        MOZ_ASSERT(mPort >= -1, "Invalid negative mPort");
        if (mPort != -1 && mPort != mDefaultPort) {
            buf[i++] = ':';
            // Already formatted while building approxLen
            i = AppendToBuf(buf, i, portbuf.get(), portbuf.Length());
        }
    }

    // record authority length
    mAuthority.mLen = i - mAuthority.mPos;

    // path must always start with a "/"
    if (mPath.mLen <= 0) {
        LOG(("setting path=/"));
        mDirectory.mPos = mFilepath.mPos = mPath.mPos = i;
        mDirectory.mLen = mFilepath.mLen = mPath.mLen = 1;
        // basename must exist, even if empty (bug 113508)
        mBasename.mPos = i+1;
        mBasename.mLen = 0;
        buf[i++] = '/';
    }
    else {
        uint32_t leadingSlash = 0;
        if (spec[path.mPos] != '/') {
            LOG(("adding leading slash to path\n"));
            leadingSlash = 1;
            buf[i++] = '/';
            // basename must exist, even if empty (bugs 113508, 429347)
            if (mBasename.mLen == -1) {
                mBasename.mPos = basename.mPos = i;
                mBasename.mLen = basename.mLen = 0;
            }
        }

        // record corrected (file)path starting position
        mPath.mPos = mFilepath.mPos = i - leadingSlash;

        i = AppendSegmentToBuf(buf, i, spec, directory, mDirectory,
                               &encDirectory, useEncDirectory, &diff);
        ShiftFromBasename(diff);

        // the directory must end with a '/'
        if (buf[i-1] != '/') {
            buf[i++] = '/';
            mDirectory.mLen++;
        }

        i = AppendSegmentToBuf(buf, i, spec, basename, mBasename,
                               &encBasename, useEncBasename, &diff);
        ShiftFromExtension(diff);

        // make corrections to directory segment if leadingSlash
        if (leadingSlash) {
            mDirectory.mPos = mPath.mPos;
            if (mDirectory.mLen >= 0)
                mDirectory.mLen += leadingSlash;
            else
                mDirectory.mLen = 1;
        }

        if (mExtension.mLen >= 0) {
            buf[i++] = '.';
            i = AppendSegmentToBuf(buf, i, spec, extension, mExtension,
                                   &encExtension, useEncExtension, &diff);
            ShiftFromQuery(diff);
        }
        // calculate corrected filepath length
        mFilepath.mLen = i - mFilepath.mPos;

        if (mQuery.mLen >= 0) {
            buf[i++] = '?';
            i = AppendSegmentToBuf(buf, i, spec, query, mQuery,
                                   &encQuery, useEncQuery,
                                   &diff);
            ShiftFromRef(diff);
        }
        if (mRef.mLen >= 0) {
            buf[i++] = '#';
            i = AppendSegmentToBuf(buf, i, spec, ref, mRef, &encRef, useEncRef,
                                   &diff);
        }
        // calculate corrected path length
        mPath.mLen = i - mPath.mPos;
    }

    buf[i] = '\0';

    if (mDirectory.mLen > 1) {
        netCoalesceFlags coalesceFlag = NET_COALESCE_NORMAL;
        if (SegmentIs(buf,mScheme,"ftp")) {
            coalesceFlag = (netCoalesceFlags) (coalesceFlag 
                                        | NET_COALESCE_ALLOW_RELATIVE_ROOT
                                        | NET_COALESCE_DOUBLE_SLASH_IS_ROOT);
        }
        CoalescePath(coalesceFlag, buf + mDirectory.mPos);
    }
    mSpec.SetLength(strlen(buf));
    NS_ASSERTION(mSpec.Length() <= approxLen, "We've overflowed the mSpec buffer!");
    return NS_OK;
}

bool
nsStandardURL::SegmentIs(const URLSegment &seg, const char *val, bool ignoreCase)
{
    // one or both may be null
    if (!val || mSpec.IsEmpty())
        return (!val && (mSpec.IsEmpty() || seg.mLen < 0));
    if (seg.mLen < 0)
        return false;
    // if the first |seg.mLen| chars of |val| match, then |val| must
    // also be null terminated at |seg.mLen|.
    if (ignoreCase)
        return !PL_strncasecmp(mSpec.get() + seg.mPos, val, seg.mLen)
            && (val[seg.mLen] == '\0');
    else
        return !strncmp(mSpec.get() + seg.mPos, val, seg.mLen)
            && (val[seg.mLen] == '\0');
}

bool
nsStandardURL::SegmentIs(const char* spec, const URLSegment &seg, const char *val, bool ignoreCase)
{
    // one or both may be null
    if (!val || !spec)
        return (!val && (!spec || seg.mLen < 0));
    if (seg.mLen < 0)
        return false;
    // if the first |seg.mLen| chars of |val| match, then |val| must
    // also be null terminated at |seg.mLen|.
    if (ignoreCase)
        return !PL_strncasecmp(spec + seg.mPos, val, seg.mLen)
            && (val[seg.mLen] == '\0');
    else
        return !strncmp(spec + seg.mPos, val, seg.mLen)
            && (val[seg.mLen] == '\0');
}

bool
nsStandardURL::SegmentIs(const URLSegment &seg1, const char *val, const URLSegment &seg2, bool ignoreCase)
{
    if (seg1.mLen != seg2.mLen)
        return false;
    if (seg1.mLen == -1 || (!val && mSpec.IsEmpty()))
        return true; // both are empty
    if (!val)
        return false;
    if (ignoreCase)
        return !PL_strncasecmp(mSpec.get() + seg1.mPos, val + seg2.mPos, seg1.mLen); 
    else
        return !strncmp(mSpec.get() + seg1.mPos, val + seg2.mPos, seg1.mLen); 
}

int32_t
nsStandardURL::ReplaceSegment(uint32_t pos, uint32_t len, const char *val, uint32_t valLen)
{
    if (val && valLen) {
        if (len == 0)
            mSpec.Insert(val, pos, valLen);
        else
            mSpec.Replace(pos, len, nsDependentCString(val, valLen));
        return valLen - len;
    }

    // else remove the specified segment
    mSpec.Cut(pos, len);
    return -int32_t(len);
}

int32_t
nsStandardURL::ReplaceSegment(uint32_t pos, uint32_t len, const nsACString &val)
{
    if (len == 0)
        mSpec.Insert(val, pos);
    else
        mSpec.Replace(pos, len, val);
    return val.Length() - len;
}

nsresult
nsStandardURL::ParseURL(const char *spec, int32_t specLen)
{
    nsresult rv;

    if (specLen > net_GetURLMaxLength()) {
        return NS_ERROR_MALFORMED_URI;
    }

    //
    // parse given URL string
    //
    rv = mParser->ParseURL(spec, specLen,
                           &mScheme.mPos, &mScheme.mLen,
                           &mAuthority.mPos, &mAuthority.mLen,
                           &mPath.mPos, &mPath.mLen);
    if (NS_FAILED(rv)) return rv;

#ifdef DEBUG
    if (mScheme.mLen <= 0) {
        printf("spec=%s\n", spec);
        NS_WARNING("malformed url: no scheme");
    }
#endif
     
    if (mAuthority.mLen > 0) {
        rv = mParser->ParseAuthority(spec + mAuthority.mPos, mAuthority.mLen,
                                     &mUsername.mPos, &mUsername.mLen,
                                     &mPassword.mPos, &mPassword.mLen,
                                     &mHost.mPos, &mHost.mLen,
                                     &mPort);
        if (NS_FAILED(rv)) return rv;

        // Don't allow mPort to be set to this URI's default port
        if (mPort == mDefaultPort)
            mPort = -1;

        mUsername.mPos += mAuthority.mPos;
        mPassword.mPos += mAuthority.mPos;
        mHost.mPos += mAuthority.mPos;
    }

    if (mPath.mLen > 0)
        rv = ParsePath(spec, mPath.mPos, mPath.mLen);

    return rv;
}

nsresult
nsStandardURL::ParsePath(const char *spec, uint32_t pathPos, int32_t pathLen)
{
    LOG(("ParsePath: %s pathpos %d len %d\n",spec,pathPos,pathLen));

    if (pathLen > net_GetURLMaxLength()) {
        return NS_ERROR_MALFORMED_URI;
    }

    nsresult rv = mParser->ParsePath(spec + pathPos, pathLen,
                                     &mFilepath.mPos, &mFilepath.mLen,
                                     &mQuery.mPos, &mQuery.mLen,
                                     &mRef.mPos, &mRef.mLen);
    if (NS_FAILED(rv)) return rv;

    mFilepath.mPos += pathPos;
    mQuery.mPos += pathPos;
    mRef.mPos += pathPos;

    if (mFilepath.mLen > 0) {
        rv = mParser->ParseFilePath(spec + mFilepath.mPos, mFilepath.mLen,
                                    &mDirectory.mPos, &mDirectory.mLen,
                                    &mBasename.mPos, &mBasename.mLen,
                                    &mExtension.mPos, &mExtension.mLen);
        if (NS_FAILED(rv)) return rv;

        mDirectory.mPos += mFilepath.mPos;
        mBasename.mPos += mFilepath.mPos;
        mExtension.mPos += mFilepath.mPos;
    }
    return NS_OK;
}

char *
nsStandardURL::AppendToSubstring(uint32_t pos,
                                 int32_t len,
                                 const char *tail)
{
    // Verify pos and length are within boundaries
    if (pos > mSpec.Length())
        return nullptr;
    if (len < 0)
        return nullptr;
    if ((uint32_t)len > (mSpec.Length() - pos))
        return nullptr;
    if (!tail)
        return nullptr;

    uint32_t tailLen = strlen(tail);

    // Check for int overflow for proposed length of combined string
    if (UINT32_MAX - ((uint32_t)len + 1) < tailLen)
        return nullptr;

    char *result = (char *) moz_xmalloc(len + tailLen + 1);
    if (result) {
        memcpy(result, mSpec.get() + pos, len);
        memcpy(result + len, tail, tailLen);
        result[len + tailLen] = '\0';
    }
    return result;
}

nsresult
nsStandardURL::ReadSegment(nsIBinaryInputStream *stream, URLSegment &seg)
{
    nsresult rv;

    rv = stream->Read32(&seg.mPos);
    if (NS_FAILED(rv)) return rv;

    rv = stream->Read32((uint32_t *) &seg.mLen);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

nsresult
nsStandardURL::WriteSegment(nsIBinaryOutputStream *stream, const URLSegment &seg)
{
    nsresult rv;

    rv = stream->Write32(seg.mPos);
    if (NS_FAILED(rv)) return rv;

    rv = stream->Write32(uint32_t(seg.mLen));
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

/* static */ void
nsStandardURL::PrefsChanged(nsIPrefBranch *prefs, const char *pref)
{
    bool val;

    LOG(("nsStandardURL::PrefsChanged [pref=%s]\n", pref));

#define PREF_CHANGED(p) ((pref == nullptr) || !strcmp(pref, p))
#define GOT_PREF(p, b) (NS_SUCCEEDED(prefs->GetBoolPref(p, &b)))

    if (PREF_CHANGED(NS_NET_PREF_ESCAPEUTF8)) {
        if (GOT_PREF(NS_NET_PREF_ESCAPEUTF8, val))
            gEscapeUTF8 = val;
        LOG(("escape UTF-8 %s\n", gEscapeUTF8 ? "enabled" : "disabled"));
    }

    if (PREF_CHANGED(NS_NET_PREF_ALWAYSENCODEINUTF8)) {
        if (GOT_PREF(NS_NET_PREF_ALWAYSENCODEINUTF8, val))
            gAlwaysEncodeInUTF8 = val;
        LOG(("encode in UTF-8 %s\n", gAlwaysEncodeInUTF8 ? "enabled" : "disabled"));
    }
#undef PREF_CHANGED
#undef GOT_PREF
}

#define SHIFT_FROM(name, what)                    \
void                                              \
nsStandardURL::name(int32_t diff)                 \
{                                                 \
    if (!diff) return;                            \
    if (what.mLen >= 0) {                         \
        CheckedInt<int32_t> pos = what.mPos;      \
        pos += diff;                              \
        MOZ_ASSERT(pos.isValid());                \
        what.mPos = pos.value();                  \
    }

#define SHIFT_FROM_NEXT(name, what, next)         \
    SHIFT_FROM(name, what)                        \
    next(diff);                                   \
}

#define SHIFT_FROM_LAST(name, what)               \
    SHIFT_FROM(name, what)                        \
}

SHIFT_FROM_NEXT(ShiftFromAuthority, mAuthority, ShiftFromUsername)
SHIFT_FROM_NEXT(ShiftFromUsername, mUsername, ShiftFromPassword)
SHIFT_FROM_NEXT(ShiftFromPassword, mPassword, ShiftFromHost)
SHIFT_FROM_NEXT(ShiftFromHost, mHost, ShiftFromPath)
SHIFT_FROM_NEXT(ShiftFromPath, mPath, ShiftFromFilepath)
SHIFT_FROM_NEXT(ShiftFromFilepath, mFilepath, ShiftFromDirectory)
SHIFT_FROM_NEXT(ShiftFromDirectory, mDirectory, ShiftFromBasename)
SHIFT_FROM_NEXT(ShiftFromBasename, mBasename, ShiftFromExtension)
SHIFT_FROM_NEXT(ShiftFromExtension, mExtension, ShiftFromQuery)
SHIFT_FROM_NEXT(ShiftFromQuery, mQuery, ShiftFromRef)
SHIFT_FROM_LAST(ShiftFromRef, mRef)

//----------------------------------------------------------------------------
// nsStandardURL::nsISupports
//----------------------------------------------------------------------------

NS_IMPL_ADDREF(nsStandardURL)
NS_IMPL_RELEASE(nsStandardURL)

NS_INTERFACE_MAP_BEGIN(nsStandardURL)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStandardURL)
    NS_INTERFACE_MAP_ENTRY(nsIURI)
    NS_INTERFACE_MAP_ENTRY(nsIURL)
    NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIFileURL, mSupportsFileURL)
    NS_INTERFACE_MAP_ENTRY(nsIStandardURL)
    NS_INTERFACE_MAP_ENTRY(nsISerializable)
    NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
    NS_INTERFACE_MAP_ENTRY(nsIMutable)
    NS_INTERFACE_MAP_ENTRY(nsIIPCSerializableURI)
    NS_INTERFACE_MAP_ENTRY(nsISensitiveInfoHiddenURI)
    // see nsStandardURL::Equals
    if (aIID.Equals(kThisImplCID))
        foundInterface = static_cast<nsIURI *>(this);
    else
    NS_INTERFACE_MAP_ENTRY(nsISizeOf)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------------
// nsStandardURL::nsIURI
//----------------------------------------------------------------------------

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsStandardURL::GetSpec(nsACString &result)
{
    MOZ_ASSERT(mSpec.Length() <= (uint32_t) net_GetURLMaxLength(),
               "The spec should never be this long, we missed a check.");
    result = mSpec;
    return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsStandardURL::GetSensitiveInfoHiddenSpec(nsACString &result)
{
    result = mSpec;
    if (mPassword.mLen >= 0) {
      result.Replace(mPassword.mPos, mPassword.mLen, "****");
    }
    return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsStandardURL::GetSpecIgnoringRef(nsACString &result)
{
    // URI without ref is 0 to one char before ref
    if (mRef.mLen >= 0) {
        URLSegment noRef(0, mRef.mPos - 1);

        result = Segment(noRef);
    } else {
        result = mSpec;
    }
    return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsStandardURL::GetPrePath(nsACString &result)
{
    result = Prepath();
    return NS_OK;
}

// result is strictly US-ASCII
NS_IMETHODIMP
nsStandardURL::GetScheme(nsACString &result)
{
    result = Scheme();
    return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsStandardURL::GetUserPass(nsACString &result)
{
    result = Userpass();
    return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsStandardURL::GetUsername(nsACString &result)
{
    result = Username();
    return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsStandardURL::GetPassword(nsACString &result)
{
    result = Password();
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::GetHostPort(nsACString &result)
{
    result = Hostport();
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::GetHost(nsACString &result)
{
    result = Host();
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::GetPort(int32_t *result)
{
    *result = mPort;
    return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsStandardURL::GetPath(nsACString &result)
{
    result = Path();
    return NS_OK;
}

// result is ASCII
NS_IMETHODIMP
nsStandardURL::GetAsciiSpec(nsACString &result)
{
    if (mSpecEncoding == eEncoding_Unknown) {
        if (IsASCII(mSpec))
            mSpecEncoding = eEncoding_ASCII;
        else
            mSpecEncoding = eEncoding_UTF8;
    }

    if (mSpecEncoding == eEncoding_ASCII) {
        result = mSpec;
        return NS_OK;
    }

    // try to guess the capacity required for result...
    result.SetCapacity(mSpec.Length() + std::min<uint32_t>(32, mSpec.Length()/10));

    result = Substring(mSpec, 0, mScheme.mLen + 3);

    NS_EscapeURL(Userpass(true), esc_OnlyNonASCII | esc_AlwaysCopy, result);

    // get the hostport
    nsAutoCString hostport;
    MOZ_ALWAYS_SUCCEEDS(GetAsciiHostPort(hostport));
    result += hostport;

    NS_EscapeURL(Path(), esc_OnlyNonASCII | esc_AlwaysCopy, result);
    return NS_OK;
}

// result is ASCII
NS_IMETHODIMP
nsStandardURL::GetAsciiHostPort(nsACString &result)
{
    if (mHostEncoding == eEncoding_ASCII) {
        result = Hostport();
        return NS_OK;
    }

    MOZ_ALWAYS_SUCCEEDS(GetAsciiHost(result));

    // As our mHostEncoding is not eEncoding_ASCII, we know that
    // the our host is not ipv6, and we can avoid looking at it.
    MOZ_ASSERT(result.FindChar(':') == -1, "The host must not be ipv6");

    // hostport = "hostA" + ":port"
    uint32_t pos = mHost.mPos + mHost.mLen;
    if (pos < mPath.mPos)
        result += Substring(mSpec, pos, mPath.mPos - pos);

    return NS_OK;
}

// result is ASCII
NS_IMETHODIMP
nsStandardURL::GetAsciiHost(nsACString &result)
{
    if (mHostEncoding == eEncoding_ASCII) {
        result = Host();
        return NS_OK;
    }

    // perhaps we have it cached...
    if (mHostA) {
        result = mHostA;
        return NS_OK;
    }

    if (gIDN) {
        nsresult rv;
        rv = gIDN->ConvertUTF8toACE(Host(), result);
        if (NS_SUCCEEDED(rv)) {
            mHostA = ToNewCString(result);
            return NS_OK;
        }
        NS_WARNING("nsIDNService::ConvertUTF8toACE failed");
    }

    // something went wrong... guess all we can do is URL escape :-/
    NS_EscapeURL(Host(), esc_OnlyNonASCII | esc_AlwaysCopy, result);
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::GetOriginCharset(nsACString &result)
{
    if (mOriginCharset.IsEmpty())
        result.AssignLiteral("UTF-8");
    else
        result = mOriginCharset;
    return NS_OK;
}

static bool
IsSpecialProtocol(const nsACString &input)
{
    nsACString::const_iterator start, end;
    input.BeginReading(start);
    nsACString::const_iterator iterator(start);
    input.EndReading(end);

    while (iterator != end && *iterator != ':') {
        iterator++;
    }

    nsAutoCString protocol(nsDependentCSubstring(start.get(), iterator.get()));

    return protocol.LowerCaseEqualsLiteral("http") ||
           protocol.LowerCaseEqualsLiteral("https") ||
           protocol.LowerCaseEqualsLiteral("ftp") ||
           protocol.LowerCaseEqualsLiteral("ws") ||
           protocol.LowerCaseEqualsLiteral("wss") ||
           protocol.LowerCaseEqualsLiteral("file") ||
           protocol.LowerCaseEqualsLiteral("gopher");
}

NS_IMETHODIMP
nsStandardURL::SetSpec(const nsACString &input)
{
    ENSURE_MUTABLE();

    const nsPromiseFlatCString &flat = PromiseFlatCString(input);
    const char *spec = flat.get();
    int32_t specLength = flat.Length();

    LOG(("nsStandardURL::SetSpec [spec=%s]\n", spec));

    if (!spec || !*spec)
        return NS_ERROR_MALFORMED_URI;

    if (input.Length() > (uint32_t) net_GetURLMaxLength()) {
        return NS_ERROR_MALFORMED_URI;
    }

    // NUL characters aren't allowed
    // \r\n\t are stripped out instead of returning error(see below)
    if (input.Contains('\0')) {
        return NS_ERROR_MALFORMED_URI;
    }

    // Make a backup of the curent URL
    nsStandardURL prevURL(false,false);
    prevURL.CopyMembers(this, eHonorRef);
    Clear();

    // filter out unexpected chars "\r\n\t" if necessary
    nsAutoCString filteredURI;
    if (!net_FilterURIString(spec, filteredURI)) {
        // Copy the content into filteredURI even if no whitespace was stripped.
        // We need a non-const buffer to perform backslash replacement.
        filteredURI = input;
    }

    if (IsSpecialProtocol(filteredURI)) {
        // Bug 652186: Replace all backslashes with slashes when parsing paths
        // Stop when we reach the query or the hash.
        nsAutoCString::iterator start;
        nsAutoCString::iterator end;
        filteredURI.BeginWriting(start);
        filteredURI.EndWriting(end);
        while (start != end) {
            if (*start == '?' || *start == '#') {
                break;
            }
            if (*start == '\\') {
                *start = '/';
            }
            start++;
        }
    }

    spec = filteredURI.get();
    specLength = filteredURI.Length();


    // parse the given URL...
    nsresult rv = ParseURL(spec, specLength);
    if (NS_SUCCEEDED(rv)) {
        // finally, use the URLSegment member variables to build a normalized
        // copy of |spec|
        rv = BuildNormalizedSpec(spec);
    }

    if (NS_FAILED(rv)) {
        Clear();
        // If parsing the spec has failed, restore the old URL
        // so we don't end up with an empty URL.
        CopyMembers(&prevURL, eHonorRef);
        return rv;
    }

    if (LOG_ENABLED()) {
        LOG((" spec      = %s\n", mSpec.get()));
        LOG((" port      = %d\n", mPort));
        LOG((" scheme    = (%u,%d)\n", mScheme.mPos,    mScheme.mLen));
        LOG((" authority = (%u,%d)\n", mAuthority.mPos, mAuthority.mLen));
        LOG((" username  = (%u,%d)\n", mUsername.mPos,  mUsername.mLen));
        LOG((" password  = (%u,%d)\n", mPassword.mPos,  mPassword.mLen));
        LOG((" hostname  = (%u,%d)\n", mHost.mPos,      mHost.mLen));
        LOG((" path      = (%u,%d)\n", mPath.mPos,      mPath.mLen));
        LOG((" filepath  = (%u,%d)\n", mFilepath.mPos,  mFilepath.mLen));
        LOG((" directory = (%u,%d)\n", mDirectory.mPos, mDirectory.mLen));
        LOG((" basename  = (%u,%d)\n", mBasename.mPos,  mBasename.mLen));
        LOG((" extension = (%u,%d)\n", mExtension.mPos, mExtension.mLen));
        LOG((" query     = (%u,%d)\n", mQuery.mPos,     mQuery.mLen));
        LOG((" ref       = (%u,%d)\n", mRef.mPos,       mRef.mLen));
    }
    return rv;
}

NS_IMETHODIMP
nsStandardURL::SetScheme(const nsACString &input)
{
    ENSURE_MUTABLE();

    const nsPromiseFlatCString &scheme = PromiseFlatCString(input);

    LOG(("nsStandardURL::SetScheme [scheme=%s]\n", scheme.get()));

    if (scheme.IsEmpty()) {
        NS_WARNING("cannot remove the scheme from an url");
        return NS_ERROR_UNEXPECTED;
    }
    if (mScheme.mLen < 0) {
        NS_WARNING("uninitialized");
        return NS_ERROR_NOT_INITIALIZED;
    }

    if (!net_IsValidScheme(scheme)) {
        NS_WARNING("the given url scheme contains invalid characters");
        return NS_ERROR_UNEXPECTED;
    }

    if (mSpec.Length() + input.Length() - Scheme().Length() > (uint32_t) net_GetURLMaxLength()) {
        return NS_ERROR_MALFORMED_URI;
    }

    InvalidateCache();

    int32_t shift = ReplaceSegment(mScheme.mPos, mScheme.mLen, scheme);

    if (shift) {
        mScheme.mLen = scheme.Length();
        ShiftFromAuthority(shift);
    }

    // ensure new scheme is lowercase
    //
    // XXX the string code unfortunately doesn't provide a ToLowerCase
    //     that operates on a substring.
    net_ToLowerCase((char *) mSpec.get(), mScheme.mLen);
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetUserPass(const nsACString &input)
{
    ENSURE_MUTABLE();

    const nsPromiseFlatCString &userpass = PromiseFlatCString(input);

    LOG(("nsStandardURL::SetUserPass [userpass=%s]\n", userpass.get()));

    if (mURLType == URLTYPE_NO_AUTHORITY) {
        if (userpass.IsEmpty())
            return NS_OK;
        NS_WARNING("cannot set user:pass on no-auth url");
        return NS_ERROR_UNEXPECTED;
    }
    if (mAuthority.mLen < 0) {
        NS_WARNING("uninitialized");
        return NS_ERROR_NOT_INITIALIZED;
    }

    if (mSpec.Length() + input.Length() - Userpass(true).Length() > (uint32_t) net_GetURLMaxLength()) {
        return NS_ERROR_MALFORMED_URI;
    }

    InvalidateCache();

    if (userpass.IsEmpty()) {
        // remove user:pass
        if (mUsername.mLen > 0) {
            if (mPassword.mLen > 0)
                mUsername.mLen += (mPassword.mLen + 1);
            mUsername.mLen++;
            mSpec.Cut(mUsername.mPos, mUsername.mLen);
            mAuthority.mLen -= mUsername.mLen;
            ShiftFromHost(-mUsername.mLen);
            mUsername.mLen = -1;
            mPassword.mLen = -1;
        }
        return NS_OK;
    }

    NS_ASSERTION(mHost.mLen >= 0, "uninitialized");

    nsresult rv;
    uint32_t usernamePos, passwordPos;
    int32_t usernameLen, passwordLen;

    rv = mParser->ParseUserInfo(userpass.get(), userpass.Length(),
                                &usernamePos, &usernameLen,
                                &passwordPos, &passwordLen);
    if (NS_FAILED(rv)) return rv;

    // build new user:pass in |buf|
    nsAutoCString buf;
    if (usernameLen > 0) {
        GET_SEGMENT_ENCODER(encoder);
        bool ignoredOut;
        usernameLen = encoder.EncodeSegmentCount(userpass.get(),
                                                 URLSegment(usernamePos,
                                                            usernameLen),
                                                 esc_Username | esc_AlwaysCopy,
                                                 buf, ignoredOut);
        if (passwordLen >= 0) {
            buf.Append(':');
            passwordLen = encoder.EncodeSegmentCount(userpass.get(),
                                                     URLSegment(passwordPos,
                                                                passwordLen),
                                                     esc_Password |
                                                     esc_AlwaysCopy, buf,
                                                     ignoredOut);
        }
        if (mUsername.mLen < 0)
            buf.Append('@');
    }

    uint32_t shift = 0;

    if (mUsername.mLen < 0) {
        // no existing user:pass
        if (!buf.IsEmpty()) {
            mSpec.Insert(buf, mHost.mPos);
            mUsername.mPos = mHost.mPos;
            shift = buf.Length();
        }
    }
    else {
        // replace existing user:pass
        uint32_t userpassLen = mUsername.mLen;
        if (mPassword.mLen >= 0)
            userpassLen += (mPassword.mLen + 1);
        mSpec.Replace(mUsername.mPos, userpassLen, buf);
        shift = buf.Length() - userpassLen;
    }
    if (shift) {
        ShiftFromHost(shift);
        mAuthority.mLen += shift;
    }
    // update positions and lengths
    mUsername.mLen = usernameLen;
    mPassword.mLen = passwordLen;
    if (passwordLen)
        mPassword.mPos = mUsername.mPos + mUsername.mLen + 1;
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetUsername(const nsACString &input)
{
    ENSURE_MUTABLE();

    const nsPromiseFlatCString &username = PromiseFlatCString(input);

    LOG(("nsStandardURL::SetUsername [username=%s]\n", username.get()));

    if (mURLType == URLTYPE_NO_AUTHORITY) {
        if (username.IsEmpty())
            return NS_OK;
        NS_WARNING("cannot set username on no-auth url");
        return NS_ERROR_UNEXPECTED;
    }

    if (username.IsEmpty())
        return SetUserPass(username);

    if (mSpec.Length() + input.Length() - Username().Length() > (uint32_t) net_GetURLMaxLength()) {
        return NS_ERROR_MALFORMED_URI;
    }

    InvalidateCache();

    // escape username if necessary
    nsAutoCString buf;
    GET_SEGMENT_ENCODER(encoder);
    const nsACString &escUsername =
        encoder.EncodeSegment(username, esc_Username, buf);

    int32_t shift;

    if (mUsername.mLen < 0) {
        mUsername.mPos = mAuthority.mPos;
        mSpec.Insert(escUsername + NS_LITERAL_CSTRING("@"), mUsername.mPos);
        shift = escUsername.Length() + 1;
    }
    else
        shift = ReplaceSegment(mUsername.mPos, mUsername.mLen, escUsername);

    if (shift) {
        mUsername.mLen = escUsername.Length();
        mAuthority.mLen += shift;
        ShiftFromPassword(shift);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetPassword(const nsACString &input)
{
    ENSURE_MUTABLE();

    const nsPromiseFlatCString &password = PromiseFlatCString(input);

    LOG(("nsStandardURL::SetPassword [password=%s]\n", password.get()));

    if (mURLType == URLTYPE_NO_AUTHORITY) {
        if (password.IsEmpty())
            return NS_OK;
        NS_WARNING("cannot set password on no-auth url");
        return NS_ERROR_UNEXPECTED;
    }
    if (mUsername.mLen <= 0) {
        NS_WARNING("cannot set password without existing username");
        return NS_ERROR_FAILURE;
    }

    if (mSpec.Length() + input.Length() - Password().Length() > (uint32_t) net_GetURLMaxLength()) {
        return NS_ERROR_MALFORMED_URI;
    }

    InvalidateCache();

    if (password.IsEmpty()) {
        if (mPassword.mLen >= 0) {
            // cut(":password")
            mSpec.Cut(mPassword.mPos - 1, mPassword.mLen + 1);
            ShiftFromHost(-(mPassword.mLen + 1));
            mAuthority.mLen -= (mPassword.mLen + 1);
            mPassword.mLen = -1;
        }
        return NS_OK;
    }

    // escape password if necessary
    nsAutoCString buf;
    GET_SEGMENT_ENCODER(encoder);
    const nsACString &escPassword =
        encoder.EncodeSegment(password, esc_Password, buf);

    int32_t shift;

    if (mPassword.mLen < 0) {
        mPassword.mPos = mUsername.mPos + mUsername.mLen + 1;
        mSpec.Insert(NS_LITERAL_CSTRING(":") + escPassword, mPassword.mPos - 1);
        shift = escPassword.Length() + 1;
    }
    else
        shift = ReplaceSegment(mPassword.mPos, mPassword.mLen, escPassword);

    if (shift) {
        mPassword.mLen = escPassword.Length();
        mAuthority.mLen += shift;
        ShiftFromHost(shift);
    }
    return NS_OK;
}

void
nsStandardURL::FindHostLimit(nsACString::const_iterator& aStart,
                             nsACString::const_iterator& aEnd)
{
  for (int32_t i = 0; gHostLimitDigits[i]; ++i) {
    nsACString::const_iterator c(aStart);
    if (FindCharInReadable(gHostLimitDigits[i], c, aEnd)) {
      aEnd = c;
    }
  }
}

NS_IMETHODIMP
nsStandardURL::SetHostPort(const nsACString &aValue)
{
    ENSURE_MUTABLE();

    // We cannot simply call nsIURI::SetHost because that would treat the name as
    // an IPv6 address (like http:://[server:443]/).  We also cannot call
    // nsIURI::SetHostPort because that isn't implemented.  Sadfaces.

    nsACString::const_iterator start, end;
    aValue.BeginReading(start);
    aValue.EndReading(end);
    nsACString::const_iterator iter(start);
    bool isIPv6 = false;

    FindHostLimit(start, end);

    if (*start == '[') { // IPv6 address
        if (!FindCharInReadable(']', iter, end)) {
            // the ] character is missing
            return NS_ERROR_MALFORMED_URI;
        }
        // iter now at the ']' character
        isIPv6 = true;
    } else {
        nsACString::const_iterator iter2(start);
        if (FindCharInReadable(']', iter2, end)) {
            // if the first char isn't [ then there should be no ] character
            return NS_ERROR_MALFORMED_URI;
        }
    }

    FindCharInReadable(':', iter, end);

    if (!isIPv6 && iter != end) {
        nsACString::const_iterator iter2(iter);
        iter2++; // Skip over the first ':' character
        if (FindCharInReadable(':', iter2, end)) {
            // If there is more than one ':' character it suggests an IPv6
            // The format should be [2001::1]:80 where the port is optional
            return NS_ERROR_MALFORMED_URI;
        }
    }

    nsresult rv = SetHost(Substring(start, iter));
    NS_ENSURE_SUCCESS(rv, rv);

    // Also set the port if needed.
    if (iter != end) {
        iter++;
        if (iter != end) {
            nsCString portStr(Substring(iter, end));
            nsresult rv;
            int32_t port = portStr.ToInteger(&rv);
            if (NS_SUCCEEDED(rv)) {
                rv = SetPort(port);
                NS_ENSURE_SUCCESS(rv, rv);
            } else {
                // Failure parsing port number
                return NS_ERROR_MALFORMED_URI;
            }
        } else {
            // port number is missing
            return NS_ERROR_MALFORMED_URI;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetHost(const nsACString &input)
{
    ENSURE_MUTABLE();

    const nsPromiseFlatCString &hostname = PromiseFlatCString(input);

    nsACString::const_iterator start, end;
    hostname.BeginReading(start);
    hostname.EndReading(end);

    FindHostLimit(start, end);

    const nsCString unescapedHost(Substring(start, end));
    // Do percent decoding on the the input.
    nsAutoCString flat;
    NS_UnescapeURL(unescapedHost.BeginReading(), unescapedHost.Length(),
                   esc_AlwaysCopy | esc_Host, flat);
    const char *host = flat.get();

    LOG(("nsStandardURL::SetHost [host=%s]\n", host));

    if (mURLType == URLTYPE_NO_AUTHORITY) {
        if (flat.IsEmpty())
            return NS_OK;
        NS_WARNING("cannot set host on no-auth url");
        return NS_ERROR_UNEXPECTED;
    } else {
        if (flat.IsEmpty()) {
            // Setting an empty hostname is not allowed for
            // URLTYPE_STANDARD and URLTYPE_AUTHORITY.
            return NS_ERROR_UNEXPECTED;
        }
    }

    if (strlen(host) < flat.Length())
        return NS_ERROR_MALFORMED_URI; // found embedded null

    // For consistency with SetSpec/nsURLParsers, don't allow spaces
    // in the hostname.
    if (strchr(host, ' '))
        return NS_ERROR_MALFORMED_URI;

    if (mSpec.Length() + strlen(host) - Host().Length() > (uint32_t) net_GetURLMaxLength()) {
        return NS_ERROR_MALFORMED_URI;
    }

    InvalidateCache();
    mHostEncoding = eEncoding_ASCII;

    uint32_t len;
    nsAutoCString hostBuf;
    nsresult rv = NormalizeIDN(flat, hostBuf);
    if (NS_FAILED(rv)) {
        return rv;
    }

    // NormalizeIDN always copies if the call was successful
    host = hostBuf.get();
    len = hostBuf.Length();

    if (!ValidIPv6orHostname(host, len)) {
        return NS_ERROR_MALFORMED_URI;
    }

    if (mHost.mLen < 0) {
        int port_length = 0;
        if (mPort != -1) {
            nsAutoCString buf;
            buf.Assign(':');
            buf.AppendInt(mPort);
            port_length = buf.Length();
        }
        if (mAuthority.mLen > 0) {
            mHost.mPos = mAuthority.mPos + mAuthority.mLen - port_length;
            mHost.mLen = 0;
        } else if (mScheme.mLen > 0) {
            mHost.mPos = mScheme.mPos + mScheme.mLen + 3;
            mHost.mLen = 0;
        }
    }

    int32_t shift = ReplaceSegment(mHost.mPos, mHost.mLen, host, len);

    if (shift) {
        mHost.mLen = len;
        mAuthority.mLen += shift;
        ShiftFromPath(shift);
    }

    // Now canonicalize the host to lowercase
    net_ToLowerCase(mSpec.BeginWriting() + mHost.mPos, mHost.mLen);

    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetPort(int32_t port)
{
    ENSURE_MUTABLE();

    LOG(("nsStandardURL::SetPort [port=%d]\n", port));

    if ((port == mPort) || (mPort == -1 && port == mDefaultPort))
        return NS_OK;

    // ports must be >= 0
    if (port < -1) // -1 == use default
        return NS_ERROR_MALFORMED_URI;

    if (mURLType == URLTYPE_NO_AUTHORITY) {
        NS_WARNING("cannot set port on no-auth url");
        return NS_ERROR_UNEXPECTED;
    }

    InvalidateCache();
    if (port == mDefaultPort) {
      port = -1;
    }

    ReplacePortInSpec(port);

    mPort = port;
    return NS_OK;
}

/**
 * Replaces the existing port in mSpec with aNewPort.
 *
 * The caller is responsible for:
 *  - Calling InvalidateCache (since our mSpec is changing).
 *  - Checking whether aNewPort is mDefaultPort (in which case the
 *    caller should pass aNewPort=-1).
 */
void
nsStandardURL::ReplacePortInSpec(int32_t aNewPort)
{
    MOZ_ASSERT(mMutable, "Caller should ensure we're mutable");
    NS_ASSERTION(aNewPort != mDefaultPort || mDefaultPort == -1,
                 "Caller should check its passed-in value and pass -1 instead of "
                 "mDefaultPort, to avoid encoding default port into mSpec");

    // Create the (possibly empty) string that we're planning to replace:
    nsAutoCString buf;
    if (mPort != -1) {
        buf.Assign(':');
        buf.AppendInt(mPort);
    }
    // Find the position & length of that string:
    const uint32_t replacedLen = buf.Length();
    const uint32_t replacedStart =
        mAuthority.mPos + mAuthority.mLen - replacedLen;

    // Create the (possibly empty) replacement string:
    if (aNewPort == -1) {
        buf.Truncate();
    } else {
        buf.Assign(':');
        buf.AppendInt(aNewPort);
    }
    // Perform the replacement:
    mSpec.Replace(replacedStart, replacedLen, buf);

    // Bookkeeping to reflect the new length:
    int32_t shift = buf.Length() - replacedLen;
    mAuthority.mLen += shift;
    ShiftFromPath(shift);
}

NS_IMETHODIMP
nsStandardURL::SetPath(const nsACString &input)
{
    ENSURE_MUTABLE();

    const nsPromiseFlatCString &path = PromiseFlatCString(input);
    LOG(("nsStandardURL::SetPath [path=%s]\n", path.get()));

    InvalidateCache();

    if (!path.IsEmpty()) {
        nsAutoCString spec;

        spec.Assign(mSpec.get(), mPath.mPos);
        if (path.First() != '/')
            spec.Append('/');
        spec.Append(path);

        return SetSpec(spec);
    }
    else if (mPath.mLen >= 1) {
        mSpec.Cut(mPath.mPos + 1, mPath.mLen - 1);
        // these contain only a '/'
        mPath.mLen = 1;
        mDirectory.mLen = 1;
        mFilepath.mLen = 1;
        // these are no longer defined
        mBasename.mLen = -1;
        mExtension.mLen = -1;
        mQuery.mLen = -1;
        mRef.mLen = -1;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::Equals(nsIURI *unknownOther, bool *result)
{
    return EqualsInternal(unknownOther, eHonorRef, result);
}

NS_IMETHODIMP
nsStandardURL::EqualsExceptRef(nsIURI *unknownOther, bool *result)
{
    return EqualsInternal(unknownOther, eIgnoreRef, result);
}

nsresult
nsStandardURL::EqualsInternal(nsIURI *unknownOther,
                              nsStandardURL::RefHandlingEnum refHandlingMode,
                              bool *result)
{
    NS_ENSURE_ARG_POINTER(unknownOther);
    NS_PRECONDITION(result, "null pointer");

    RefPtr<nsStandardURL> other;
    nsresult rv = unknownOther->QueryInterface(kThisImplCID,
                                               getter_AddRefs(other));
    if (NS_FAILED(rv)) {
        *result = false;
        return NS_OK;
    }

    // First, check whether one URIs is an nsIFileURL while the other
    // is not.  If that's the case, they're different.
    if (mSupportsFileURL != other->mSupportsFileURL) {
        *result = false;
        return NS_OK;
    }

    // Next check parts of a URI that, if different, automatically make the
    // URIs different
    if (!SegmentIs(mScheme, other->mSpec.get(), other->mScheme) ||
        // Check for host manually, since conversion to file will
        // ignore the host!
        !SegmentIs(mHost, other->mSpec.get(), other->mHost) ||
        !SegmentIs(mQuery, other->mSpec.get(), other->mQuery) ||
        !SegmentIs(mUsername, other->mSpec.get(), other->mUsername) ||
        !SegmentIs(mPassword, other->mSpec.get(), other->mPassword) ||
        Port() != other->Port()) {
        // No need to compare files or other URI parts -- these are different
        // beasties
        *result = false;
        return NS_OK;
    }

    if (refHandlingMode == eHonorRef &&
        !SegmentIs(mRef, other->mSpec.get(), other->mRef)) {
        *result = false;
        return NS_OK;
    }
    
    // Then check for exact identity of URIs.  If we have it, they're equal
    if (SegmentIs(mDirectory, other->mSpec.get(), other->mDirectory) &&
        SegmentIs(mBasename, other->mSpec.get(), other->mBasename) &&
        SegmentIs(mExtension, other->mSpec.get(), other->mExtension)) {
        *result = true;
        return NS_OK;
    }

    // At this point, the URIs are not identical, but they only differ in the
    // directory/filename/extension.  If these are file URLs, then get the
    // corresponding file objects and compare those, since two filenames that
    // differ, eg, only in case could still be equal.
    if (mSupportsFileURL) {
        // Assume not equal for failure cases... but failures in GetFile are
        // really failures, more or less, so propagate them to caller.
        *result = false;

        rv = EnsureFile();
        nsresult rv2 = other->EnsureFile();
        // special case for resource:// urls that don't resolve to files
        if (rv == NS_ERROR_NO_INTERFACE && rv == rv2) 
            return NS_OK;
        
        if (NS_FAILED(rv)) {
            LOG(("nsStandardURL::Equals [this=%p spec=%s] failed to ensure file",
                this, mSpec.get()));
            return rv;
        }
        NS_ASSERTION(mFile, "EnsureFile() lied!");
        rv = rv2;
        if (NS_FAILED(rv)) {
            LOG(("nsStandardURL::Equals [other=%p spec=%s] other failed to ensure file",
                 other.get(), other->mSpec.get()));
            return rv;
        }
        NS_ASSERTION(other->mFile, "EnsureFile() lied!");
        return mFile->Equals(other->mFile, result);
    }

    // The URLs are not identical, and they do not correspond to the
    // same file, so they are different.
    *result = false;

    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SchemeIs(const char *scheme, bool *result)
{
    NS_PRECONDITION(result, "null pointer");

    *result = SegmentIs(mScheme, scheme);
    return NS_OK;
}

/* virtual */ nsStandardURL*
nsStandardURL::StartClone()
{
    nsStandardURL *clone = new nsStandardURL();
    return clone;
}

NS_IMETHODIMP
nsStandardURL::Clone(nsIURI **result)
{
    return CloneInternal(eHonorRef, result);
}


NS_IMETHODIMP
nsStandardURL::CloneIgnoringRef(nsIURI **result)
{
    return CloneInternal(eIgnoreRef, result);
}

nsresult
nsStandardURL::CloneInternal(nsStandardURL::RefHandlingEnum refHandlingMode,
                             nsIURI **result)

{
    RefPtr<nsStandardURL> clone = StartClone();
    if (!clone)
        return NS_ERROR_OUT_OF_MEMORY;

    // Copy local members into clone.
    // Also copies the cached members mFile, mHostA
    clone->CopyMembers(this, refHandlingMode, true);

    clone.forget(result);
    return NS_OK;
}

nsresult nsStandardURL::CopyMembers(nsStandardURL * source,
    nsStandardURL::RefHandlingEnum refHandlingMode, bool copyCached)
{
    mSpec = source->mSpec;
    mDefaultPort = source->mDefaultPort;
    mPort = source->mPort;
    mScheme = source->mScheme;
    mAuthority = source->mAuthority;
    mUsername = source->mUsername;
    mPassword = source->mPassword;
    mHost = source->mHost;
    mPath = source->mPath;
    mFilepath = source->mFilepath;
    mDirectory = source->mDirectory;
    mBasename = source->mBasename;
    mExtension = source->mExtension;
    mQuery = source->mQuery;
    mRef = source->mRef;
    mOriginCharset = source->mOriginCharset;
    mURLType = source->mURLType;
    mParser = source->mParser;
    mMutable = true;
    mSupportsFileURL = source->mSupportsFileURL;
    mHostEncoding = source->mHostEncoding;

    if (copyCached) {
        mFile = source->mFile;
        mHostA = source->mHostA ? strdup(source->mHostA) : nullptr;
        mSpecEncoding = source->mSpecEncoding;
    } else {
        // The same state as after calling InvalidateCache()
        mFile = nullptr;
        mHostA = nullptr;
        mSpecEncoding = eEncoding_Unknown;
    }

    if (refHandlingMode == eIgnoreRef) {
        SetRef(EmptyCString());
    }

    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::Resolve(const nsACString &in, nsACString &out)
{
    const nsPromiseFlatCString &flat = PromiseFlatCString(in);
    const char *relpath = flat.get();

    // filter out unexpected chars "\r\n\t" if necessary
    nsAutoCString buf;
    int32_t relpathLen;
    if (!net_FilterURIString(relpath, buf)) {
        // Copy the content into filteredURI even if no whitespace was stripped.
        // We need a non-const buffer to perform backslash replacement.
        buf = in;
    }

    relpath = buf.get();
    relpathLen = buf.Length();

    char *result = nullptr;

    LOG(("nsStandardURL::Resolve [this=%p spec=%s relpath=%s]\n",
        this, mSpec.get(), relpath));

    NS_ASSERTION(mParser, "no parser: unitialized");

    // NOTE: there is no need for this function to produce normalized
    // output.  normalization will occur when the result is used to 
    // initialize a nsStandardURL object.

    if (mScheme.mLen < 0) {
        NS_WARNING("unable to Resolve URL: this URL not initialized");
        return NS_ERROR_NOT_INITIALIZED;
    }

    nsresult rv;
    URLSegment scheme;
    char *resultPath = nullptr;
    bool relative = false;
    uint32_t offset = 0;
    netCoalesceFlags coalesceFlag = NET_COALESCE_NORMAL;

    // relative urls should never contain a host, so we always want to use
    // the noauth url parser.
    // use it to extract a possible scheme
    rv = mParser->ParseURL(relpath, 
                           relpathLen,
                           &scheme.mPos, &scheme.mLen,
                           nullptr, nullptr,
                           nullptr, nullptr);

    // if the parser fails (for example because there is no valid scheme)
    // reset the scheme and assume a relative url
    if (NS_FAILED(rv)) scheme.Reset(); 

    nsAutoCString protocol(Segment(scheme));
    nsAutoCString baseProtocol(Scheme());

    // We need to do backslash replacement for the following cases:
    // 1. The input is an absolute path with a http/https/ftp scheme
    // 2. The input is a relative path, and the base URL has a http/https/ftp scheme
    if ((protocol.IsEmpty() && IsSpecialProtocol(baseProtocol)) ||
         IsSpecialProtocol(protocol)) {

        nsAutoCString::iterator start;
        nsAutoCString::iterator end;
        buf.BeginWriting(start);
        buf.EndWriting(end);
        while (start != end) {
            if (*start == '?' || *start == '#') {
                break;
            }
            if (*start == '\\') {
                *start = '/';
            }
            start++;
        }
    }

    if (scheme.mLen >= 0) {
        // add some flags to coalesceFlag if it is an ftp-url
        // need this later on when coalescing the resulting URL
        if (SegmentIs(relpath, scheme, "ftp", true)) {
            coalesceFlag = (netCoalesceFlags) (coalesceFlag 
                                        | NET_COALESCE_ALLOW_RELATIVE_ROOT
                                        | NET_COALESCE_DOUBLE_SLASH_IS_ROOT);

        }
        // this URL appears to be absolute
        // but try to find out more
        if (SegmentIs(mScheme, relpath, scheme, true)) {
            // mScheme and Scheme are the same 
            // but this can still be relative
            if (nsCRT::strncmp(relpath + scheme.mPos + scheme.mLen,
                               "://",3) == 0) {
                // now this is really absolute
                // because a :// follows the scheme 
                result = NS_strdup(relpath);
            } else {         
                // This is a deprecated form of relative urls like
                // http:file or http:/path/file
                // we will support it for now ...
                relative = true;
                offset = scheme.mLen + 1;
            }
        } else {
            // the schemes are not the same, we are also done
            // because we have to assume this is absolute 
            result = NS_strdup(relpath);
        }  
    } else {
        // add some flags to coalesceFlag if it is an ftp-url
        // need this later on when coalescing the resulting URL
        if (SegmentIs(mScheme,"ftp")) {
            coalesceFlag = (netCoalesceFlags) (coalesceFlag 
                                        | NET_COALESCE_ALLOW_RELATIVE_ROOT
                                        | NET_COALESCE_DOUBLE_SLASH_IS_ROOT);
        }
        if (relpath[0] == '/' && relpath[1] == '/') {
            // this URL //host/path is almost absolute
            result = AppendToSubstring(mScheme.mPos, mScheme.mLen + 1, relpath);
        } else {
            // then it must be relative 
            relative = true;
        }
    }
    if (relative) {
        uint32_t len = 0;
        const char *realrelpath = relpath + offset;
        switch (*realrelpath) {
        case '/':
            // overwrite everything after the authority
            len = mAuthority.mPos + mAuthority.mLen;
            break;
        case '?':
            // overwrite the existing ?query and #ref
            if (mQuery.mLen >= 0)
                len = mQuery.mPos - 1;
            else if (mRef.mLen >= 0)
                len = mRef.mPos - 1;
            else
                len = mPath.mPos + mPath.mLen;
            break;
        case '#':
        case '\0':
            // overwrite the existing #ref
            if (mRef.mLen < 0)
                len = mPath.mPos + mPath.mLen;
            else
                len = mRef.mPos - 1;
            break;
        default:
            if (coalesceFlag & NET_COALESCE_DOUBLE_SLASH_IS_ROOT) {
                if (Filename().Equals(NS_LITERAL_CSTRING("%2F"),
                                      nsCaseInsensitiveCStringComparator())) {
                    // if ftp URL ends with %2F then simply
                    // append relative part because %2F also
                    // marks the root directory with ftp-urls
                    len = mFilepath.mPos + mFilepath.mLen;
                } else {
                    // overwrite everything after the directory 
                    len = mDirectory.mPos + mDirectory.mLen;
                }
            } else { 
                // overwrite everything after the directory 
                len = mDirectory.mPos + mDirectory.mLen;
            }
        }
        result = AppendToSubstring(0, len, realrelpath);
        // locate result path
        resultPath = result + mPath.mPos;
    }
    if (!result)
        return NS_ERROR_OUT_OF_MEMORY;

    if (resultPath)
        net_CoalesceDirs(coalesceFlag, resultPath);
    else {
        // locate result path
        resultPath = PL_strstr(result, "://");
        if (resultPath) {
            resultPath = PL_strchr(resultPath + 3, '/');
            if (resultPath)
                net_CoalesceDirs(coalesceFlag,resultPath);
        }
    }
    out.Adopt(result);
    return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsStandardURL::GetCommonBaseSpec(nsIURI *uri2, nsACString &aResult)
{
    NS_ENSURE_ARG_POINTER(uri2);

    // if uri's are equal, then return uri as is
    bool isEquals = false;
    if (NS_SUCCEEDED(Equals(uri2, &isEquals)) && isEquals)
        return GetSpec(aResult);

    aResult.Truncate();

    // check pre-path; if they don't match, then return empty string
    nsStandardURL *stdurl2;
    nsresult rv = uri2->QueryInterface(kThisImplCID, (void **) &stdurl2);
    isEquals = NS_SUCCEEDED(rv)
            && SegmentIs(mScheme, stdurl2->mSpec.get(), stdurl2->mScheme)    
            && SegmentIs(mHost, stdurl2->mSpec.get(), stdurl2->mHost)
            && SegmentIs(mUsername, stdurl2->mSpec.get(), stdurl2->mUsername)
            && SegmentIs(mPassword, stdurl2->mSpec.get(), stdurl2->mPassword)
            && (Port() == stdurl2->Port());
    if (!isEquals)
    {
        if (NS_SUCCEEDED(rv))
            NS_RELEASE(stdurl2);
        return NS_OK;
    }

    // scan for first mismatched character
    const char *thisIndex, *thatIndex, *startCharPos;
    startCharPos = mSpec.get() + mDirectory.mPos;
    thisIndex = startCharPos;
    thatIndex = stdurl2->mSpec.get() + mDirectory.mPos;
    while ((*thisIndex == *thatIndex) && *thisIndex)
    {
        thisIndex++;
        thatIndex++;
    }

    // backup to just after previous slash so we grab an appropriate path
    // segment such as a directory (not partial segments)
    // todo:  also check for file matches which include '?' and '#'
    while ((thisIndex != startCharPos) && (*(thisIndex-1) != '/'))
        thisIndex--;

    // grab spec from beginning to thisIndex
    aResult = Substring(mSpec, mScheme.mPos, thisIndex - mSpec.get());

    NS_RELEASE(stdurl2);
    return rv;
}

NS_IMETHODIMP
nsStandardURL::GetRelativeSpec(nsIURI *uri2, nsACString &aResult)
{
    NS_ENSURE_ARG_POINTER(uri2);

    aResult.Truncate();

    // if uri's are equal, then return empty string
    bool isEquals = false;
    if (NS_SUCCEEDED(Equals(uri2, &isEquals)) && isEquals)
        return NS_OK;

    nsStandardURL *stdurl2;
    nsresult rv = uri2->QueryInterface(kThisImplCID, (void **) &stdurl2);
    isEquals = NS_SUCCEEDED(rv)
            && SegmentIs(mScheme, stdurl2->mSpec.get(), stdurl2->mScheme)    
            && SegmentIs(mHost, stdurl2->mSpec.get(), stdurl2->mHost)
            && SegmentIs(mUsername, stdurl2->mSpec.get(), stdurl2->mUsername)
            && SegmentIs(mPassword, stdurl2->mSpec.get(), stdurl2->mPassword)
            && (Port() == stdurl2->Port());
    if (!isEquals)
    {
        if (NS_SUCCEEDED(rv))
            NS_RELEASE(stdurl2);

        return uri2->GetSpec(aResult);
    }

    // scan for first mismatched character
    const char *thisIndex, *thatIndex, *startCharPos;
    startCharPos = mSpec.get() + mDirectory.mPos;
    thisIndex = startCharPos;
    thatIndex = stdurl2->mSpec.get() + mDirectory.mPos;

#ifdef XP_WIN
    bool isFileScheme = SegmentIs(mScheme, "file");
    if (isFileScheme)
    {
        // on windows, we need to match the first segment of the path
        // if these don't match then we need to return an absolute path
        // skip over any leading '/' in path
        while ((*thisIndex == *thatIndex) && (*thisIndex == '/'))
        {
            thisIndex++;
            thatIndex++;
        }
        // look for end of first segment
        while ((*thisIndex == *thatIndex) && *thisIndex && (*thisIndex != '/'))
        {
            thisIndex++;
            thatIndex++;
        }

        // if we didn't match through the first segment, return absolute path
        if ((*thisIndex != '/') || (*thatIndex != '/'))
        {
            NS_RELEASE(stdurl2);
            return uri2->GetSpec(aResult);
        }
    }
#endif

    while ((*thisIndex == *thatIndex) && *thisIndex)
    {
        thisIndex++;
        thatIndex++;
    }

    // backup to just after previous slash so we grab an appropriate path
    // segment such as a directory (not partial segments)
    // todo:  also check for file matches with '#' and '?'
    while ((*(thatIndex-1) != '/') && (thatIndex != startCharPos))
        thatIndex--;

    const char *limit = mSpec.get() + mFilepath.mPos + mFilepath.mLen;

    // need to account for slashes and add corresponding "../"
    for (; thisIndex <= limit && *thisIndex; ++thisIndex)
    {
        if (*thisIndex == '/')
            aResult.AppendLiteral("../");
    }

    // grab spec from thisIndex to end
    uint32_t startPos = stdurl2->mScheme.mPos + thatIndex - stdurl2->mSpec.get();
    aResult.Append(Substring(stdurl2->mSpec, startPos, 
                             stdurl2->mSpec.Length() - startPos));

    NS_RELEASE(stdurl2);
    return rv;
}

//----------------------------------------------------------------------------
// nsStandardURL::nsIURL
//----------------------------------------------------------------------------

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsStandardURL::GetFilePath(nsACString &result)
{
    result = Filepath();
    return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsStandardURL::GetQuery(nsACString &result)
{
    result = Query();
    return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsStandardURL::GetRef(nsACString &result)
{
    result = Ref();
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::GetHasRef(bool *result)
{
    *result = (mRef.mLen >= 0);
    return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsStandardURL::GetDirectory(nsACString &result)
{
    result = Directory();
    return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsStandardURL::GetFileName(nsACString &result)
{
    result = Filename();
    return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsStandardURL::GetFileBaseName(nsACString &result)
{
    result = Basename();
    return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsStandardURL::GetFileExtension(nsACString &result)
{
    result = Extension();
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetFilePath(const nsACString &input)
{
    ENSURE_MUTABLE();

    const nsPromiseFlatCString &flat = PromiseFlatCString(input);
    const char *filepath = flat.get();

    LOG(("nsStandardURL::SetFilePath [filepath=%s]\n", filepath));

    // if there isn't a filepath, then there can't be anything
    // after the path either.  this url is likely uninitialized.
    if (mFilepath.mLen < 0)
        return SetPath(flat);

    if (filepath && *filepath) {
        nsAutoCString spec;
        uint32_t dirPos, basePos, extPos;
        int32_t dirLen, baseLen, extLen;
        nsresult rv;

        rv = mParser->ParseFilePath(filepath, -1,
                                    &dirPos, &dirLen,
                                    &basePos, &baseLen,
                                    &extPos, &extLen);
        if (NS_FAILED(rv)) return rv;

        // build up new candidate spec
        spec.Assign(mSpec.get(), mPath.mPos);

        // ensure leading '/'
        if (filepath[dirPos] != '/')
            spec.Append('/');

        GET_SEGMENT_ENCODER(encoder);

        // append encoded filepath components
        if (dirLen > 0)
            encoder.EncodeSegment(Substring(filepath + dirPos,
                                            filepath + dirPos + dirLen),
                                  esc_Directory | esc_AlwaysCopy, spec);
        if (baseLen > 0)
            encoder.EncodeSegment(Substring(filepath + basePos,
                                            filepath + basePos + baseLen),
                                  esc_FileBaseName | esc_AlwaysCopy, spec);
        if (extLen >= 0) {
            spec.Append('.');
            if (extLen > 0)
                encoder.EncodeSegment(Substring(filepath + extPos,
                                                filepath + extPos + extLen),
                                      esc_FileExtension | esc_AlwaysCopy,
                                      spec);
        }

        // compute the ending position of the current filepath
        if (mFilepath.mLen >= 0) {
            uint32_t end = mFilepath.mPos + mFilepath.mLen;
            if (mSpec.Length() > end)
                spec.Append(mSpec.get() + end, mSpec.Length() - end);
        }

        return SetSpec(spec);
    }
    else if (mPath.mLen > 1) {
        mSpec.Cut(mPath.mPos + 1, mFilepath.mLen - 1);
        // left shift query, and ref
        ShiftFromQuery(1 - mFilepath.mLen);
        // these contain only a '/'
        mPath.mLen = 1;
        mDirectory.mLen = 1;
        mFilepath.mLen = 1;
        // these are no longer defined
        mBasename.mLen = -1;
        mExtension.mLen = -1;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetQuery(const nsACString &input)
{
    ENSURE_MUTABLE();

    const nsPromiseFlatCString &flat = PromiseFlatCString(input);
    const char *query = flat.get();

    LOG(("nsStandardURL::SetQuery [query=%s]\n", query));

    if (mPath.mLen < 0)
        return SetPath(flat);

    if (mSpec.Length() + input.Length() - Query().Length() > (uint32_t) net_GetURLMaxLength()) {
        return NS_ERROR_MALFORMED_URI;
    }

    InvalidateCache();

    if (!query || !*query) {
        // remove existing query
        if (mQuery.mLen >= 0) {
            // remove query and leading '?'
            mSpec.Cut(mQuery.mPos - 1, mQuery.mLen + 1);
            ShiftFromRef(-(mQuery.mLen + 1));
            mPath.mLen -= (mQuery.mLen + 1);
            mQuery.mPos = 0;
            mQuery.mLen = -1;
        }
        return NS_OK;
    }

    int32_t queryLen = strlen(query);
    if (query[0] == '?') {
        query++;
        queryLen--;
    }

    if (mQuery.mLen < 0) {
        if (mRef.mLen < 0)
            mQuery.mPos = mSpec.Length();
        else
            mQuery.mPos = mRef.mPos - 1;
        mSpec.Insert('?', mQuery.mPos);
        mQuery.mPos++;
        mQuery.mLen = 0;
        // the insertion pushes these out by 1
        mPath.mLen++;
        mRef.mPos++;
    }

    // encode query if necessary
    nsAutoCString buf;
    bool encoded;
    GET_QUERY_ENCODER(encoder);
    encoder.EncodeSegmentCount(query, URLSegment(0, queryLen), esc_Query,
                               buf, encoded);
    if (encoded) {
        query = buf.get();
        queryLen = buf.Length();
    }

    int32_t shift = ReplaceSegment(mQuery.mPos, mQuery.mLen, query, queryLen);

    if (shift) {
        mQuery.mLen = queryLen;
        mPath.mLen += shift;
        ShiftFromRef(shift);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetRef(const nsACString &input)
{
    ENSURE_MUTABLE();

    const nsPromiseFlatCString &flat = PromiseFlatCString(input);
    const char *ref = flat.get();

    LOG(("nsStandardURL::SetRef [ref=%s]\n", ref));

    if (input.Contains('\0')) {
        return NS_ERROR_MALFORMED_URI;
    }

    if (mPath.mLen < 0)
        return SetPath(flat);

    if (mSpec.Length() + input.Length() - Ref().Length() > (uint32_t) net_GetURLMaxLength()) {
        return NS_ERROR_MALFORMED_URI;
    }

    InvalidateCache();

    if (!ref || !*ref) {
        // remove existing ref
        if (mRef.mLen >= 0) {
            // remove ref and leading '#'
            mSpec.Cut(mRef.mPos - 1, mRef.mLen + 1);
            mPath.mLen -= (mRef.mLen + 1);
            mRef.mPos = 0;
            mRef.mLen = -1;
        }
        return NS_OK;
    }

    int32_t refLen = flat.Length();
    if (ref[0] == '#') {
        ref++;
        refLen--;
    }
    
    if (mRef.mLen < 0) {
        mSpec.Append('#');
        ++mPath.mLen;  // Include the # in the path.
        mRef.mPos = mSpec.Length();
        mRef.mLen = 0;
    }

    // If precent encoding is necessary, `ref` will point to `buf`'s content.
    // `buf` needs to outlive any use of the `ref` pointer.
    nsAutoCString buf;
    if (nsContentUtils::EncodeDecodeURLHash()) {
        // encode ref if necessary
        bool encoded;
        GET_SEGMENT_ENCODER(encoder);
        encoder.EncodeSegmentCount(ref, URLSegment(0, refLen), esc_Ref,
                                   buf, encoded);
        if (encoded) {
            ref = buf.get();
            refLen = buf.Length();
        }
    }

    int32_t shift = ReplaceSegment(mRef.mPos, mRef.mLen, ref, refLen);
    mPath.mLen += shift;
    mRef.mLen = refLen;
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetDirectory(const nsACString &input)
{
    NS_NOTYETIMPLEMENTED("");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStandardURL::SetFileName(const nsACString &input)
{
    ENSURE_MUTABLE();

    const nsPromiseFlatCString &flat = PromiseFlatCString(input);
    const char *filename = flat.get();

    LOG(("nsStandardURL::SetFileName [filename=%s]\n", filename));

    if (mPath.mLen < 0)
        return SetPath(flat);

    if (mSpec.Length() + input.Length() - Filename().Length() > (uint32_t) net_GetURLMaxLength()) {
        return NS_ERROR_MALFORMED_URI;
    }

    int32_t shift = 0;

    if (!(filename && *filename)) {
        // remove the filename
        if (mBasename.mLen > 0) {
            if (mExtension.mLen >= 0)
                mBasename.mLen += (mExtension.mLen + 1);
            mSpec.Cut(mBasename.mPos, mBasename.mLen);
            shift = -mBasename.mLen;
            mBasename.mLen = 0;
            mExtension.mLen = -1;
        }
    }
    else {
        nsresult rv;
        URLSegment basename, extension;

        // let the parser locate the basename and extension
        rv = mParser->ParseFileName(filename, -1,
                                    &basename.mPos, &basename.mLen,
                                    &extension.mPos, &extension.mLen);
        if (NS_FAILED(rv)) return rv;

        if (basename.mLen < 0) {
            // remove existing filename
            if (mBasename.mLen >= 0) {
                uint32_t len = mBasename.mLen;
                if (mExtension.mLen >= 0)
                    len += (mExtension.mLen + 1);
                mSpec.Cut(mBasename.mPos, len);
                shift = -int32_t(len);
                mBasename.mLen = 0;
                mExtension.mLen = -1;
            }
        }
        else {
            nsAutoCString newFilename;
            bool ignoredOut;
            GET_SEGMENT_ENCODER(encoder);
            basename.mLen = encoder.EncodeSegmentCount(filename, basename,
                                                       esc_FileBaseName |
                                                       esc_AlwaysCopy,
                                                       newFilename,
                                                       ignoredOut);
            if (extension.mLen >= 0) {
                newFilename.Append('.');
                extension.mLen = encoder.EncodeSegmentCount(filename, extension,
                                                            esc_FileExtension |
                                                            esc_AlwaysCopy,
                                                            newFilename,
                                                            ignoredOut);
            }

            if (mBasename.mLen < 0) {
                // insert new filename
                mBasename.mPos = mDirectory.mPos + mDirectory.mLen;
                mSpec.Insert(newFilename, mBasename.mPos);
                shift = newFilename.Length();
            }
            else {
                // replace existing filename
                uint32_t oldLen = uint32_t(mBasename.mLen);
                if (mExtension.mLen >= 0)
                    oldLen += (mExtension.mLen + 1);
                mSpec.Replace(mBasename.mPos, oldLen, newFilename);
                shift = newFilename.Length() - oldLen;
            }
            
            mBasename.mLen = basename.mLen;
            mExtension.mLen = extension.mLen;
            if (mExtension.mLen >= 0)
                mExtension.mPos = mBasename.mPos + mBasename.mLen + 1;
        }
    }
    if (shift) {
        ShiftFromQuery(shift);
        mFilepath.mLen += shift;
        mPath.mLen += shift;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetFileBaseName(const nsACString &input)
{
    nsAutoCString extension;
    nsresult rv = GetFileExtension(extension);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString newFileName(input);

    if (!extension.IsEmpty()) {
        newFileName.Append('.');
        newFileName.Append(extension);
    }

    return SetFileName(newFileName);
}

NS_IMETHODIMP
nsStandardURL::SetFileExtension(const nsACString &input)
{
    nsAutoCString newFileName;
    nsresult rv = GetFileBaseName(newFileName);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!input.IsEmpty()) {
        newFileName.Append('.');
        newFileName.Append(input);
    }

    return SetFileName(newFileName);
}

//----------------------------------------------------------------------------
// nsStandardURL::nsIFileURL
//----------------------------------------------------------------------------

nsresult
nsStandardURL::EnsureFile()
{
    NS_PRECONDITION(mSupportsFileURL,
                    "EnsureFile() called on a URL that doesn't support files!");
    if (mFile) {
        // Nothing to do
        return NS_OK;
    }

    // Parse the spec if we don't have a cached result
    if (mSpec.IsEmpty()) {
        NS_WARNING("url not initialized");
        return NS_ERROR_NOT_INITIALIZED;
    }

    if (!SegmentIs(mScheme, "file")) {
        NS_WARNING("not a file URL");
        return NS_ERROR_FAILURE;
    }

    return net_GetFileFromURLSpec(mSpec, getter_AddRefs(mFile));
}

NS_IMETHODIMP
nsStandardURL::GetFile(nsIFile **result)
{
    NS_PRECONDITION(mSupportsFileURL,
                    "GetFile() called on a URL that doesn't support files!");
    nsresult rv = EnsureFile();
    if (NS_FAILED(rv))
        return rv;

    if (LOG_ENABLED()) {
        nsAutoCString path;
        mFile->GetNativePath(path);
        LOG(("nsStandardURL::GetFile [this=%p spec=%s resulting_path=%s]\n",
            this, mSpec.get(), path.get()));
    }

    // clone the file, so the caller can modify it.
    // XXX nsIFileURL.idl specifies that the consumer must _not_ modify the
    // nsIFile returned from this method; but it seems that some folks do
    // (see bug 161921). until we can be sure that all the consumers are
    // behaving themselves, we'll stay on the safe side and clone the file.
    // see bug 212724 about fixing the consumers.
    return mFile->Clone(result);
}

NS_IMETHODIMP
nsStandardURL::SetFile(nsIFile *file)
{
    ENSURE_MUTABLE();

    NS_ENSURE_ARG_POINTER(file);

    nsresult rv;
    nsAutoCString url;

    rv = net_GetURLSpecFromFile(file, url);
    if (NS_FAILED(rv)) return rv;

    SetSpec(url);

    rv = Init(mURLType, mDefaultPort, url, nullptr, nullptr);

    // must clone |file| since its value is not guaranteed to remain constant
    if (NS_SUCCEEDED(rv)) {
        InvalidateCache();
        if (NS_FAILED(file->Clone(getter_AddRefs(mFile)))) {
            NS_WARNING("nsIFile::Clone failed");
            // failure to clone is not fatal (GetFile will generate mFile)
            mFile = 0;
        }
    }
    return rv;
}

//----------------------------------------------------------------------------
// nsStandardURL::nsIStandardURL
//----------------------------------------------------------------------------

inline bool
IsUTFCharset(const char *aCharset)
{
    return ((aCharset[0] == 'U' || aCharset[0] == 'u') &&
            (aCharset[1] == 'T' || aCharset[1] == 't') &&
            (aCharset[2] == 'F' || aCharset[2] == 'f'));
}

NS_IMETHODIMP
nsStandardURL::Init(uint32_t urlType,
                    int32_t defaultPort,
                    const nsACString &spec,
                    const char *charset,
                    nsIURI *baseURI)
{
    ENSURE_MUTABLE();

    if (spec.Length() > (uint32_t) net_GetURLMaxLength()) {
        return NS_ERROR_MALFORMED_URI;
    }

    InvalidateCache();

    switch (urlType) {
    case URLTYPE_STANDARD:
        mParser = net_GetStdURLParser();
        break;
    case URLTYPE_AUTHORITY:
        mParser = net_GetAuthURLParser();
        break;
    case URLTYPE_NO_AUTHORITY:
        mParser = net_GetNoAuthURLParser();
        break;
    default:
        NS_NOTREACHED("bad urlType");
        return NS_ERROR_INVALID_ARG;
    }
    mDefaultPort = defaultPort;
    mURLType = urlType;

    mOriginCharset.Truncate();

    //if charset override is absent, use UTF8 as url encoding
    if (charset != nullptr && *charset != '\0' && !IsUTFCharset(charset)) {
        mOriginCharset = charset;
    }

    if (baseURI && net_IsAbsoluteURL(spec)) {
        baseURI = nullptr;
    }

    if (!baseURI)
        return SetSpec(spec);

    nsAutoCString buf;
    nsresult rv = baseURI->Resolve(spec, buf);
    if (NS_FAILED(rv)) return rv;

    return SetSpec(buf);
}

NS_IMETHODIMP
nsStandardURL::SetDefaultPort(int32_t aNewDefaultPort)
{
    ENSURE_MUTABLE();

    InvalidateCache();

    // If we're already using the new default-port as a custom port, then clear
    // it off of our mSpec & set mPort to -1, to indicate that we'll be using
    // the default from now on (which happens to match what we already had).
    if (mPort == aNewDefaultPort) {
        ReplacePortInSpec(-1);
        mPort = -1;
    }
    mDefaultPort = aNewDefaultPort;

    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::GetMutable(bool *value)
{
    *value = mMutable;
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetMutable(bool value)
{
    NS_ENSURE_ARG(mMutable || !value);

    mMutable = value;
    return NS_OK;
}

//----------------------------------------------------------------------------
// nsStandardURL::nsISerializable
//----------------------------------------------------------------------------

NS_IMETHODIMP
nsStandardURL::Read(nsIObjectInputStream *stream)
{
    NS_PRECONDITION(!mHostA, "Shouldn't have cached ASCII host");
    NS_PRECONDITION(mSpecEncoding == eEncoding_Unknown,
                    "Shouldn't have spec encoding here");
    
    nsresult rv;
    
    uint32_t urlType;
    rv = stream->Read32(&urlType);
    if (NS_FAILED(rv)) return rv;
    mURLType = urlType;
    switch (mURLType) {
      case URLTYPE_STANDARD:
        mParser = net_GetStdURLParser();
        break;
      case URLTYPE_AUTHORITY:
        mParser = net_GetAuthURLParser();
        break;
      case URLTYPE_NO_AUTHORITY:
        mParser = net_GetNoAuthURLParser();
        break;
      default:
        NS_NOTREACHED("bad urlType");
        return NS_ERROR_FAILURE;
    }

    rv = stream->Read32((uint32_t *) &mPort);
    if (NS_FAILED(rv)) return rv;

    rv = stream->Read32((uint32_t *) &mDefaultPort);
    if (NS_FAILED(rv)) return rv;

    rv = NS_ReadOptionalCString(stream, mSpec);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mScheme);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mAuthority);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mUsername);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mPassword);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mHost);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mPath);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mFilepath);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mDirectory);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mBasename);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mExtension);
    if (NS_FAILED(rv)) return rv;

    // handle forward compatibility from older serializations that included mParam
    URLSegment old_param;
    rv = ReadSegment(stream, old_param);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mQuery);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mRef);
    if (NS_FAILED(rv)) return rv;

    rv = NS_ReadOptionalCString(stream, mOriginCharset);
    if (NS_FAILED(rv)) return rv;

    bool isMutable;
    rv = stream->ReadBoolean(&isMutable);
    if (NS_FAILED(rv)) return rv;
    mMutable = isMutable;

    bool supportsFileURL;
    rv = stream->ReadBoolean(&supportsFileURL);
    if (NS_FAILED(rv)) return rv;
    mSupportsFileURL = supportsFileURL;

    uint32_t hostEncoding;
    rv = stream->Read32(&hostEncoding);
    if (NS_FAILED(rv)) return rv;
    if (hostEncoding != eEncoding_ASCII && hostEncoding != eEncoding_UTF8) {
        NS_WARNING("Unexpected host encoding");
        return NS_ERROR_UNEXPECTED;
    }
    mHostEncoding = hostEncoding;

    // wait until object is set up, then modify path to include the param
    if (old_param.mLen >= 0) {  // note that mLen=0 is ";"
        // If this wasn't empty, it marks characters between the end of the 
        // file and start of the query - mPath should include the param,
        // query and ref already.  Bump the mFilePath and 
        // directory/basename/extension components to include this.
        mFilepath.Merge(mSpec,  ';', old_param);
        mDirectory.Merge(mSpec, ';', old_param);
        mBasename.Merge(mSpec,  ';', old_param);
        mExtension.Merge(mSpec, ';', old_param);
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::Write(nsIObjectOutputStream *stream)
{
    MOZ_ASSERT(mSpec.Length() <= (uint32_t) net_GetURLMaxLength(),
               "The spec should never be this long, we missed a check.");
    nsresult rv;

    rv = stream->Write32(mURLType);
    if (NS_FAILED(rv)) return rv;

    rv = stream->Write32(uint32_t(mPort));
    if (NS_FAILED(rv)) return rv;

    rv = stream->Write32(uint32_t(mDefaultPort));
    if (NS_FAILED(rv)) return rv;

    rv = NS_WriteOptionalStringZ(stream, mSpec.get());
    if (NS_FAILED(rv)) return rv;

    rv = WriteSegment(stream, mScheme);
    if (NS_FAILED(rv)) return rv;

    rv = WriteSegment(stream, mAuthority);
    if (NS_FAILED(rv)) return rv;

    rv = WriteSegment(stream, mUsername);
    if (NS_FAILED(rv)) return rv;

    rv = WriteSegment(stream, mPassword);
    if (NS_FAILED(rv)) return rv;

    rv = WriteSegment(stream, mHost);
    if (NS_FAILED(rv)) return rv;

    rv = WriteSegment(stream, mPath);
    if (NS_FAILED(rv)) return rv;

    rv = WriteSegment(stream, mFilepath);
    if (NS_FAILED(rv)) return rv;

    rv = WriteSegment(stream, mDirectory);
    if (NS_FAILED(rv)) return rv;

    rv = WriteSegment(stream, mBasename);
    if (NS_FAILED(rv)) return rv;

    rv = WriteSegment(stream, mExtension);
    if (NS_FAILED(rv)) return rv;

    // for backwards compatibility since we removed mParam.  Note that this will mean that
    // an older browser will read "" for mParam, and the param(s) will be part of mPath (as they
    // after the removal of special handling).  It only matters if you downgrade a browser to before
    // the patch.
    URLSegment empty;
    rv = WriteSegment(stream, empty);
    if (NS_FAILED(rv)) return rv;

    rv = WriteSegment(stream, mQuery);
    if (NS_FAILED(rv)) return rv;

    rv = WriteSegment(stream, mRef);
    if (NS_FAILED(rv)) return rv;

    rv = NS_WriteOptionalStringZ(stream, mOriginCharset.get());
    if (NS_FAILED(rv)) return rv;

    rv = stream->WriteBoolean(mMutable);
    if (NS_FAILED(rv)) return rv;

    rv = stream->WriteBoolean(mSupportsFileURL);
    if (NS_FAILED(rv)) return rv;

    rv = stream->Write32(mHostEncoding);
    if (NS_FAILED(rv)) return rv;

    // mSpecEncoding and mHostA are just caches that can be recovered as needed.

    return NS_OK;
}

//---------------------------------------------------------------------------
// nsStandardURL::nsIIPCSerializableURI
//---------------------------------------------------------------------------

inline
mozilla::ipc::StandardURLSegment
ToIPCSegment(const nsStandardURL::URLSegment& aSegment)
{
    return mozilla::ipc::StandardURLSegment(aSegment.mPos, aSegment.mLen);
}

inline
nsStandardURL::URLSegment
FromIPCSegment(const mozilla::ipc::StandardURLSegment& aSegment)
{
    return nsStandardURL::URLSegment(aSegment.position(), aSegment.length());
}

void
nsStandardURL::Serialize(URIParams& aParams)
{
    MOZ_ASSERT(mSpec.Length() <= (uint32_t) net_GetURLMaxLength(),
               "The spec should never be this long, we missed a check.");
    StandardURLParams params;

    params.urlType() = mURLType;
    params.port() = mPort;
    params.defaultPort() = mDefaultPort;
    params.spec() = mSpec;
    params.scheme() = ToIPCSegment(mScheme);
    params.authority() = ToIPCSegment(mAuthority);
    params.username() = ToIPCSegment(mUsername);
    params.password() = ToIPCSegment(mPassword);
    params.host() = ToIPCSegment(mHost);
    params.path() = ToIPCSegment(mPath);
    params.filePath() = ToIPCSegment(mFilepath);
    params.directory() = ToIPCSegment(mDirectory);
    params.baseName() = ToIPCSegment(mBasename);
    params.extension() = ToIPCSegment(mExtension);
    params.query() = ToIPCSegment(mQuery);
    params.ref() = ToIPCSegment(mRef);
    params.originCharset() = mOriginCharset;
    params.isMutable() = !!mMutable;
    params.supportsFileURL() = !!mSupportsFileURL;
    params.hostEncoding() = mHostEncoding;
    // mSpecEncoding and mHostA are just caches that can be recovered as needed.

    aParams = params;
}

bool
nsStandardURL::Deserialize(const URIParams& aParams)
{
    NS_PRECONDITION(!mHostA, "Shouldn't have cached ASCII host");
    NS_PRECONDITION(mSpecEncoding == eEncoding_Unknown,
                    "Shouldn't have spec encoding here");
    NS_PRECONDITION(!mFile, "Shouldn't have cached file");

    if (aParams.type() != URIParams::TStandardURLParams) {
        NS_ERROR("Received unknown parameters from the other process!");
        return false;
    }

    const StandardURLParams& params = aParams.get_StandardURLParams();

    mURLType = params.urlType();
    switch (mURLType) {
        case URLTYPE_STANDARD:
            mParser = net_GetStdURLParser();
            break;
        case URLTYPE_AUTHORITY:
            mParser = net_GetAuthURLParser();
            break;
        case URLTYPE_NO_AUTHORITY:
            mParser = net_GetNoAuthURLParser();
            break;
        default:
            NS_NOTREACHED("bad urlType");
            return false;
    }

    if (params.hostEncoding() != eEncoding_ASCII &&
        params.hostEncoding() != eEncoding_UTF8) {
        NS_WARNING("Unexpected host encoding");
        return false;
    }

    mPort = params.port();
    mDefaultPort = params.defaultPort();
    mSpec = params.spec();
    mScheme = FromIPCSegment(params.scheme());
    mAuthority = FromIPCSegment(params.authority());
    mUsername = FromIPCSegment(params.username());
    mPassword = FromIPCSegment(params.password());
    mHost = FromIPCSegment(params.host());
    mPath = FromIPCSegment(params.path());
    mFilepath = FromIPCSegment(params.filePath());
    mDirectory = FromIPCSegment(params.directory());
    mBasename = FromIPCSegment(params.baseName());
    mExtension = FromIPCSegment(params.extension());
    mQuery = FromIPCSegment(params.query());
    mRef = FromIPCSegment(params.ref());
    mOriginCharset = params.originCharset();
    mMutable = params.isMutable();
    mSupportsFileURL = params.supportsFileURL();
    mHostEncoding = params.hostEncoding();

    // mSpecEncoding and mHostA are just caches that can be recovered as needed.
    return true;
}

//----------------------------------------------------------------------------
// nsStandardURL::nsIClassInfo
//----------------------------------------------------------------------------

NS_IMETHODIMP 
nsStandardURL::GetInterfaces(uint32_t *count, nsIID * **array)
{
    *count = 0;
    *array = nullptr;
    return NS_OK;
}

NS_IMETHODIMP 
nsStandardURL::GetScriptableHelper(nsIXPCScriptable **_retval)
{
    *_retval = nullptr;
    return NS_OK;
}

NS_IMETHODIMP 
nsStandardURL::GetContractID(char * *aContractID)
{
    *aContractID = nullptr;
    return NS_OK;
}

NS_IMETHODIMP 
nsStandardURL::GetClassDescription(char * *aClassDescription)
{
    *aClassDescription = nullptr;
    return NS_OK;
}

NS_IMETHODIMP 
nsStandardURL::GetClassID(nsCID * *aClassID)
{
    *aClassID = (nsCID*) moz_xmalloc(sizeof(nsCID));
    if (!*aClassID)
        return NS_ERROR_OUT_OF_MEMORY;
    return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP 
nsStandardURL::GetFlags(uint32_t *aFlags)
{
    *aFlags = nsIClassInfo::MAIN_THREAD_ONLY;
    return NS_OK;
}

NS_IMETHODIMP 
nsStandardURL::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    *aClassIDNoAlloc = kStandardURLCID;
    return NS_OK;
}

//----------------------------------------------------------------------------
// nsStandardURL::nsISizeOf
//----------------------------------------------------------------------------

size_t
nsStandardURL::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return mSpec.SizeOfExcludingThisIfUnshared(aMallocSizeOf) +
         mOriginCharset.SizeOfExcludingThisIfUnshared(aMallocSizeOf) +
         aMallocSizeOf(mHostA);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mParser
  // - mFile
}

size_t
nsStandardURL::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}
