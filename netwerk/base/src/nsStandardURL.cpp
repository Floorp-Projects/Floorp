/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsStandardURL.h"
#include "nsURLHelper.h"
#include "nsDependentSubstring.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsEscape.h"
#include "nsILocalFile.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "prlog.h"

static NS_DEFINE_CID(kThisImplCID, NS_THIS_STANDARDURL_IMPL_CID);

nsIURLParser *nsStandardURL::gNoAuthParser = nsnull;
nsIURLParser *nsStandardURL::gAuthParser = nsnull;
nsIURLParser *nsStandardURL::gStdParser = nsnull;
PRBool nsStandardURL::gInitialized = PR_FALSE;

#if defined(PR_LOGGING)
//
// setenv NSPR_LOG_MODULES nsStandardURL:5
//
static PRLogModuleInfo *gStandardURLLog;
#endif
#define LOG(args)     PR_LOG(gStandardURLLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gStandardURLLog, PR_LOG_DEBUG)

//----------------------------------------------------------------------------

static PRInt32
AppendEscaped(nsACString &buf, const char *str, PRInt32 len, PRInt16 mask)
{
    nsCAutoString escaped;
    if (NS_EscapeURLPart(str, len, mask, escaped)) {
        str = escaped.get();
        len = escaped.Length();
    }
    buf.Append(str, len);
    return len;
}

//----------------------------------------------------------------------------
// nsStandardURL <public>
//----------------------------------------------------------------------------

nsStandardURL::nsStandardURL()
    : mDefaultPort(-1)
    , mPort(-1)
    , mURLType(URLTYPE_STANDARD)
{
#if defined(PR_LOGGING)
    if (!gStandardURLLog)
        gStandardURLLog = PR_NewLogModule("nsStandardURL");
#endif

    LOG(("Creating nsStandardURL @%p\n", this));

    NS_INIT_ISUPPORTS();

    if (!gInitialized) {
        gInitialized = PR_TRUE;
        InitGlobalObjects();
    }

    // default parser in case nsIStandardURL::Init is never called
    mParser = gStdParser;
}

nsStandardURL::~nsStandardURL()
{
    LOG(("Destroying nsStandardURL @%p\n", this));
    // nothing to do
}

void
nsStandardURL::InitGlobalObjects()
{
    nsCOMPtr<nsIURLParser> parser;

    parser = do_GetService(NS_NOAUTHURLPARSER_CONTRACTID);
    NS_ASSERTION(parser, "failed getting 'noauth' url parser");
    if (parser) {
        gNoAuthParser = parser.get();
        NS_ADDREF(gNoAuthParser);
    }

    parser = do_GetService(NS_AUTHURLPARSER_CONTRACTID);
    NS_ASSERTION(parser, "failed getting 'auth' url parser");
    if (parser) {
        gAuthParser = parser.get();
        NS_ADDREF(gAuthParser);
    }

    parser = do_GetService(NS_STDURLPARSER_CONTRACTID);
    NS_ASSERTION(parser, "failed getting 'std' url parser");
    if (parser) {
        gStdParser = parser.get();
        NS_ADDREF(gStdParser);
    }
}

void
nsStandardURL::ShutdownGlobalObjects()
{
    NS_IF_RELEASE(gNoAuthParser);
    NS_IF_RELEASE(gAuthParser);
    NS_IF_RELEASE(gStdParser);
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
    mHostname.Reset();

    mPath.Reset();
    mFilePath.Reset();
    mDirectory.Reset();
    mBasename.Reset();

    mExtension.Reset();
    mParam.Reset();
    mQuery.Reset();
    mRef.Reset();

    mFile = 0;
}

PRInt32
nsStandardURL::EscapeSegment(const char *str, const URLSegment &seg, PRInt16 mask, nsCString &result)
{
    PRInt32 len = 0;
    if (seg.mLen > 0) {
        if (NS_EscapeURLPart(str + seg.mPos, seg.mLen, mask, result))
            len = result.Length();
        else
            len = seg.mLen;
    }
    return len;
}

PRBool
nsStandardURL::EscapeHost(const char *host, nsCString &result)
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
nsStandardURL::FilterString(const char *str, nsCString &result)
{
    PRBool writing = PR_FALSE;
    result.Truncate();
    const char *p = str;
    for (; *p; ++p) {
        if (*p == '\t' || *p == '\r' || *p == '\n') {
            writing = PR_TRUE;
            // append chars up to but not including *p
            if (p > str)
                result.Append(str, p - str);
            str = p + 1;
        }
    }
    if (writing && p > str)
        result.Append(str, p - str);
    return writing;
}

void
nsStandardURL::CoalescePath(char *path)
{
    CoalesceDirsAbs(path);
    PRInt32 newLen = strlen(path);
    if (newLen < mPath.mLen) {
        PRInt32 diff = newLen - mPath.mLen;
        mPath.mLen = newLen;
        mDirectory.mLen += diff;
        mFilePath.mLen += diff;
        ShiftFromBasename(diff);
    }
}

