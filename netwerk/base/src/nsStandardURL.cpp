/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 *   Andreas Otte <andreas.otte@debitel.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef MOZ_IPC
#include "IPCMessageUtils.h"
#endif

#include "nsStandardURL.h"
#include "nsDependentSubstring.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsEscape.h"
#include "nsILocalFile.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsICharsetConverterManager.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranch2.h"
#include "nsIIDNService.h"
#include "nsNetUtil.h"
#include "prlog.h"
#include "nsAutoPtr.h"
#include "nsIProgrammingLanguage.h"
#include "nsVoidArray.h"

static NS_DEFINE_CID(kThisImplCID, NS_THIS_STANDARDURL_IMPL_CID);
static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);

nsIIDNService *nsStandardURL::gIDN = nsnull;
nsICharsetConverterManager *nsStandardURL::gCharsetMgr = nsnull;
PRBool nsStandardURL::gInitialized = PR_FALSE;
PRBool nsStandardURL::gEscapeUTF8 = PR_TRUE;
PRBool nsStandardURL::gAlwaysEncodeInUTF8 = PR_TRUE;
PRBool nsStandardURL::gEncodeQueryInUTF8 = PR_TRUE;

#if defined(PR_LOGGING)
//
// setenv NSPR_LOG_MODULES nsStandardURL:5
//
static PRLogModuleInfo *gStandardURLLog;
#endif

// The Chromium code defines its own LOG macro which we don't want
#undef LOG
#define LOG(args)     PR_LOG(gStandardURLLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gStandardURLLog, PR_LOG_DEBUG)

//----------------------------------------------------------------------------

#define ENSURE_MUTABLE() \
  PR_BEGIN_MACRO \
    if (!mMutable) { \
        NS_ERROR("attempt to modify an immutable nsStandardURL"); \
        return NS_ERROR_ABORT; \
    } \
  PR_END_MACRO

//----------------------------------------------------------------------------