PRUint32
nsStandardURL::AppendSegmentToBuf(char *buf, PRUint32 i, const char *str, URLSegment &seg, const nsCString *escapedStr)
{
    if (seg.mLen > 0) {
        if (escapedStr && !escapedStr->IsEmpty()) {
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
    nsCAutoString escapedScheme;
    nsCAutoString escapedUsername;
    nsCAutoString escapedPassword;
    nsCAutoString escapedDirectory;
    nsCAutoString escapedBasename;
    nsCAutoString escapedExtension;
    nsCAutoString escapedParam;
    nsCAutoString escapedQuery;
    nsCAutoString escapedRef;

    //
    // escape each URL segment, if necessary, and calculate approximate normalized
    // spec length.
    //
    PRInt32 approxLen = 3; // includes room for "://"

    approxLen += EscapeSegment(spec, mScheme,    esc_Scheme,        escapedScheme);
    approxLen += EscapeSegment(spec, mUsername,  esc_Username,      escapedUsername);
    approxLen += EscapeSegment(spec, mPassword,  esc_Password,      escapedPassword);
    approxLen += EscapeSegment(spec, mDirectory, esc_Directory,     escapedDirectory);
    approxLen += EscapeSegment(spec, mBasename,  esc_FileBaseName,  escapedBasename);
    approxLen += EscapeSegment(spec, mExtension, esc_FileExtension, escapedExtension);
    approxLen += EscapeSegment(spec, mParam,     esc_Param,         escapedParam);
    approxLen += EscapeSegment(spec, mQuery,     esc_Query,         escapedQuery);
    approxLen += EscapeSegment(spec, mRef,       esc_Ref,           escapedRef);

    // do not escape the hostname, if IPv6 address literal, mHostname will
    // already point to a [ ] delimited IPv6 address literal.
    if (mHostname.mLen > 0)
        approxLen += mHostname.mLen;

    //
    // generate the normalized URL string
    //
    char *buf = (char *) malloc(approxLen + 32);
    if (!buf)
        return NS_ERROR_OUT_OF_MEMORY;
    PRUint32 i = 0;

    if (mScheme.mLen > 0) {
        i = AppendSegmentToBuf(buf, i, spec, mScheme, &escapedScheme);
        ToLowerCase(buf + mScheme.mPos, mScheme.mLen);
        i = AppendToBuf(buf, i, "://", 3);
    }

    // record authority starting position
    mAuthority.mPos = i;

    // append authority
    if (mUsername.mLen > 0) {
        i = AppendSegmentToBuf(buf, i, spec, mUsername, &escapedUsername);
        if (mPassword.mLen >= 0) {
            buf[i++] = ':';
            i = AppendSegmentToBuf(buf, i, spec, mPassword, &escapedPassword);
        }
        buf[i++] = '@';
    }
    if (mHostname.mLen > 0) {
        i = AppendSegmentToBuf(buf, i, spec, mHostname);
        ToLowerCase(buf + mHostname.mPos, mHostname.mLen);
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
        mDirectory.mPos = mFilePath.mPos = mPath.mPos = i;
        mDirectory.mLen = mFilePath.mLen = mPath.mLen = 1;
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
        }

        // record corrected (file)path starting position
        mPath.mPos = mFilePath.mPos = i - leadingSlash;

        i = AppendSegmentToBuf(buf, i, spec, mDirectory, &escapedDirectory);

        // the directory must end with a '/'
        if (buf[i-1] != '/') {
            buf[i++] = '/';
            mDirectory.mLen++;
        }

        i = AppendSegmentToBuf(buf, i, spec, mBasename, &escapedBasename);

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
            i = AppendSegmentToBuf(buf, i, spec, mExtension, &escapedExtension);
        }
        // calculate corrected filepath length
        mFilePath.mLen = i - mFilePath.mPos;

        if (mParam.mLen >= 0) {
            buf[i++] = ';';
            i = AppendSegmentToBuf(buf, i, spec, mParam, &escapedParam);
        }
        if (mQuery.mLen >= 0) {
            buf[i++] = '?';
            i = AppendSegmentToBuf(buf, i, spec, mQuery, &escapedQuery);
        }
        if (mRef.mLen >= 0) {
            buf[i++] = '#';
            i = AppendSegmentToBuf(buf, i, spec, mRef, &escapedRef);
        }
        // calculate corrected path length
        mPath.mLen = i - mPath.mPos;
    }

    buf[i] = '\0';

    if (mDirectory.mLen > 1)
        CoalescePath(buf + mDirectory.mPos);

    mSpec.Adopt(buf);
    return NS_OK;
}

PRBool
nsStandardURL::SegmentIs(const URLSegment &seg, const char *val)
{
    // one or both may be null
    if (!val || mSpec.IsEmpty())
        return (!val && (mSpec.IsEmpty() || seg.mLen < 0));
    if (seg.mLen < 0)
        return PR_FALSE;
    // if the first |seg.mLen| chars of |val| match, then |val| must
    // also be null terminated at |seg.mLen|.
    return !nsCRT::strncasecmp(mSpec.get() + seg.mPos, val, seg.mLen)
        && (val[seg.mLen] == '\0');
}