static nsresult
EncodeString(nsIUnicodeEncoder *encoder, const nsAFlatString &str, nsACString &result)
{
    nsresult rv;
    PRInt32 len = str.Length();
    PRInt32 maxlen;

    rv = encoder->GetMaxLength(str.get(), len, &maxlen);
    if (NS_FAILED(rv))
        return rv;

    char buf[256], *p = buf;
    if (PRUint32(maxlen) > sizeof(buf) - 1) {
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
#define NS_NET_PREF_ENABLEIDN          "network.enableIDN"
#define NS_NET_PREF_ALWAYSENCODEINUTF8 "network.standard-url.encode-utf8"
#define NS_NET_PREF_ENCODEQUERYINUTF8  "network.standard-url.encode-query-utf8"

NS_IMPL_ISUPPORTS1(nsStandardURL::nsPrefObserver, nsIObserver)

NS_IMETHODIMP nsStandardURL::
nsPrefObserver::Observe(nsISupports *subject,
                        const char *topic,
                        const PRUnichar *data)
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

PRInt32 nsStandardURL::
nsSegmentEncoder::EncodeSegmentCount(const char *str,
                                     const URLSegment &seg,
                                     PRInt16 mask,
                                     nsAFlatCString &result,
                                     PRBool &appended)
{
    appended = PR_FALSE;
    if (!str)
        return 0;
    PRInt32 len = 0;
    if (seg.mLen > 0) {
        PRUint32 pos = seg.mPos;
        len = seg.mLen;

        // first honor the origin charset if appropriate. as an optimization,
        // only do this if the segment is non-ASCII.  Further, if mCharset is
        // null or the empty string then the origin charset is UTF-8 and there
        // is nothing to do.
        nsCAutoString encBuf;
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
        PRInt16 escapeFlags = (gEscapeUTF8 || mEncoder) ? 0 : esc_OnlyASCII;

        PRUint32 initLen = result.Length();

        // now perform any required escaping
        if (NS_EscapeURL(str + pos, len, mask | escapeFlags, result)) {
            len = result.Length() - initLen;
            appended = PR_TRUE;
        }
        else if (str == encBuf.get()) {
            result += encBuf; // append only!!
            len = encBuf.Length();
            appended = PR_TRUE;
        }
    }
    return len;
}

const nsACString &nsStandardURL::
nsSegmentEncoder::EncodeSegment(const nsASingleFragmentCString &str,
                                PRInt16 mask,
                                nsAFlatCString &result)
{
    const char *text;
    PRBool encoded;
    EncodeSegmentCount(str.BeginReading(text), URLSegment(0, str.Length()), mask, result, encoded);
    if (encoded)
        return result;
    return str;
}

PRBool nsStandardURL::
nsSegmentEncoder::InitUnicodeEncoder()
{
    NS_ASSERTION(!mEncoder, "Don't call this if we have an encoder already!");
    nsresult rv;
    if (!gCharsetMgr) {
        rv = CallGetService("@mozilla.org/charset-converter-manager;1",
                            &gCharsetMgr);
        if (NS_FAILED(rv)) {
            NS_ERROR("failed to get charset-converter-manager");
            return PR_FALSE;
        }
    }

    rv = gCharsetMgr->GetUnicodeEncoder(mCharset, getter_AddRefs(mEncoder));
    if (NS_FAILED(rv)) {
        NS_ERROR("failed to get unicode encoder");
        mEncoder = 0; // just in case
        return PR_FALSE;
    }

    return PR_TRUE;
}

#define GET_SEGMENT_ENCODER_INTERNAL(name, useUTF8) \
    nsSegmentEncoder name(useUTF8 ? nsnull : mOriginCharset.get())

#define GET_SEGMENT_ENCODER(name) \
    GET_SEGMENT_ENCODER_INTERNAL(name, gAlwaysEncodeInUTF8)

#define GET_QUERY_ENCODER(name) \
    GET_SEGMENT_ENCODER_INTERNAL(name, gAlwaysEncodeInUTF8 && \
                                 gEncodeQueryInUTF8)

//----------------------------------------------------------------------------
// nsStandardURL <public>
//----------------------------------------------------------------------------

#ifdef DEBUG_DUMP_URLS_AT_SHUTDOWN
static PRCList gAllURLs;
#endif

nsStandardURL::nsStandardURL(PRBool aSupportsFileURL)
    : mDefaultPort(-1)
    , mPort(-1)
    , mHostA(nsnull)
    , mHostEncoding(eEncoding_ASCII)
    , mSpecEncoding(eEncoding_Unknown)
    , mURLType(URLTYPE_STANDARD)
    , mMutable(PR_TRUE)
    , mSupportsFileURL(aSupportsFileURL)
{
#if defined(PR_LOGGING)
    if (!gStandardURLLog)
        gStandardURLLog = PR_NewLogModule("nsStandardURL");
#endif

    LOG(("Creating nsStandardURL @%p\n", this));

    if (!gInitialized) {
        gInitialized = PR_TRUE;
        InitGlobalObjects();
    }

    // default parser in case nsIStandardURL::Init is never called
    mParser = net_GetStdURLParser();

#ifdef DEBUG_DUMP_URLS_AT_SHUTDOWN
    PR_APPEND_LINK(&mDebugCList, &gAllURLs);
#endif
}

nsStandardURL::~nsStandardURL()
{
    LOG(("Destroying nsStandardURL @%p\n", this));

    CRTFREEIF(mHostA);
#ifdef DEBUG_DUMP_URLS_AT_SHUTDOWN
    PR_REMOVE_LINK(&mDebugCList);
#endif
}

#ifdef DEBUG_DUMP_URLS_AT_SHUTDOWN
static void DumpLeakedURLs()
{
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
    nsCOMPtr<nsIPrefBranch2> prefBranch( do_GetService(NS_PREFSERVICE_CONTRACTID) );
    if (prefBranch) {
        nsCOMPtr<nsIObserver> obs( new nsPrefObserver() );
        prefBranch->AddObserver(NS_NET_PREF_ESCAPEUTF8, obs.get(), PR_FALSE);
        prefBranch->AddObserver(NS_NET_PREF_ALWAYSENCODEINUTF8, obs.get(), PR_FALSE);
        prefBranch->AddObserver(NS_NET_PREF_ENCODEQUERYINUTF8, obs.get(), PR_FALSE);
        prefBranch->AddObserver(NS_NET_PREF_ENABLEIDN, obs.get(), PR_FALSE);

        PrefsChanged(prefBranch, nsnull);
    }

#ifdef DEBUG_DUMP_URLS_AT_SHUTDOWN
    PR_INIT_CLIST(&gAllURLs);
#endif
}

void
nsStandardURL::ShutdownGlobalObjects()
{
    NS_IF_RELEASE(gIDN);
    NS_IF_RELEASE(gCharsetMgr);

#ifdef DEBUG_DUMP_URLS_AT_SHUTDOWN
    if (gInitialized)
        atexit(DumpLeakedURLs);
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
    mParam.Reset();
    mQuery.Reset();
    mRef.Reset();

    InvalidateCache();
}

void
nsStandardURL::InvalidateCache(PRBool invalidateCachedFile)
{
    if (invalidateCachedFile)
        mFile = 0;
    CRTFREEIF(mHostA);
    mSpecEncoding = eEncoding_Unknown;
}

PRBool
nsStandardURL::EscapeIPv6(const char *host, nsCString &result)
{
    // Escape IPv6 address literal by surrounding it with []'s
    if (host && (host[0] != '[') && PL_strchr(host, ':')) {
        result.Assign('[');
        result.Append(host);
        result.Append(']');
        return PR_TRUE;
    }
    return PR_FALSE;
}

PRBool
nsStandardURL::NormalizeIDN(const nsCSubstring &host, nsCString &result)
{
    // If host is ACE, then convert to UTF-8.  Else, if host is already UTF-8,
    // then make sure it is normalized per IDN.

    // this function returns PR_TRUE if normalization succeeds.

    // NOTE: As a side-effect this function sets mHostEncoding.  While it would
    // be nice to avoid side-effects in this function, the implementation of
    // this function is already somewhat bound to the behavior of the
    // callsites.  Anyways, this function exists to avoid code duplication, so
    // side-effects abound :-/

    NS_ASSERTION(mHostEncoding == eEncoding_ASCII, "unexpected default encoding");

    PRBool isASCII;
    if (gIDN &&
        NS_SUCCEEDED(gIDN->ConvertToDisplayIDN(host, &isASCII, result))) {
        if (!isASCII)
          mHostEncoding = eEncoding_UTF8;

        return PR_TRUE;
    }

    result.Truncate();
    return PR_FALSE;
}

void
nsStandardURL::CoalescePath(netCoalesceFlags coalesceFlag, char *path)
{
    net_CoalesceDirs(coalesceFlag, path);
    PRInt32 newLen = strlen(path);
    if (newLen < mPath.mLen) {
        PRInt32 diff = newLen - mPath.mLen;
        mPath.mLen = newLen;
        mDirectory.mLen += diff;
        mFilepath.mLen += diff;
        ShiftFromBasename(diff);
    }
}

PRUint32
nsStandardURL::AppendSegmentToBuf(char *buf, PRUint32 i, const char *str, URLSegment &seg, const nsCString *escapedStr, PRBool useEscaped)
{
    if (seg.mLen > 0) {
        if (useEscaped) {
            seg.mLen = escapedStr->Length();
            memcpy(buf + i, escapedStr->get(), seg.mLen);
        }
        else
            memcpy(buf + i, str + seg.mPos, seg.mLen);
        seg.mPos = i;
        i += seg.mLen;
    }
    return i;
}

PRUint32
nsStandardURL::AppendToBuf(char *buf, PRUint32 i, const char *str, PRUint32 len)
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
    nsCAutoString encUsername, encPassword, encHost, encDirectory,
      encBasename, encExtension, encParam, encQuery, encRef;
    PRBool useEncUsername, useEncPassword, useEncHost, useEncDirectory,
      useEncBasename, useEncExtension, useEncParam, useEncQuery, useEncRef;

    //
    // escape each URL segment, if necessary, and calculate approximate normalized
    // spec length.
    //
    PRUint32 approxLen = 3; // includes room for "://"

    // the scheme is already ASCII
    if (mScheme.mLen > 0)
        approxLen += mScheme.mLen;

    // encode URL segments; convert UTF-8 to origin charset and possibly escape.
    // results written to encXXX variables only if |spec| is not already in the
    // appropriate encoding.
    {
        GET_SEGMENT_ENCODER(encoder);
        GET_QUERY_ENCODER(queryEncoder);
        approxLen += encoder.EncodeSegmentCount(spec, mUsername,  esc_Username,      encUsername,  useEncUsername);
        approxLen += encoder.EncodeSegmentCount(spec, mPassword,  esc_Password,      encPassword,  useEncPassword);
        approxLen += encoder.EncodeSegmentCount(spec, mDirectory, esc_Directory,     encDirectory, useEncDirectory);
        approxLen += encoder.EncodeSegmentCount(spec, mBasename,  esc_FileBaseName,  encBasename,  useEncBasename);
        approxLen += encoder.EncodeSegmentCount(spec, mExtension, esc_FileExtension, encExtension, useEncExtension);
        approxLen += encoder.EncodeSegmentCount(spec, mParam,     esc_Param,         encParam,     useEncParam);
        approxLen += queryEncoder.EncodeSegmentCount(spec, mQuery, esc_Query,        encQuery,     useEncQuery);
        approxLen += encoder.EncodeSegmentCount(spec, mRef,       esc_Ref,           encRef,       useEncRef);
    }

    // do not escape the hostname, if IPv6 address literal, mHost will
    // already point to a [ ] delimited IPv6 address literal.
    // However, perform Unicode normalization on it, as IDN does.
    mHostEncoding = eEncoding_ASCII;
    if (mHost.mLen > 0) {
        const nsCSubstring& tempHost =
            Substring(spec + mHost.mPos, spec + mHost.mPos + mHost.mLen);
        if (tempHost.FindChar('\0') != kNotFound)
            return NS_ERROR_MALFORMED_URI;  // null embedded in hostname
        if (tempHost.FindChar(' ') != kNotFound)
            return NS_ERROR_MALFORMED_URI;  // don't allow spaces in the hostname
        if ((useEncHost = NormalizeIDN(tempHost, encHost)))
            approxLen += encHost.Length();
        else
            approxLen += mHost.mLen;
    }

    //
    // generate the normalized URL string
    //
    if (!EnsureStringLength(mSpec, approxLen + 32))
        return NS_ERROR_OUT_OF_MEMORY;
    char *buf;
    mSpec.BeginWriting(buf);
    PRUint32 i = 0;

    if (mScheme.mLen > 0) {
        i = AppendSegmentToBuf(buf, i, spec, mScheme);
        net_ToLowerCase(buf + mScheme.mPos, mScheme.mLen);
        i = AppendToBuf(buf, i, "://", 3);
    }

    // record authority starting position
    mAuthority.mPos = i;

    // append authority
    if (mUsername.mLen > 0) {
        i = AppendSegmentToBuf(buf, i, spec, mUsername, &encUsername, useEncUsername);
        if (mPassword.mLen >= 0) {
            buf[i++] = ':';
            i = AppendSegmentToBuf(buf, i, spec, mPassword, &encPassword, useEncPassword);
        }
        buf[i++] = '@';
    }
    if (mHost.mLen > 0) {
        i = AppendSegmentToBuf(buf, i, spec, mHost, &encHost, useEncHost);
        net_ToLowerCase(buf + mHost.mPos, mHost.mLen);
        if (mPort != -1 && mPort != mDefaultPort) {
            nsCAutoString portbuf;
            portbuf.AppendInt(mPort);
            buf[i++] = ':';
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
        PRUint32 leadingSlash = 0;
        if (spec[mPath.mPos] != '/') {
            LOG(("adding leading slash to path\n"));
            leadingSlash = 1;
            buf[i++] = '/';
            // basename must exist, even if empty (bugs 113508, 429347)
            if (mBasename.mLen == -1) {
                mBasename.mPos = i;
                mBasename.mLen = 0;
            }
        }

        // record corrected (file)path starting position
        mPath.mPos = mFilepath.mPos = i - leadingSlash;

        i = AppendSegmentToBuf(buf, i, spec, mDirectory, &encDirectory, useEncDirectory);

        // the directory must end with a '/'
        if (buf[i-1] != '/') {
            buf[i++] = '/';
            mDirectory.mLen++;
        }

        i = AppendSegmentToBuf(buf, i, spec, mBasename, &encBasename, useEncBasename);

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
            i = AppendSegmentToBuf(buf, i, spec, mExtension, &encExtension, useEncExtension);
        }
        // calculate corrected filepath length
        mFilepath.mLen = i - mFilepath.mPos;

        if (mParam.mLen >= 0) {
            buf[i++] = ';';
            i = AppendSegmentToBuf(buf, i, spec, mParam, &encParam, useEncParam);
        }
        if (mQuery.mLen >= 0) {
            buf[i++] = '?';
            i = AppendSegmentToBuf(buf, i, spec, mQuery, &encQuery, useEncQuery);
        }
        if (mRef.mLen >= 0) {
            buf[i++] = '#';
            i = AppendSegmentToBuf(buf, i, spec, mRef, &encRef, useEncRef);
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
    NS_ASSERTION(mSpec.Length() <= approxLen+32, "We've overflowed the mSpec buffer!");
    return NS_OK;
}

PRBool
nsStandardURL::SegmentIs(const URLSegment &seg, const char *val, PRBool ignoreCase)
{
    // one or both may be null
    if (!val || mSpec.IsEmpty())
        return (!val && (mSpec.IsEmpty() || seg.mLen < 0));
    if (seg.mLen < 0)
        return PR_FALSE;
    // if the first |seg.mLen| chars of |val| match, then |val| must
    // also be null terminated at |seg.mLen|.
    if (ignoreCase)
        return !PL_strncasecmp(mSpec.get() + seg.mPos, val, seg.mLen)
            && (val[seg.mLen] == '\0');
    else
        return !strncmp(mSpec.get() + seg.mPos, val, seg.mLen)
            && (val[seg.mLen] == '\0');
}

PRBool
nsStandardURL::SegmentIs(const char* spec, const URLSegment &seg, const char *val, PRBool ignoreCase)
{
    // one or both may be null
    if (!val || !spec)
        return (!val && (!spec || seg.mLen < 0));
    if (seg.mLen < 0)
        return PR_FALSE;
    // if the first |seg.mLen| chars of |val| match, then |val| must
    // also be null terminated at |seg.mLen|.
    if (ignoreCase)
        return !PL_strncasecmp(spec + seg.mPos, val, seg.mLen)
            && (val[seg.mLen] == '\0');
    else
        return !strncmp(spec + seg.mPos, val, seg.mLen)
            && (val[seg.mLen] == '\0');
}

PRBool
nsStandardURL::SegmentIs(const URLSegment &seg1, const char *val, const URLSegment &seg2, PRBool ignoreCase)
{
    if (seg1.mLen != seg2.mLen)
        return PR_FALSE;
    if (seg1.mLen == -1 || (!val && mSpec.IsEmpty()))
        return PR_TRUE; // both are empty
    if (ignoreCase)
        return !PL_strncasecmp(mSpec.get() + seg1.mPos, val + seg2.mPos, seg1.mLen); 
    else
        return !strncmp(mSpec.get() + seg1.mPos, val + seg2.mPos, seg1.mLen); 
}

PRInt32
nsStandardURL::ReplaceSegment(PRUint32 pos, PRUint32 len, const char *val, PRUint32 valLen)
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
    return -PRInt32(len);
}

PRInt32
nsStandardURL::ReplaceSegment(PRUint32 pos, PRUint32 len, const nsACString &val)
{
    if (len == 0)
        mSpec.Insert(val, pos);
    else
        mSpec.Replace(pos, len, val);
    return val.Length() - len;
}

nsresult
nsStandardURL::ParseURL(const char *spec, PRInt32 specLen)
{
    nsresult rv;

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
nsStandardURL::ParsePath(const char *spec, PRUint32 pathPos, PRInt32 pathLen)
{
    nsresult rv = mParser->ParsePath(spec + pathPos, pathLen,
                                     &mFilepath.mPos, &mFilepath.mLen,
                                     &mParam.mPos, &mParam.mLen,
                                     &mQuery.mPos, &mQuery.mLen,
                                     &mRef.mPos, &mRef.mLen);
    if (NS_FAILED(rv)) return rv;

    mFilepath.mPos += pathPos;
    mParam.mPos += pathPos;
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
nsStandardURL::AppendToSubstring(PRUint32 pos,
                                 PRInt32 len,
                                 const char *tail,
                                 PRInt32 tailLen)
{
    if (tailLen < 0)
        tailLen = strlen(tail);

    char *result = (char *) NS_Alloc(len + tailLen + 1);
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

    rv = stream->Read32((PRUint32 *) &seg.mLen);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

nsresult
nsStandardURL::WriteSegment(nsIBinaryOutputStream *stream, const URLSegment &seg)
{
    nsresult rv;

    rv = stream->Write32(seg.mPos);
    if (NS_FAILED(rv)) return rv;

    rv = stream->Write32(PRUint32(seg.mLen));
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

#ifdef MOZ_IPC
bool
nsStandardURL::ReadSegment(const IPC::Message *aMsg, void **aIter, URLSegment &seg)
{
    return (IPC::ReadParam(aMsg, aIter, &seg.mPos) &&
            IPC::ReadParam(aMsg, aIter, &seg.mLen));
}

void
nsStandardURL::WriteSegment(IPC::Message *aMsg, const URLSegment &seg)
{
    IPC::WriteParam(aMsg, seg.mPos);
    IPC::WriteParam(aMsg, seg.mLen);
}
#endif

/* static */ void
nsStandardURL::PrefsChanged(nsIPrefBranch *prefs, const char *pref)
{
    PRBool val;

    LOG(("nsStandardURL::PrefsChanged [pref=%s]\n", pref));

#define PREF_CHANGED(p) ((pref == nsnull) || !strcmp(pref, p))
#define GOT_PREF(p, b) (NS_SUCCEEDED(prefs->GetBoolPref(p, &b)))

    if (PREF_CHANGED(NS_NET_PREF_ENABLEIDN)) {
        NS_IF_RELEASE(gIDN);
        if (GOT_PREF(NS_NET_PREF_ENABLEIDN, val) && val) {
            // initialize IDN
            nsCOMPtr<nsIIDNService> serv(do_GetService(NS_IDNSERVICE_CONTRACTID));
            if (serv)
                NS_ADDREF(gIDN = serv.get());
        }
        LOG(("IDN support %s\n", gIDN ? "enabled" : "disabled"));
    }
    
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

    if (PREF_CHANGED(NS_NET_PREF_ENCODEQUERYINUTF8)) {
        if (GOT_PREF(NS_NET_PREF_ENCODEQUERYINUTF8, val))
            gEncodeQueryInUTF8 = val;
        LOG(("encode query in UTF-8 %s\n", gEncodeQueryInUTF8 ? "enabled" : "disabled"));
    }
#undef PREF_CHANGED
#undef GOT_PREF
}

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
    NS_INTERFACE_MAP_ENTRY(nsIIPCSerializable)
    NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
    NS_INTERFACE_MAP_ENTRY(nsIMutable)
    // see nsStandardURL::Equals
    if (aIID.Equals(kThisImplCID))
        foundInterface = static_cast<nsIURI *>(this);
    else
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------------
// nsStandardURL::nsIURI
//----------------------------------------------------------------------------

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsStandardURL::GetSpec(nsACString &result)
{
    result = mSpec;
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
nsStandardURL::GetPort(PRInt32 *result)
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
    result.SetCapacity(mSpec.Length() + PR_MIN(32, mSpec.Length()/10));

    result = Substring(mSpec, 0, mScheme.mLen + 3);

    NS_EscapeURL(Userpass(PR_TRUE), esc_OnlyNonASCII | esc_AlwaysCopy, result);

    // get escaped host
    nsCAutoString escHostport;
    if (mHost.mLen > 0) {
        // this doesn't fail
        (void) GetAsciiHost(escHostport);

        // escHostport = "hostA" + ":port"
        PRUint32 pos = mHost.mPos + mHost.mLen;
        if (pos < mPath.mPos)
            escHostport += Substring(mSpec, pos, mPath.mPos - pos);
    }
    result += escHostport;

    NS_EscapeURL(Path(), esc_OnlyNonASCII | esc_AlwaysCopy, result);
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

NS_IMETHODIMP
nsStandardURL::SetSpec(const nsACString &input)
{
    ENSURE_MUTABLE();

    const nsPromiseFlatCString &flat = PromiseFlatCString(input);
    const char *spec = flat.get();
    PRInt32 specLength = flat.Length();

    LOG(("nsStandardURL::SetSpec [spec=%s]\n", spec));

    Clear();

    if (!spec || !*spec)
        return NS_OK;

    // filter out unexpected chars "\r\n\t" if necessary
    nsCAutoString buf1;
    if (net_FilterURIString(spec, buf1)) {
        spec = buf1.get();
        specLength = buf1.Length();
    }

    // parse the given URL...
    nsresult rv = ParseURL(spec, specLength);
    if (NS_FAILED(rv)) return rv;

    // finally, use the URLSegment member variables to build a normalized
    // copy of |spec|
    rv = BuildNormalizedSpec(spec);

#if defined(PR_LOGGING)
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
        LOG((" param     = (%u,%d)\n", mParam.mPos,     mParam.mLen));
        LOG((" query     = (%u,%d)\n", mQuery.mPos,     mQuery.mLen));
        LOG((" ref       = (%u,%d)\n", mRef.mPos,       mRef.mLen));
    }
#endif
    return rv;
}

NS_IMETHODIMP
nsStandardURL::SetScheme(const nsACString &input)
{
    ENSURE_MUTABLE();

    const nsPromiseFlatCString &scheme = PromiseFlatCString(input);

    LOG(("nsStandardURL::SetScheme [scheme=%s]\n", scheme.get()));

    if (scheme.IsEmpty()) {
        NS_ERROR("cannot remove the scheme from an url");
        return NS_ERROR_UNEXPECTED;
    }
    if (mScheme.mLen < 0) {
        NS_ERROR("uninitialized");
        return NS_ERROR_NOT_INITIALIZED;
    }

    if (!net_IsValidScheme(scheme)) {
        NS_ERROR("the given url scheme contains invalid characters");
        return NS_ERROR_UNEXPECTED;
    }

    InvalidateCache();

    PRInt32 shift = ReplaceSegment(mScheme.mPos, mScheme.mLen, scheme);

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
        NS_ERROR("cannot set user:pass on no-auth url");
        return NS_ERROR_UNEXPECTED;
    }
    if (mAuthority.mLen < 0) {
        NS_ERROR("uninitialized");
        return NS_ERROR_NOT_INITIALIZED;
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
    PRUint32 usernamePos, passwordPos;
    PRInt32 usernameLen, passwordLen;

    rv = mParser->ParseUserInfo(userpass.get(), userpass.Length(),
                                &usernamePos, &usernameLen,
                                &passwordPos, &passwordLen);
    if (NS_FAILED(rv)) return rv;

    // build new user:pass in |buf|
    nsCAutoString buf;
    if (usernameLen > 0) {
        GET_SEGMENT_ENCODER(encoder);
        PRBool ignoredOut;
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

    PRUint32 shift = 0;

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
        PRUint32 userpassLen = mUsername.mLen;
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
        NS_ERROR("cannot set username on no-auth url");
        return NS_ERROR_UNEXPECTED;
    }

    if (username.IsEmpty())
        return SetUserPass(username);

    InvalidateCache();

    // escape username if necessary
    nsCAutoString buf;
    GET_SEGMENT_ENCODER(encoder);
    const nsACString &escUsername =
        encoder.EncodeSegment(username, esc_Username, buf);

    PRInt32 shift;

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
        NS_ERROR("cannot set password on no-auth url");
        return NS_ERROR_UNEXPECTED;
    }
    if (mUsername.mLen <= 0) {
        NS_ERROR("cannot set password without existing username");
        return NS_ERROR_FAILURE;
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
    nsCAutoString buf;
    GET_SEGMENT_ENCODER(encoder);
    const nsACString &escPassword =
        encoder.EncodeSegment(password, esc_Password, buf);

    PRInt32 shift;

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

NS_IMETHODIMP
nsStandardURL::SetHostPort(const nsACString &value)
{
    ENSURE_MUTABLE();

    // XXX needs implementation!!
    NS_NOTREACHED("not implemented");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStandardURL::SetHost(const nsACString &input)
{
    ENSURE_MUTABLE();

    const nsPromiseFlatCString &flat = PromiseFlatCString(input);
    const char *host = flat.get();

    LOG(("nsStandardURL::SetHost [host=%s]\n", host));

    if (mURLType == URLTYPE_NO_AUTHORITY) {
        if (flat.IsEmpty())
            return NS_OK;
        NS_WARNING("cannot set host on no-auth url");
        return NS_ERROR_UNEXPECTED;
    }

    if (host && strlen(host) < flat.Length())
        return NS_ERROR_MALFORMED_URI; // found embedded null

    // For consistency with SetSpec/nsURLParsers, don't allow spaces
    // in the hostname.
    if (strchr(host, ' '))
        return NS_ERROR_MALFORMED_URI;

    InvalidateCache();
    mHostEncoding = eEncoding_ASCII;

    if (!(host && *host)) {
        // remove existing hostname
        if (mHost.mLen > 0) {
            // remove entire authority
            mSpec.Cut(mAuthority.mPos, mAuthority.mLen);
            ShiftFromPath(-mAuthority.mLen);
            mAuthority.mLen = 0;
            mUsername.mLen = -1;
            mPassword.mLen = -1;
            mHost.mLen = -1;
            mPort = -1;
        }
        return NS_OK;
    }

    // handle IPv6 unescaped address literal
    PRInt32 len;
    nsCAutoString hostBuf;
    if (EscapeIPv6(host, hostBuf)) {
        host = hostBuf.get();
        len = hostBuf.Length();
    }
    else if (NormalizeIDN(flat, hostBuf)) {
        host = hostBuf.get();
        len = hostBuf.Length();
    }
    else
        len = flat.Length();

    if (mHost.mLen < 0) {
        mHost.mPos = mAuthority.mPos;
        mHost.mLen = 0;
    }

    PRInt32 shift = ReplaceSegment(mHost.mPos, mHost.mLen, host, len);

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
nsStandardURL::SetPort(PRInt32 port)
{
    ENSURE_MUTABLE();

    LOG(("nsStandardURL::SetPort [port=%d]\n", port));

    if ((port == mPort) || (mPort == -1 && port == mDefaultPort))
        return NS_OK;

    if (mURLType == URLTYPE_NO_AUTHORITY) {
        NS_WARNING("cannot set port on no-auth url");
        return NS_ERROR_UNEXPECTED;
    }

    InvalidateCache();

    if (mPort == -1) {
        // need to insert the port number in the URL spec
        nsCAutoString buf;
        buf.Assign(':');
        buf.AppendInt(port);
        mSpec.Insert(buf, mHost.mPos + mHost.mLen);
        mAuthority.mLen += buf.Length();
        ShiftFromPath(buf.Length());
    }
    else if (port == -1 || port == mDefaultPort) {
        // Don't allow mPort == mDefaultPort
        port = -1;

        // need to remove the port number from the URL spec
        PRUint32 start = mHost.mPos + mHost.mLen;
        PRUint32 lengthToCut = mPath.mPos - start;
        mSpec.Cut(start, lengthToCut);
        mAuthority.mLen -= lengthToCut;
        ShiftFromPath(-lengthToCut);
    }
    else {
        // need to replace the existing port
        nsCAutoString buf;
        buf.AppendInt(port);
        PRUint32 start = mHost.mPos + mHost.mLen + 1;
        PRUint32 length = mPath.mPos - start;
        mSpec.Replace(start, length, buf);
        if (buf.Length() != length) {
            mAuthority.mLen += buf.Length() - length;
            ShiftFromPath(buf.Length() - length);
        }
    }

    mPort = port;
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetPath(const nsACString &input)
{
    ENSURE_MUTABLE();

    const nsPromiseFlatCString &path = PromiseFlatCString(input);

    LOG(("nsStandardURL::SetPath [path=%s]\n", path.get()));

    InvalidateCache();

    if (!path.IsEmpty()) {
        nsCAutoString spec;

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
        mParam.mLen = -1;
        mQuery.mLen = -1;
        mRef.mLen = -1;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::Equals(nsIURI *unknownOther, PRBool *result)
{
    NS_ENSURE_ARG_POINTER(unknownOther);
    NS_PRECONDITION(result, "null pointer");

    nsRefPtr<nsStandardURL> other;
    nsresult rv = unknownOther->QueryInterface(kThisImplCID,
                                               getter_AddRefs(other));
    if (NS_FAILED(rv)) {
        *result = PR_FALSE;
        return NS_OK;
    }

    // First, check whether one URIs is an nsIFileURL while the other
    // is not.  If that's the case, they're different.
    if (mSupportsFileURL != other->mSupportsFileURL) {
        *result = PR_FALSE;
        return NS_OK;
    }

    // Next check parts of a URI that, if different, automatically make the
    // URIs different
    if (!SegmentIs(mScheme, other->mSpec.get(), other->mScheme) ||
        // Check for host manually, since conversion to file will
        // ignore the host!
        !SegmentIs(mHost, other->mSpec.get(), other->mHost) ||
        !SegmentIs(mQuery, other->mSpec.get(), other->mQuery) ||
        !SegmentIs(mRef, other->mSpec.get(), other->mRef) ||
        !SegmentIs(mUsername, other->mSpec.get(), other->mUsername) ||
        !SegmentIs(mPassword, other->mSpec.get(), other->mPassword) ||
        Port() != other->Port() ||
        !SegmentIs(mParam, other->mSpec.get(), other->mParam)) {
        // No need to compare files or other URI parts -- these are different
        // beasties
        *result = PR_FALSE;
        return NS_OK;
    }
    
    // Then check for exact identity of URIs.  If we have it, they're equal
    if (SegmentIs(mDirectory, other->mSpec.get(), other->mDirectory) &&
        SegmentIs(mBasename, other->mSpec.get(), other->mBasename) &&
        SegmentIs(mExtension, other->mSpec.get(), other->mExtension)) {
        *result = PR_TRUE;
        return NS_OK;
    }

    // At this point, the URIs are not identical, but they only differ in the
    // directory/filename/extension.  If these are file URLs, then get the
    // corresponding file objects and compare those, since two filenames that
    // differ, eg, only in case could still be equal.
    if (mSupportsFileURL) {
        // Assume not equal for failure cases... but failures in GetFile are
        // really failures, more or less, so propagate them to caller.
        *result = PR_FALSE;

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
    *result = PR_FALSE;

    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SchemeIs(const char *scheme, PRBool *result)
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
    nsStandardURL *clone = StartClone();
    if (!clone)
        return NS_ERROR_OUT_OF_MEMORY;

    clone->mSpec = mSpec;
    clone->mDefaultPort = mDefaultPort;
    clone->mPort = mPort;
    clone->mScheme = mScheme;
    clone->mAuthority = mAuthority;
    clone->mUsername = mUsername;
    clone->mPassword = mPassword;
    clone->mHost = mHost;
    clone->mPath = mPath;
    clone->mFilepath = mFilepath;
    clone->mDirectory = mDirectory;
    clone->mBasename = mBasename;
    clone->mExtension = mExtension;
    clone->mParam = mParam;
    clone->mQuery = mQuery;
    clone->mRef = mRef;
    clone->mOriginCharset = mOriginCharset;
    clone->mURLType = mURLType;
    clone->mParser = mParser;
    clone->mFile = mFile;
    clone->mHostA = mHostA ? nsCRT::strdup(mHostA) : nsnull;
    clone->mMutable = PR_TRUE;
    clone->mSupportsFileURL = mSupportsFileURL;
    clone->mHostEncoding = mHostEncoding;
    clone->mSpecEncoding = mSpecEncoding;

    NS_ADDREF(*result = clone);
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::Resolve(const nsACString &in, nsACString &out)
{
    const nsPromiseFlatCString &flat = PromiseFlatCString(in);
    const char *relpath = flat.get();

    // filter out unexpected chars "\r\n\t" if necessary
    nsCAutoString buf;
    PRInt32 relpathLen;
    if (net_FilterURIString(relpath, buf)) {
        relpath = buf.get();
        relpathLen = buf.Length();
    } else
        relpathLen = flat.Length();
    
    char *result = nsnull;

    LOG(("nsStandardURL::Resolve [this=%p spec=%s relpath=%s]\n",
        this, mSpec.get(), relpath));

    NS_ASSERTION(mParser, "no parser: unitialized");

    // NOTE: there is no need for this function to produce normalized
    // output.  normalization will occur when the result is used to 
    // initialize a nsStandardURL object.

    if (mScheme.mLen < 0) {
        NS_ERROR("unable to Resolve URL: this URL not initialized");
        return NS_ERROR_NOT_INITIALIZED;
    }

    nsresult rv;
    URLSegment scheme;
    char *resultPath = nsnull;
    PRBool relative = PR_FALSE;
    PRUint32 offset = 0;
    netCoalesceFlags coalesceFlag = NET_COALESCE_NORMAL;

    // relative urls should never contain a host, so we always want to use
    // the noauth url parser.
    // use it to extract a possible scheme
    rv = mParser->ParseURL(relpath, 
                           relpathLen,
                           &scheme.mPos, &scheme.mLen,
                           nsnull, nsnull,
                           nsnull, nsnull);

    // if the parser fails (for example because there is no valid scheme)
    // reset the scheme and assume a relative url
    if (NS_FAILED(rv)) scheme.Reset(); 

    if (scheme.mLen >= 0) {
        // add some flags to coalesceFlag if it is an ftp-url
        // need this later on when coalescing the resulting URL
        if (SegmentIs(relpath, scheme, "ftp", PR_TRUE)) {
            coalesceFlag = (netCoalesceFlags) (coalesceFlag 
                                        | NET_COALESCE_ALLOW_RELATIVE_ROOT
                                        | NET_COALESCE_DOUBLE_SLASH_IS_ROOT);

        }
        // this URL appears to be absolute
        // but try to find out more
        if (SegmentIs(mScheme, relpath, scheme, PR_TRUE)) {
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
                relative = PR_TRUE;
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
            relative = PR_TRUE;
        }
    }
    if (relative) {
        PRUint32 len = 0;
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
    PRBool isEquals = PR_FALSE;
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
    // todo:  also check for file matches which include '?', '#', and ';'
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
    PRBool isEquals = PR_FALSE;
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
    PRBool isFileScheme = SegmentIs(mScheme, "file");
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
    // todo:  also check for file matches with '#', '?' and ';'
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
    PRUint32 startPos = stdurl2->mScheme.mPos + thatIndex - stdurl2->mSpec.get();
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
nsStandardURL::GetParam(nsACString &result)
{
    result = Param();
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
        nsCAutoString spec;
        PRUint32 dirPos, basePos, extPos;
        PRInt32 dirLen, baseLen, extLen;
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
            PRUint32 end = mFilepath.mPos + mFilepath.mLen;
            if (mSpec.Length() > end)
                spec.Append(mSpec.get() + end, mSpec.Length() - end);
        }

        return SetSpec(spec);
    }
    else if (mPath.mLen > 1) {
        mSpec.Cut(mPath.mPos + 1, mFilepath.mLen - 1);
        // left shift param, query, and ref
        ShiftFromParam(1 - mFilepath.mLen);
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
nsStandardURL::SetParam(const nsACString &input)
{
    NS_NOTYETIMPLEMENTED("");
    return NS_ERROR_NOT_IMPLEMENTED;
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

    PRInt32 queryLen = strlen(query);
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
    nsCAutoString buf;
    PRBool encoded;
    GET_QUERY_ENCODER(encoder);
    encoder.EncodeSegmentCount(query, URLSegment(0, queryLen), esc_Query,
                               buf, encoded);
    if (encoded) {
        query = buf.get();
        queryLen = buf.Length();
    }

    PRInt32 shift = ReplaceSegment(mQuery.mPos, mQuery.mLen, query, queryLen);

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

    if (mPath.mLen < 0)
        return SetPath(flat);

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
            
    PRInt32 refLen = strlen(ref);
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

    // encode ref if necessary
    nsCAutoString buf;
    PRBool encoded;
    GET_SEGMENT_ENCODER(encoder);
    encoder.EncodeSegmentCount(ref, URLSegment(0, refLen), esc_Ref,
                               buf, encoded);
    if (encoded) {
        ref = buf.get();
        refLen = buf.Length();
    }

    PRInt32 shift = ReplaceSegment(mRef.mPos, mRef.mLen, ref, refLen);
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

    PRInt32 shift = 0;

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
                PRUint32 len = mBasename.mLen;
                if (mExtension.mLen >= 0)
                    len += (mExtension.mLen + 1);
                mSpec.Cut(mBasename.mPos, len);
                shift = -PRInt32(len);
                mBasename.mLen = 0;
                mExtension.mLen = -1;
            }
        }
        else {
            nsCAutoString newFilename;
            PRBool ignoredOut;
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
                PRUint32 oldLen = PRUint32(mBasename.mLen);
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
        ShiftFromParam(shift);
        mFilepath.mLen += shift;
        mPath.mLen += shift;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetFileBaseName(const nsACString &input)
{
    nsCAutoString extension;
    nsresult rv = GetFileExtension(extension);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString newFileName(input);

    if (!extension.IsEmpty()) {
        newFileName.Append('.');
        newFileName.Append(extension);
    }

    return SetFileName(newFileName);
}

NS_IMETHODIMP
nsStandardURL::SetFileExtension(const nsACString &input)
{
    nsCAutoString newFileName;
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
        NS_ERROR("url not initialized");
        return NS_ERROR_NOT_INITIALIZED;
    }

    if (!SegmentIs(mScheme, "file")) {
        NS_ERROR("not a file URL");
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

#if defined(PR_LOGGING)
    if (LOG_ENABLED()) {
        nsCAutoString path;
        mFile->GetNativePath(path);
        LOG(("nsStandardURL::GetFile [this=%p spec=%s resulting_path=%s]\n",
            this, mSpec.get(), path.get()));
    }
#endif

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
    nsCAutoString url;

    rv = net_GetURLSpecFromFile(file, url);
    if (NS_FAILED(rv)) return rv;

    SetSpec(url);

    rv = Init(mURLType, mDefaultPort, url, nsnull, nsnull);

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

inline PRBool
IsUTFCharset(const char *aCharset)
{
    return ((aCharset[0] == 'U' || aCharset[0] == 'u') &&
            (aCharset[1] == 'T' || aCharset[1] == 't') &&
            (aCharset[2] == 'F' || aCharset[2] == 'f'));
}

NS_IMETHODIMP
nsStandardURL::Init(PRUint32 urlType,
                    PRInt32 defaultPort,
                    const nsACString &spec,
                    const char *charset,
                    nsIURI *baseURI)
{
    ENSURE_MUTABLE();

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

    if (charset == nsnull || *charset == '\0') {
        // check if baseURI provides an origin charset and use that.
        if (baseURI)
            baseURI->GetOriginCharset(mOriginCharset);

        // URI can't be encoded in UTF-16, UTF-16BE, UTF-16LE, UTF-32,
        // UTF-32-LE, UTF-32LE, UTF-32BE (yet?). Truncate mOriginCharset if
        // it starts with "utf" (since an empty mOriginCharset implies
        // UTF-8, this is safe even if mOriginCharset is UTF-8).

        if (mOriginCharset.Length() > 3 &&
            IsUTFCharset(mOriginCharset.get())) {
            mOriginCharset.Truncate();
        }
    }
    else if (!IsUTFCharset(charset)) {
        mOriginCharset = charset;
    }

    if (baseURI) {
        PRUint32 start, end;
        // pull out the scheme and where it ends
        nsresult rv = net_ExtractURLScheme(spec, &start, &end, nsnull);
        if (NS_SUCCEEDED(rv) && spec.Length() > end+2) {
            nsACString::const_iterator slash;
            spec.BeginReading(slash);
            slash.advance(end+1);
            // then check if // follows
            // if it follows, aSpec is really absolute ... 
            // ignore aBaseURI in this case
            if (*slash == '/' && *(++slash) == '/')
                baseURI = nsnull;
        }
    }

    if (!baseURI)
        return SetSpec(spec);

    nsCAutoString buf;
    nsresult rv = baseURI->Resolve(spec, buf);
    if (NS_FAILED(rv)) return rv;

    return SetSpec(buf);
}

NS_IMETHODIMP
nsStandardURL::GetMutable(PRBool *value)
{
    *value = mMutable;
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetMutable(PRBool value)
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
    
    PRUint32 urlType;
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

    rv = stream->Read32((PRUint32 *) &mPort);
    if (NS_FAILED(rv)) return rv;

    rv = stream->Read32((PRUint32 *) &mDefaultPort);
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

    rv = ReadSegment(stream, mParam);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mQuery);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mRef);
    if (NS_FAILED(rv)) return rv;

    rv = NS_ReadOptionalCString(stream, mOriginCharset);
    if (NS_FAILED(rv)) return rv;

    PRBool isMutable;
    rv = stream->ReadBoolean(&isMutable);
    if (NS_FAILED(rv)) return rv;
    if (isMutable != PR_TRUE && isMutable != PR_FALSE) {
        NS_WARNING("Unexpected boolean value");
        return NS_ERROR_UNEXPECTED;
    }
    mMutable = isMutable;

    PRBool supportsFileURL;
    rv = stream->ReadBoolean(&supportsFileURL);
    if (NS_FAILED(rv)) return rv;
    if (supportsFileURL != PR_TRUE && supportsFileURL != PR_FALSE) {
        NS_WARNING("Unexpected boolean value");
        return NS_ERROR_UNEXPECTED;
    }
    mSupportsFileURL = supportsFileURL;

    PRUint32 hostEncoding;
    rv = stream->Read32(&hostEncoding);
    if (NS_FAILED(rv)) return rv;
    if (hostEncoding != eEncoding_ASCII && hostEncoding != eEncoding_UTF8) {
        NS_WARNING("Unexpected host encoding");
        return NS_ERROR_UNEXPECTED;
    }
    mHostEncoding = hostEncoding;
    
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::Write(nsIObjectOutputStream *stream)
{
    nsresult rv;

    rv = stream->Write32(mURLType);
    if (NS_FAILED(rv)) return rv;

    rv = stream->Write32(PRUint32(mPort));
    if (NS_FAILED(rv)) return rv;

    rv = stream->Write32(PRUint32(mDefaultPort));
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

    rv = WriteSegment(stream, mParam);
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
// nsStandardURL::nsIIPCSerializable
//---------------------------------------------------------------------------

PRBool
nsStandardURL::Read(const IPC::Message *aMsg, void **aIter)
{
#ifdef MOZ_IPC
    using IPC::ReadParam;
    
    NS_PRECONDITION(!mHostA, "Shouldn't have cached ASCII host");
    NS_PRECONDITION(mSpecEncoding == eEncoding_Unknown,
                    "Shouldn't have spec encoding here");
    NS_PRECONDITION(!mFile, "Shouldn't have cached file");
    
    PRUint32 urlType;
    if (!ReadParam(aMsg, aIter, &urlType))
        return PR_FALSE;
    
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
            return PR_FALSE;
    }

    PRUint32 hostEncoding;
    bool isMutable, supportsFileURL;
    if (!ReadParam(aMsg, aIter, &mPort) ||
        !ReadParam(aMsg, aIter, &mDefaultPort) ||
        !ReadParam(aMsg, aIter, &mSpec) ||
        !ReadSegment(aMsg, aIter, mScheme) ||
        !ReadSegment(aMsg, aIter, mAuthority) ||
        !ReadSegment(aMsg, aIter, mUsername) ||
        !ReadSegment(aMsg, aIter, mPassword) ||
        !ReadSegment(aMsg, aIter, mHost) ||
        !ReadSegment(aMsg, aIter, mPath) ||
        !ReadSegment(aMsg, aIter, mFilepath) ||
        !ReadSegment(aMsg, aIter, mDirectory) ||
        !ReadSegment(aMsg, aIter, mBasename) ||
        !ReadSegment(aMsg, aIter, mExtension) ||
        !ReadSegment(aMsg, aIter, mParam) ||
        !ReadSegment(aMsg, aIter, mQuery) ||
        !ReadSegment(aMsg, aIter, mRef) ||
        !ReadParam(aMsg, aIter, &mOriginCharset) ||
        !ReadParam(aMsg, aIter, &isMutable) ||
        !ReadParam(aMsg, aIter, &supportsFileURL) ||
        !ReadParam(aMsg, aIter, &hostEncoding))
        return PR_FALSE;

    if (hostEncoding != eEncoding_ASCII && hostEncoding != eEncoding_UTF8) {
        NS_WARNING("Unexpected host encoding");
        return PR_FALSE;
    }
    mHostEncoding = hostEncoding;
    mMutable = isMutable;
    mSupportsFileURL = supportsFileURL;

    // mSpecEncoding and mHostA are just caches that can be recovered as needed.

    return PR_TRUE;
#else
    return PR_FALSE;
#endif
}

void
nsStandardURL::Write(IPC::Message *aMsg)
{
#ifdef MOZ_IPC
    using IPC::WriteParam;
    
    WriteParam(aMsg, mURLType);
    WriteParam(aMsg, mPort);
    WriteParam(aMsg, mDefaultPort);
    WriteParam(aMsg, mSpec);
    WriteSegment(aMsg, mScheme);
    WriteSegment(aMsg, mAuthority);
    WriteSegment(aMsg, mUsername);
    WriteSegment(aMsg, mPassword);
    WriteSegment(aMsg, mHost);
    WriteSegment(aMsg, mPath);
    WriteSegment(aMsg, mFilepath);
    WriteSegment(aMsg, mDirectory);
    WriteSegment(aMsg, mBasename);
    WriteSegment(aMsg, mExtension);
    WriteSegment(aMsg, mParam);
    WriteSegment(aMsg, mQuery);
    WriteSegment(aMsg, mRef);
    WriteParam(aMsg, mOriginCharset);
    WriteParam(aMsg, bool(mMutable));
    WriteParam(aMsg, bool(mSupportsFileURL));
    WriteParam(aMsg, mHostEncoding);
    // mSpecEncoding and mHostA are just caches that can be recovered as needed.
#endif
}

//----------------------------------------------------------------------------
// nsStandardURL::nsIClassInfo
//----------------------------------------------------------------------------

NS_IMETHODIMP 
nsStandardURL::GetInterfaces(PRUint32 *count, nsIID * **array)
{
    *count = 0;
    *array = nsnull;
    return NS_OK;
}

NS_IMETHODIMP 
nsStandardURL::GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
{
    *_retval = nsnull;
    return NS_OK;
}

NS_IMETHODIMP 
nsStandardURL::GetContractID(char * *aContractID)
{
    *aContractID = nsnull;
    return NS_OK;
}

NS_IMETHODIMP 
nsStandardURL::GetClassDescription(char * *aClassDescription)
{
    *aClassDescription = nsnull;
    return NS_OK;
}

NS_IMETHODIMP 
nsStandardURL::GetClassID(nsCID * *aClassID)
{
    *aClassID = (nsCID*) nsMemory::Alloc(sizeof(nsCID));
    if (!*aClassID)
        return NS_ERROR_OUT_OF_MEMORY;
    return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP 
nsStandardURL::GetImplementationLanguage(PRUint32 *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
}

NS_IMETHODIMP 
nsStandardURL::GetFlags(PRUint32 *aFlags)
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