PRBool
nsStandardURL::SegmentIs(const URLSegment &seg1, const char *val, const URLSegment &seg2)
{
    if (seg1.mLen != seg2.mLen)
        return PR_FALSE;
    if (seg1.mLen == -1 || (!val && mSpec.IsEmpty()))
        return PR_TRUE; // both are empty
    return !nsCRT::strncasecmp(mSpec.get() + seg1.mPos, val + seg2.mPos, seg1.mLen); 
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

nsresult
nsStandardURL::ParseURL(const char *spec)
{
    nsresult rv;

    //
    // parse given URL string
    //
    rv = mParser->ParseURL(spec, -1,
                           &mScheme.mPos, &mScheme.mLen,
                           &mAuthority.mPos, &mAuthority.mLen,
                           &mPath.mPos, &mPath.mLen);
    if (NS_FAILED(rv)) return rv;

#ifdef DEBUG
    if (mScheme.mLen <= 0)
        NS_WARNING("malformed url: no scheme");
#endif
     
    if (mAuthority.mLen > 0) {
        rv = mParser->ParseAuthority(spec + mAuthority.mPos, mAuthority.mLen,
                                     &mUsername.mPos, &mUsername.mLen,
                                     &mPassword.mPos, &mPassword.mLen,
                                     &mHostname.mPos, &mHostname.mLen,
                                     &mPort);
        if (NS_FAILED(rv)) return rv;

        mUsername.mPos += mAuthority.mPos;
        mPassword.mPos += mAuthority.mPos;
        mHostname.mPos += mAuthority.mPos;
    }

    if (mPath.mLen > 0)
        rv = ParsePath(spec, mPath.mPos, mPath.mLen);

    return rv;
}

nsresult
nsStandardURL::ParsePath(const char *spec, PRUint32 pathPos, PRInt32 pathLen)
{
    nsresult rv = mParser->ParsePath(spec + pathPos, pathLen,
                                     &mFilePath.mPos, &mFilePath.mLen,
                                     &mParam.mPos, &mParam.mLen,
                                     &mQuery.mPos, &mQuery.mLen,
                                     &mRef.mPos, &mRef.mLen);
    if (NS_FAILED(rv)) return rv;

    mFilePath.mPos += pathPos;
    mParam.mPos += pathPos;
    mQuery.mPos += pathPos;
    mRef.mPos += pathPos;

    if (mFilePath.mLen > 0) {
        rv = mParser->ParseFilePath(spec + mFilePath.mPos, mFilePath.mLen,
                                    &mDirectory.mPos, &mDirectory.mLen,
                                    &mBasename.mPos, &mBasename.mLen,
                                    &mExtension.mPos, &mExtension.mLen);
        if (NS_FAILED(rv)) return rv;

        mDirectory.mPos += mFilePath.mPos;
        mBasename.mPos += mFilePath.mPos;
        mExtension.mPos += mFilePath.mPos;
    }
    return NS_OK;
}

nsresult
nsStandardURL::NewSubstring(PRUint32 pos, PRInt32 len, char **result, PRBool unescaped)
{
    if (len < 0) {
        *result = nsnull;
        return NS_OK;
    }
    *result = ToNewCString(Substring(mSpec, pos, len));
    if (!*result)
        return NS_ERROR_OUT_OF_MEMORY;
    if (unescaped)
        NS_UnescapeURL(*result);
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

    char *result = (char *) malloc(len + tailLen + 1);
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

//----------------------------------------------------------------------------
// nsStandardURL::nsISupports
//----------------------------------------------------------------------------

NS_IMPL_ADDREF(nsStandardURL)
NS_IMPL_RELEASE(nsStandardURL)

NS_INTERFACE_MAP_BEGIN(nsStandardURL)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStandardURL)
    NS_INTERFACE_MAP_ENTRY(nsIURI)
    NS_INTERFACE_MAP_ENTRY(nsIURL)
    NS_INTERFACE_MAP_ENTRY(nsIFileURL)
    NS_INTERFACE_MAP_ENTRY(nsIStandardURL)
    NS_INTERFACE_MAP_ENTRY(nsISerializable)
    // see nsStandardURL::Equals
    if (aIID.Equals(kThisImplCID))
        foundInterface = NS_STATIC_CAST(nsIURI *, this);
    else
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------------
// nsStandardURL::nsIURI
//----------------------------------------------------------------------------

NS_IMETHODIMP
nsStandardURL::GetSpec(char **result)
{
    *result = ToNewCString(mSpec);
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::GetPrePath(char **result)
{
    if (mAuthority.mLen < 0) {
        *result = nsnull;
        return NS_OK;
    }
    return NewSubstring(mScheme.mPos, mAuthority.mPos + mAuthority.mLen, result);
}

NS_IMETHODIMP
nsStandardURL::GetScheme(char **result)
{
    return NewSubstring(mScheme, result, PR_TRUE);
}

NS_IMETHODIMP
nsStandardURL::GetPreHost(char **result)
{
    if (mUsername.mLen < 0) {
        *result = nsnull;
        return NS_OK;
    }
    if (mPassword.mLen < 0) {
        // prehost = <username>
        return NewSubstring(mUsername, result);
    }
    // prehost = <username>:<password>
    return NewSubstring(mUsername.mPos, mUsername.mLen + mPassword.mLen + 1, result);
}

NS_IMETHODIMP
nsStandardURL::GetUsername(char **result)
{
    return NewSubstring(mUsername, result, PR_TRUE);
}

NS_IMETHODIMP
nsStandardURL::GetPassword(char **result)
{
    return NewSubstring(mPassword, result, PR_TRUE);
}

NS_IMETHODIMP
nsStandardURL::GetHost(char **result)
{
    if (mHostname.mLen <= 0) {
        *result = nsnull;
        return NS_OK;
    }
    nsresult rv;
    // strip []'s if hostname is an IPv6 address literal
    if (mSpec.CharAt(mHostname.mPos) == '[')
        rv = NewSubstring(mHostname.mPos + 1, mHostname.mLen - 2, result);
    else
        rv = NewSubstring(mHostname, result);
    return rv;
}

NS_IMETHODIMP
nsStandardURL::GetPort(PRInt32 *result)
{
    *result = mPort;
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::GetPath(char **result)
{
    return NewSubstring(mPath, result);
}

NS_IMETHODIMP
nsStandardURL::SetSpec(const char *spec)
{
    LOG(("nsStandardURL::SetSpec [spec=%s]\n", spec));

    NS_PRECONDITION(spec, "null pointer");

    Clear();

    // filter out unexpected chars "\r\n\t" if necessary
    nsCAutoString filteredSpec;
    if (FilterString(spec, filteredSpec))
        spec = filteredSpec.get();

    // parse the given URL...
    nsresult rv = ParseURL(spec);
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
        LOG((" hostname  = (%u,%d)\n", mHostname.mPos,  mHostname.mLen));
        LOG((" path      = (%u,%d)\n", mPath.mPos,      mPath.mLen));
        LOG((" filepath  = (%u,%d)\n", mFilePath.mPos,  mFilePath.mLen));
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
nsStandardURL::SetPrePath(const char *prepath)
{
    NS_NOTYETIMPLEMENTED("");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStandardURL::SetScheme(const char *scheme)
{
    LOG(("nsStandardURL::SetScheme [scheme=%s]\n", scheme));

    if (!(scheme && *scheme)) {
        NS_ERROR("cannot remove the scheme from an url");
        return NS_ERROR_UNEXPECTED;
    }
    if (mScheme.mLen < 0) {
        NS_ERROR("uninitialized");
        return NS_ERROR_NOT_INITIALIZED;
    }

    PRInt32 len = strlen(scheme);
    if (!IsValidScheme(scheme, len)) {
        NS_ERROR("the given url scheme contains invalid characters");
        return NS_ERROR_UNEXPECTED;
    }

    mFile = 0;
    PRInt32 shift = ReplaceSegment(mScheme.mPos, mScheme.mLen, scheme, len);

    if (shift) {
        mScheme.mLen = len;
        ShiftFromAuthority(shift);
    }

    // ensure new scheme is lowercase
    // XXX the string code's ToLowerCase doesn't operate on substrings
    ToLowerCase((char *)mSpec.get() + mScheme.mPos, mScheme.mLen);
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetPreHost(const char *prehost)
{
    LOG(("nsStandardURL::SetPreHost [prehost=%s]\n", prehost));

    if (mURLType == URLTYPE_NO_AUTHORITY) {
        NS_ERROR("cannot set prehost on no-auth url");
        return NS_ERROR_UNEXPECTED;
    }
    if (mAuthority.mLen < 0) {
        NS_ERROR("uninitialized");
        return NS_ERROR_NOT_INITIALIZED;
    }
    NS_PRECONDITION(mHostname.mLen >= 0, "wtf");

    mFile = 0;

    if (!(prehost && *prehost)) {
        // remove prehost
        if (mUsername.mLen >= 0) {
            if (mPassword.mLen > 0)
                mUsername.mLen += (mPassword.mLen + 1);
            mUsername.mLen++;
            mSpec.Cut(mUsername.mPos, mUsername.mLen);
            mAuthority.mLen -= mUsername.mLen;
            ShiftFromHostname(-mUsername.mLen);
            mUsername.mLen = -1;
            mPassword.mLen = -1;
        }
        return NS_OK;
    }

    nsresult rv;
    PRUint32 usernamePos, passwordPos;
    PRInt32 usernameLen, passwordLen;

    rv = mParser->ParseUserInfo(prehost, -1,
                                &usernamePos, &usernameLen,
                                &passwordPos, &passwordLen);
    if (NS_FAILED(rv)) return rv;

    // build new prehost in |buf|
    nsCAutoString buf;
    if (usernameLen > 0) {
        usernameLen = AppendEscaped(buf, prehost + usernamePos,
                                    usernameLen, esc_Username);
        if (passwordLen >= 0) {
            buf.Append(':');
            passwordLen = AppendEscaped(buf, prehost + passwordPos,
                                        passwordLen, esc_Password);
        }
        if (mUsername.mLen < 0)
            buf.Append('@');
    }

    if (mUsername.mLen < 0) {
        // no existing prehost
        if (!buf.IsEmpty()) {
            mSpec.Insert(buf, mHostname.mPos);
            mUsername.mPos = mHostname.mPos;
            ShiftFromHostname(buf.Length());
        }
    }
    else {
        // replace existing prehost
        PRUint32 prehostLen = mUsername.mLen;
        if (mPassword.mLen >= 0)
            prehostLen += (mPassword.mLen + 1);
        mSpec.Replace(mUsername.mPos, prehostLen, buf);
        if (buf.Length() != prehostLen)
            ShiftFromHostname(buf.Length() - prehostLen);
    }
    // update positions and lengths
    mUsername.mLen = usernameLen;
    mPassword.mLen = passwordLen;
    if (passwordLen)
        mPassword.mPos = mUsername.mPos + mUsername.mLen + 1;
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetUsername(const char *username)
{
    LOG(("nsStandardURL::SetUsername [username=%s]\n", username));

    if (mURLType == URLTYPE_NO_AUTHORITY) {
        NS_ERROR("cannot set username on no-auth url");
        return NS_ERROR_UNEXPECTED;
    }

    if (!(username && *username))
        return SetPreHost(username);

    mFile = 0;

    PRInt32 len = strlen(username);

    // escape username if necessary
    nsCAutoString escaped;
    if (NS_EscapeURLPart(username, len, esc_Username, escaped)) {
        username = escaped.get();
        len = escaped.Length();
    }

    PRInt32 shift;

    if (mUsername.mLen < 0) {
        mSpec.Insert(username, mAuthority.mPos, len + 1);
        mSpec.SetCharAt('@', mAuthority.mPos + len);
        mUsername.mPos = mAuthority.mPos;
        mAuthority.mLen += len;
        shift = len + 1;
    }
    else
        shift = ReplaceSegment(mUsername.mPos, mUsername.mLen, username, len);

    if (shift) {
        mUsername.mLen = len;
        ShiftFromPassword(shift);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetPassword(const char *password)
{
    LOG(("nsStandardURL::SetPassword [password=%s]\n", password));

    if (mURLType == URLTYPE_NO_AUTHORITY) {
        NS_ERROR("cannot set password on no-auth url");
        return NS_ERROR_UNEXPECTED;
    }
    if (mUsername.mLen < 0) {
        NS_ERROR("cannot set password without existing username");
        return NS_ERROR_FAILURE;
    }

    mFile = 0;

    if (!password) {
        if (mPassword.mLen >= 0) {
            mSpec.Cut(mPassword.mPos - 1, mPassword.mLen + 1);
            ShiftFromHostname(-(mPassword.mLen + 1));
            mPassword.mLen = -1;
        }
        return NS_OK;
    }

    PRInt32 len = strlen(password);
    PRInt32 shift = 0;
    if (mPassword.mLen < 0) {
        mPassword.mPos = mUsername.mPos + mUsername.mLen;
        mSpec.Insert(':', mPassword.mPos);
        mPassword.mPos++;
        mPassword.mLen = 0;
    }

    // escape password if necessary
    nsCAutoString escaped;
    if (NS_EscapeURLPart(password, len, esc_Password, escaped)) {
        password = escaped.get();
        len = escaped.Length();
    }

    shift = ReplaceSegment(mPassword.mPos, mPassword.mLen, password, len);

    if (shift) {
        mPassword.mLen = len;
        ShiftFromHostname(shift);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetHost(const char *host)
{
    LOG(("nsStandardURL::SetHost [host=%s]\n", host));

    if (mURLType == URLTYPE_NO_AUTHORITY) {
        NS_ERROR("cannot set host on no-auth url");
        return NS_ERROR_UNEXPECTED;
    }

    mFile = 0;

    if (!(host && *host)) {
        // remove existing hostname
        if (mHostname.mLen > 0) {
            // remove entire authority
            mSpec.Cut(mAuthority.mPos, mAuthority.mLen);
            ShiftFromPath(-mAuthority.mLen);
            mAuthority.mLen = 0;
            mUsername.mLen = -1;
            mPassword.mLen = -1;
            mHostname.mLen = -1;
            mPort = -1;
        }
        return NS_OK;
    }

    // handle IPv6 unescaped address literal
    PRInt32 len;
    nsCAutoString escapedHost;
    if (EscapeHost(host, escapedHost)) {
        host = escapedHost.get();
        len = escapedHost.Length();
    }
    else
        len = strlen(host);

    if (mHostname.mLen < 0) {
        mHostname.mPos = mAuthority.mPos;
        mHostname.mLen = 0;
    }

    PRInt32 shift = ReplaceSegment(mHostname.mPos, mHostname.mLen, host, len);

    if (shift) {
        mHostname.mLen = len;
        mAuthority.mLen += shift;
        ShiftFromPath(shift);
    }
    return NS_OK;
}
             
NS_IMETHODIMP
nsStandardURL::SetPort(PRInt32 port)
{
    LOG(("nsStandardURL::SetPort [port=%d]\n", port));

    if ((port == mPort) || (mPort == -1 && port == mDefaultPort))
        return NS_OK;

    mFile = 0;

    if (mPort == -1) {
        // need to insert the port number in the URL spec
        nsCAutoString buf;
        buf.Assign(':');
        buf.AppendInt(port);
        mSpec.Insert(buf, mHostname.mPos + mHostname.mLen);
        ShiftFromPath(buf.Length());
    }
    else if (port == -1) {
        // need to remove the port number from the URL spec
        PRUint32 start = mHostname.mPos + mHostname.mLen;
        mSpec.Cut(start, mPath.mPos - start);
        ShiftFromPath(start - mPath.mPos);
    }
    else {
        // need to replace the existing port
        nsCAutoString buf;
        buf.AppendInt(port);
        PRUint32 start = mHostname.mPos + mHostname.mLen + 1;
        PRUint32 length = mPath.mPos - start;
        mSpec.Replace(start, length, buf);
        if (buf.Length() != length)
            ShiftFromPath(buf.Length() - length);
    }

    mPort = port;
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetPath(const char *path)
{
    LOG(("nsStandardURL::SetPath [path=%s]\n", path));

    mFile = 0;

    if (path && path[0]) {
        nsCAutoString spec;

        spec.Assign(mSpec.get(), mPath.mPos);
        if (path[0] != '/')
            spec.Append('/');
        spec.Append(path);

        return SetSpec(spec.get());
    }
    else if (mPath.mLen > 1) {
        mSpec.Cut(mPath.mPos + 1, mPath.mLen - 1);
        // these contain only a '/'
        mPath.mLen = 1;
        mDirectory.mLen = 1;
        mFilePath.mLen = 1;
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

    nsStandardURL *other;
    nsresult rv = unknownOther->QueryInterface(kThisImplCID, (void **) &other);
    if (NS_FAILED(rv)) {
        *result = PR_FALSE;
        return NS_OK;
    }

    *result = 
        SegmentIs(mScheme, other->mSpec.get(), other->mScheme) &&
        SegmentIs(mDirectory, other->mSpec.get(), other->mDirectory) &&
        SegmentIs(mBasename, other->mSpec.get(), other->mBasename) &&
        SegmentIs(mExtension, other->mSpec.get(), other->mExtension) &&
        SegmentIs(mHostname, other->mSpec.get(), other->mHostname) &&
        SegmentIs(mQuery, other->mSpec.get(), other->mQuery) &&
        SegmentIs(mRef, other->mSpec.get(), other->mRef) &&
        SegmentIs(mUsername, other->mSpec.get(), other->mUsername) &&
        SegmentIs(mPassword, other->mSpec.get(), other->mPassword) &&
        SegmentIs(mParam, other->mSpec.get(), other->mParam) &&
        (Port() == other->Port());

    NS_RELEASE(other);
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SchemeIs(const char *scheme, PRBool *result)
{
    NS_PRECONDITION(result, "null pointer");

    *result = SegmentIs(mScheme, scheme);
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::Clone(nsIURI **result)
{
    nsStandardURL *clone;
    NS_NEWXPCOM(clone, nsStandardURL);
    if (!clone)
        return NS_ERROR_OUT_OF_MEMORY;

    // XXX a copy-on-write string would be very nice here
    clone->mSpec = mSpec;
    clone->mDefaultPort = mDefaultPort;
    clone->mPort = mPort;
    clone->mScheme = mScheme;
    clone->mAuthority = mAuthority;
    clone->mUsername = mUsername;
    clone->mPassword = mPassword;
    clone->mHostname = mHostname;
    clone->mPath = mPath;
    clone->mFilePath = mFilePath;
    clone->mDirectory = mDirectory;
    clone->mBasename = mBasename;
    clone->mExtension = mExtension;
    clone->mParam = mParam;
    clone->mQuery = mQuery;
    clone->mRef = mRef;
    clone->mParser = mParser;
    clone->mFile = mFile;

    NS_ADDREF(*result = clone);
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::Resolve(const char *relpath, char **result)
{
    LOG(("nsStandardURL::Resolve [this=%p spec=%s relpath=%s]\n",
        this, mSpec.get(), relpath));

    NS_ENSURE_ARG_POINTER(relpath);
    NS_ENSURE_ARG_POINTER(result);

    NS_ASSERTION(gNoAuthParser, "no parser: unitialized");

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

    // relative urls should never contain a host, so we always want to use
    // the noauth url parser.
    rv = gNoAuthParser->ParseURL(relpath, -1,
                                 &scheme.mPos, &scheme.mLen,
                                 nsnull, nsnull,
                                 nsnull, nsnull);
    if (NS_FAILED(rv)) return rv;

    if (scheme.mLen >= 0) {
        // this URL appears to be absolute
        *result = nsCRT::strdup(relpath);
    }
    else if (relpath[0] == '/' && relpath[1] == '/') {
        // this URL is almost absolute
        *result = AppendToSubstring(mScheme.mPos, mScheme.mLen + 1, relpath);
    }
    else {
        PRUint32 len = 0;
        switch (*relpath) {
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
            // overwrite everything after the directory 
            len = mDirectory.mPos + mDirectory.mLen;
        }
        *result = AppendToSubstring(0, len, relpath);

        // locate result path
        resultPath = *result + mPath.mPos;
    }
    if (!*result)
        return NS_ERROR_OUT_OF_MEMORY;

    if (resultPath)
        CoalesceDirsRel(resultPath);
    else {
        // locate result path
        resultPath = PL_strstr(*result, "://");
        if (resultPath) {
            resultPath = PL_strchr(resultPath + 3, '/');
            if (resultPath)
                CoalesceDirsRel(resultPath);
        }
    }
    return NS_OK;
}

//----------------------------------------------------------------------------
// nsStandardURL::nsIURL
//----------------------------------------------------------------------------

NS_IMETHODIMP
nsStandardURL::GetFilePath(char **result)
{
    return NewSubstring(mFilePath, result);
}

NS_IMETHODIMP
nsStandardURL::GetParam(char **result)
{
    return NewSubstring(mParam, result, PR_TRUE);
}

NS_IMETHODIMP
nsStandardURL::GetQuery(char **result)
{
    return NewSubstring(mQuery, result, PR_TRUE);
}

NS_IMETHODIMP
nsStandardURL::GetRef(char **result)
{
    return NewSubstring(mRef, result, PR_TRUE);
}

NS_IMETHODIMP
nsStandardURL::GetDirectory(char **result)
{
    return NewSubstring(mDirectory, result);
}

NS_IMETHODIMP
nsStandardURL::GetFileName(char **result)
{
    URLSegment filename;
    if (mBasename.mLen >= 0) {
        if (mExtension.mLen >= 0) {
            filename.mPos = mBasename.mPos;
            filename.mLen = mExtension.mPos + mExtension.mLen - mBasename.mPos;
        }
        else {
            filename.mPos = mBasename.mPos;
            filename.mLen = mBasename.mLen;
        }
    }
    else if (mExtension.mLen >= 0) {
        filename.mPos = mExtension.mPos - 1;
        filename.mLen = mExtension.mLen + 1;
    }
    return NewSubstring(filename, result);
}

NS_IMETHODIMP
nsStandardURL::GetFileBaseName(char **result)
{
    return NewSubstring(mBasename, result, PR_TRUE);
}

NS_IMETHODIMP
nsStandardURL::GetFileExtension(char **result)
{
    return NewSubstring(mExtension, result, PR_TRUE);
}

NS_IMETHODIMP
nsStandardURL::GetEscapedQuery(char **result)
{
    return NewSubstring(mQuery, result);
}

NS_IMETHODIMP
nsStandardURL::SetFilePath(const char *filepath)
{
    LOG(("nsStandardURL::SetFilePath [filepath=%s]\n", filepath));

    // if there isn't a filepath, then there can't be anything
    // after the path either.  this url is likely uninitialized.
    if (mFilePath.mLen < 0)
        return SetPath(filepath);

    if (filepath && *filepath) {
        nsCAutoString spec;
        PRUint32 dirPos, basePos, extPos;
        PRInt32 dirLen, baseLen, extLen;
        nsresult rv;

        rv = gNoAuthParser->ParseFilePath(filepath, -1,
                                          &dirPos, &dirLen,
                                          &basePos, &baseLen,
                                          &extPos, &extLen);
        if (NS_FAILED(rv)) return rv;

        // build up new candidate spec
        spec.Assign(mSpec.get(), mPath.mPos);

        // ensure leading '/'
        if (filepath[dirPos] != '/')
            spec.Append('/');

        // append filepath components
        if (dirLen > 0)
            AppendEscaped(spec, filepath + dirPos, dirLen, esc_Directory);
        if (baseLen > 0)
            AppendEscaped(spec, filepath + basePos, baseLen, esc_FileBaseName);
        if (extLen >= 0) {
            spec.Append('.');
            if (extLen > 0)
                AppendEscaped(spec, filepath + extPos, extLen, esc_FileExtension);
        }

        // compute the ending position of the current filepath
        if (mFilePath.mLen >= 0) {
            PRUint32 end = mFilePath.mPos + mFilePath.mLen;
            if (mSpec.Length() > end)
                spec.Append(mSpec.get() + end, mSpec.Length() - end);
        }

        return SetSpec(spec.get());
    }
    else if (mPath.mLen > 1) {
        mSpec.Cut(mPath.mPos + 1, mFilePath.mLen - 1);
        // left shift param, query, and ref
        ShiftFromParam(1 - mFilePath.mLen);
        // these contain only a '/'
        mPath.mLen = 1;
        mDirectory.mLen = 1;
        mFilePath.mLen = 1;
        // these are no longer defined
        mBasename.mLen = -1;
        mExtension.mLen = -1;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetParam(const char *param)
{
    LOG(("nsStandardURL::SetParam [param=%s]\n", param));

    NS_NOTYETIMPLEMENTED("");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStandardURL::SetQuery(const char *query)
{
    LOG(("nsStandardURL::SetQuery [query=%s]\n", query));

    if (mPath.mLen < 0)
        return SetPath(query);

    mFile = 0;

    if (!query) {
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

    // escape query if necessary
    nsCAutoString escaped;
    if (NS_EscapeURLPart(query, queryLen, esc_Query, escaped)) {
        query = escaped.get();
        queryLen = escaped.Length();
    }

    PRInt32 shift = ReplaceSegment(mQuery.mPos, mQuery.mLen, query, queryLen);

    if (shift) {
        mQuery.mLen = queryLen;
        mPath.mLen += shift;
        ShiftFromRef(queryLen - mQuery.mLen);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetRef(const char *ref)
{
    LOG(("nsStandardURL::SetRef [ref=%s]\n", ref));

    if (mPath.mLen < 0)
        return SetPath(ref);

    mFile = 0;

    if (!ref) {
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
        mRef.mPos = mSpec.Length();
        mRef.mLen = 0;
    }

    // escape ref if necessary
    nsCAutoString escaped;
    if (NS_EscapeURLPart(ref, refLen, esc_Ref, escaped)) {
        ref = escaped.get();
        refLen = escaped.Length();
    }

    ReplaceSegment(mRef.mPos, mRef.mLen, ref, refLen);
    mPath.mLen += (refLen - mRef.mLen);
    mRef.mLen = refLen;
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetDirectory(const char *directory)
{
    LOG(("nsStandardURL::SetDirectory [directory=%s]\n", directory));

    NS_NOTYETIMPLEMENTED("");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStandardURL::SetFileName(const char *filename)
{
    LOG(("nsStandardURL::SetFileName [filename=%s]\n", filename));

    if (mPath.mLen < 0)
        return SetPath(filename);

    if (!(filename && *filename)) {
        // remove the filename
        if (mBasename.mLen > 0) {
            if (mExtension.mLen >= 0)
                mBasename.mLen += (mExtension.mLen + 1);
            mSpec.Cut(mBasename.mPos, mBasename.mLen);
            ShiftFromParam(-mBasename.mLen);
            mBasename.mLen = 0;
            mExtension.mLen = -1;
        }
        return NS_OK;
    }

    nsresult rv;
    URLSegment basename, extension;

    // let the parser locate the basename and extension
    rv = gNoAuthParser->ParseFileName(filename, -1,
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
            ShiftFromParam(-PRInt32(len));
            mBasename.mLen = 0;
            mExtension.mLen = -1;
        }
    }
    else {
        nsCAutoString newFilename;
        basename.mLen = AppendEscaped(newFilename, filename + basename.mPos,
                                      basename.mLen, esc_FileBaseName);
        if (extension.mLen >= 0) {
            newFilename.Append('.');
            extension.mLen = AppendEscaped(newFilename, filename + extension.mPos,
                                           extension.mLen, esc_FileExtension);
        }

        if (mBasename.mLen < 0) {
            // insert new filename
            mBasename.mPos = mDirectory.mPos + mDirectory.mLen;
            mSpec.Insert(newFilename, mBasename.mPos);
            ShiftFromParam(newFilename.Length());
        }
        else {
            // replace existing filename
            PRUint32 oldLen = PRUint32(mBasename.mLen);
            if (mExtension.mLen >= 0)
                oldLen += (mExtension.mLen + 1);
            mSpec.Replace(mBasename.mPos, oldLen, newFilename);
            if (oldLen != newFilename.Length())
                ShiftFromParam(newFilename.Length() - oldLen);
        }

        mBasename.mLen = basename.mLen;
        mExtension.mLen = extension.mLen;
        if (mExtension.mLen >= 0)
            mExtension.mPos = mBasename.mPos + mBasename.mLen + 1;
        mFilePath.mLen = mDirectory.mLen + newFilename.Length();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStandardURL::SetFileBaseName(const char *basename)
{
    LOG(("nsStandardURL::SetFileBaseName [basename=%s]\n", basename));

    NS_NOTYETIMPLEMENTED("");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStandardURL::SetFileExtension(const char *extension)
{
    LOG(("nsStandardURL::SetFileExtension [extension=%s]\n", extension));

    NS_NOTYETIMPLEMENTED("");
    return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------------
// nsStandardURL::nsIFileURL
//----------------------------------------------------------------------------

NS_IMETHODIMP
nsStandardURL::GetFile(nsIFile **result)
{
    // use cached result if present
    if (mFile) {
        NS_ADDREF(*result = mFile);
        return NS_OK;
    }

    if (mSpec.IsEmpty()) {
        NS_ERROR("url not initialized");
        return NS_ERROR_NOT_INITIALIZED;
    }

    if (!SegmentIs(mScheme, "file")) {
        NS_ERROR("not a file URL");
        return NS_ERROR_FAILURE;
    }

    nsresult rv;
    nsCOMPtr<nsILocalFile> localFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;

    rv = NS_InitFileFromURLSpec(localFile, mSpec.get());
    if (NS_FAILED(rv)) return rv;

#if defined(PR_LOGGING)
    if (LOG_ENABLED()) {
        nsXPIDLCString path;
        localFile->GetPath(getter_Copies(path));
        LOG(("nsStandardURL::GetFile [this=%p spec=%s resulting_path=%s]\n",
            this, mSpec.get(), path.get()));
    }
#endif

    return CallQueryInterface(localFile, result);
}

NS_IMETHODIMP
nsStandardURL::SetFile(nsIFile *file)
{
    NS_PRECONDITION(file, "null pointer");

    nsresult rv;
    nsXPIDLCString url;

    rv = NS_GetURLSpecFromFile(file, getter_Copies(url));
    if (NS_FAILED(rv)) return rv;

    rv = SetSpec(url);

    // must clone |file| since its value is not guaranteed to remain constant
    if (NS_SUCCEEDED(rv)) {
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

NS_IMETHODIMP
nsStandardURL::Init(PRUint32 urlType,
                    PRInt32 defaultPort,
                    const char *spec,
                    nsIURI *baseURI)
{
    mFile = 0;

    switch (urlType) {
    case URLTYPE_STANDARD:
        mParser = gStdParser;
        break;
    case URLTYPE_AUTHORITY:
        mParser = gAuthParser;
        break;
    case URLTYPE_NO_AUTHORITY:
        mParser = gNoAuthParser;
        break;
    default:
        NS_NOTREACHED("bad urlType");
        return NS_ERROR_INVALID_ARG;
    }
    mDefaultPort = defaultPort;

    if (!spec) {
        Clear();
        return NS_OK;
    }

    nsXPIDLCString buf;
    const char *resolvedSpec;
    if (baseURI) {
        nsresult rv = baseURI->Resolve(spec, getter_Copies(buf));
        if (NS_FAILED(rv)) return rv;
        resolvedSpec = buf.get();
    }
    else
        resolvedSpec = spec;

    return SetSpec(resolvedSpec);
}

//----------------------------------------------------------------------------
// nsStandardURL::nsISerializable
//----------------------------------------------------------------------------

NS_IMETHODIMP
nsStandardURL::Read(nsIObjectInputStream *stream)
{
    nsresult rv;
    
    rv = stream->Read32(&mURLType);
    if (NS_FAILED(rv)) return rv;
    switch (mURLType) {
      case URLTYPE_STANDARD:
        mParser = gStdParser;
        break;
      case URLTYPE_AUTHORITY:
        mParser = gAuthParser;
        break;
      case URLTYPE_NO_AUTHORITY:
        mParser = gNoAuthParser;
        break;
      default:
        NS_NOTREACHED("bad urlType");
        return NS_ERROR_FAILURE;
    }

    rv = stream->Read32((PRUint32 *) &mPort);
    if (NS_FAILED(rv)) return rv;

    rv = stream->Read32((PRUint32 *) &mDefaultPort);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString spec;
    rv = NS_ReadOptionalStringZ(stream, getter_Copies(spec));
    if (NS_FAILED(rv)) return rv;
    mSpec = spec;

    rv = ReadSegment(stream, mScheme);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mAuthority);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mUsername);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mPassword);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mHostname);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mPath);
    if (NS_FAILED(rv)) return rv;

    rv = ReadSegment(stream, mFilePath);
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

    rv = WriteSegment(stream, mHostname);
    if (NS_FAILED(rv)) return rv;

    rv = WriteSegment(stream, mPath);
    if (NS_FAILED(rv)) return rv;

    rv = WriteSegment(stream, mFilePath);
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

    return NS_OK;
}
