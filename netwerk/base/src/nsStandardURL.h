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

#ifndef nsStandardURL_h__
#define nsStandardURL_h__

#include "nsString.h"
#include "nsDependentString.h"
#include "nsDependentSubstring.h"
#include "nsISerializable.h"
#include "nsIFileURL.h"
#include "nsIStandardURL.h"
#include "nsIFile.h"
#include "nsIURLParser.h"
#include "nsIUnicodeEncoder.h"
#include "nsIObserver.h"
#include "nsIIOService.h"
#include "nsCOMPtr.h"

class nsIBinaryInputStream;
class nsIBinaryOutputStream;
class nsIIDNService;
class nsICharsetConverterManager2;

//-----------------------------------------------------------------------------
// standard URL implementation
//-----------------------------------------------------------------------------

class nsStandardURL : public nsIFileURL
                    , public nsIStandardURL
                    , public nsISerializable
                    , public nsIClassInfo
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIURI
    NS_DECL_NSIURL
    NS_DECL_NSIFILEURL
    NS_DECL_NSISTANDARDURL
    NS_DECL_NSISERIALIZABLE
    NS_DECL_NSICLASSINFO

    nsStandardURL(PRBool aSupportsFileURL = PR_FALSE);
    virtual ~nsStandardURL();

    static void InitGlobalObjects();
    static void ShutdownGlobalObjects();

public: /* internal -- HPUX compiler can't handle this being private */
    //
    // location and length of an url segment relative to mSpec
    //
    struct URLSegment
    {
        PRUint32 mPos;
        PRInt32  mLen;

        URLSegment() : mPos(0), mLen(-1) {}
        URLSegment(PRUint32 pos, PRInt32 len) : mPos(pos), mLen(len) {}
        void Reset() { mPos = 0; mLen = -1; }
    };

    //
    // Pref observer
    //
    class nsPrefObserver : public nsIObserver
    {
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIOBSERVER

        nsPrefObserver() { }
    };
    friend class nsPrefObserver;

    //
    // URL segment encoder : performs charset conversion and URL escaping.
    //
    class nsSegmentEncoder
    {
    public:
        nsSegmentEncoder(const char *charset);

        // Encode the given segment if necessary, and return the length of
        // the encoded segment.  The encoded segment is appended to |buf|
        // if and only if encoding is required.
        PRInt32 EncodeSegmentCount(const char *str,
                                   const URLSegment &segment,
                                   PRInt16 mask,
                                   nsAFlatCString &buf);
         
        // Encode the given string if necessary, and return a reference to
        // the encoded string.  Returns a reference to |buf| if encoding
        // is required.  Otherwise, a reference to |str| is returned.
        const nsACString &EncodeSegment(const nsASingleFragmentCString &str,
                                        PRInt16 mask,
                                        nsAFlatCString &buf);
    private:
        nsCOMPtr<nsIUnicodeEncoder> mEncoder;
    };
    friend class nsSegmentEncoder;

private:
    PRInt32  Port() { return mPort == -1 ? mDefaultPort : mPort; }

    void     Clear();
    void     InvalidateCache(PRBool invalidateCachedFile = PR_TRUE);

    PRBool   EncodeHost(const char *host, nsCString &result);
    void     CoalescePath(char *path);

    PRUint32 AppendSegmentToBuf(char *, PRUint32, const char *, URLSegment &, const nsCString *esc=nsnull);
    PRUint32 AppendToBuf(char *, PRUint32, const char *, PRUint32);

    nsresult BuildNormalizedSpec(const char *spec);

    PRBool   HostsAreEquivalent(nsStandardURL *other);

    PRBool   SegmentIs(const URLSegment &s1, const char *val);
    PRBool   SegmentIs(const URLSegment &s1, const char *val, const URLSegment &s2);

    PRInt32  ReplaceSegment(PRUint32 pos, PRUint32 len, const char *val, PRUint32 valLen);
    PRInt32  ReplaceSegment(PRUint32 pos, PRUint32 len, const nsACString &val);

    nsresult ParseURL(const char *spec);
    nsresult ParsePath(const char *spec, PRUint32 pathPos, PRInt32 pathLen = -1);

    char    *AppendToSubstring(PRUint32 pos, PRInt32 len, const char *tail, PRInt32 tailLen = -1);

    // dependent substring helpers
    const nsDependentSingleFragmentCSubstring Segment(PRUint32 pos, PRInt32 len); // see below
    const nsDependentSingleFragmentCSubstring Segment(const URLSegment &s) { return Segment(s.mPos, s.mLen); }

    // dependent substring getters
    const nsDependentSingleFragmentCSubstring Prepath();  // see below
    const nsDependentSingleFragmentCSubstring Scheme()    { return Segment(mScheme); }
    const nsDependentSingleFragmentCSubstring Userpass(PRBool includeDelim = PR_FALSE); // see below
    const nsDependentSingleFragmentCSubstring Username()  { return Segment(mUsername); }
    const nsDependentSingleFragmentCSubstring Password()  { return Segment(mPassword); }
    const nsDependentSingleFragmentCSubstring Hostport(); // see below
    const nsDependentSingleFragmentCSubstring Host();     // see below
    const nsDependentSingleFragmentCSubstring Path()      { return Segment(mPath); }
    const nsDependentSingleFragmentCSubstring Filepath()  { return Segment(mFilepath); }
    const nsDependentSingleFragmentCSubstring Directory() { return Segment(mDirectory); }
    const nsDependentSingleFragmentCSubstring Filename(); // see below
    const nsDependentSingleFragmentCSubstring Basename()  { return Segment(mBasename); }
    const nsDependentSingleFragmentCSubstring Extension() { return Segment(mExtension); }
    const nsDependentSingleFragmentCSubstring Param()     { return Segment(mParam); }
    const nsDependentSingleFragmentCSubstring Query()     { return Segment(mQuery); }
    const nsDependentSingleFragmentCSubstring Ref()       { return Segment(mRef); }

    // shift the URLSegments to the right by diff
    void ShiftFromAuthority(PRInt32 diff) { mAuthority.mPos += diff; ShiftFromUsername(diff); }
    void ShiftFromUsername(PRInt32 diff)  { mUsername.mPos += diff; ShiftFromPassword(diff); }
    void ShiftFromPassword(PRInt32 diff)  { mPassword.mPos += diff; ShiftFromHost(diff); }
    void ShiftFromHost(PRInt32 diff)      { mHost.mPos += diff; ShiftFromPath(diff); }
    void ShiftFromPath(PRInt32 diff)      { mPath.mPos += diff; ShiftFromFilepath(diff); }
    void ShiftFromFilepath(PRInt32 diff)  { mFilepath.mPos += diff; ShiftFromDirectory(diff); }
    void ShiftFromDirectory(PRInt32 diff) { mDirectory.mPos += diff; ShiftFromBasename(diff); }
    void ShiftFromBasename(PRInt32 diff)  { mBasename.mPos += diff; ShiftFromExtension(diff); }
    void ShiftFromExtension(PRInt32 diff) { mExtension.mPos += diff; ShiftFromParam(diff); }
    void ShiftFromParam(PRInt32 diff)     { mParam.mPos += diff; ShiftFromQuery(diff); }
    void ShiftFromQuery(PRInt32 diff)     { mQuery.mPos += diff; ShiftFromRef(diff); }
    void ShiftFromRef(PRInt32 diff)       { mRef.mPos += diff; }

    // fastload helper functions
    nsresult ReadSegment(nsIBinaryInputStream *, URLSegment &);
    nsresult WriteSegment(nsIBinaryOutputStream *, const URLSegment &);

    // mSpec contains the normalized version of the URL spec (UTF-8 encoded).
    nsCString mSpec;
    PRInt32   mDefaultPort;
    PRInt32   mPort;

    // url parts (relative to mSpec)
    URLSegment mScheme;
    URLSegment mAuthority;
    URLSegment mUsername;
    URLSegment mPassword;
    URLSegment mHost;
    URLSegment mPath;
    URLSegment mFilepath;
    URLSegment mDirectory;
    URLSegment mBasename;
    URLSegment mExtension;
    URLSegment mParam;
    URLSegment mQuery;
    URLSegment mRef;

    nsCString              mOriginCharset;
    PRUint32               mURLType;
    nsCOMPtr<nsIURLParser> mParser;
    nsCOMPtr<nsIFile>      mFile;  // cached result for nsIFileURL::GetFile
    char                  *mHostA; // cached result for nsIURI::GetHostA

    enum nsEncodingType {
        eEncoding_Unknown,
        eEncoding_ASCII,
        eEncoding_UTF8
    };
    nsEncodingType mHostEncoding;
    nsEncodingType mSpecEncoding;

    PRPackedBool mMutable;         // nsIStandardURL::mutable
    PRPackedBool mSupportsFileURL; // QI to nsIFileURL?

    // global objects.  don't use COMPtr as its destructor will cause a
    // coredump if we leak it.
    static nsIIDNService               *gIDNService;
    static nsICharsetConverterManager2 *gCharsetMgr;
    static PRBool                       gInitialized;
    static PRBool                       gEscapeUTF8;
};

#define NS_THIS_STANDARDURL_IMPL_CID                 \
{ /* b8e3e97b-1ccd-4b45-af5a-79596770f5d7 */         \
    0xb8e3e97b,                                      \
    0x1ccd,                                          \
    0x4b45,                                          \
    {0xaf, 0x5a, 0x79, 0x59, 0x67, 0x70, 0xf5, 0xd7} \
}

//-----------------------------------------------------------------------------
// Dependent substring getters
//-----------------------------------------------------------------------------

inline const nsDependentSingleFragmentCSubstring
nsStandardURL::Segment(PRUint32 pos, PRInt32 len)
{
    if (len < 0) {
        pos = 0;
        len = 0;
    }
    return Substring(mSpec, pos, PRUint32(len));
}

inline const nsDependentSingleFragmentCSubstring
nsStandardURL::Prepath()
{
    PRUint32 len = 0;
    if (mAuthority.mLen >= 0)
        len = mAuthority.mPos + mAuthority.mLen;
    return Substring(mSpec, 0, len);
}

inline const nsDependentSingleFragmentCSubstring
nsStandardURL::Userpass(int includeDelim)
{
    PRUint32 pos=0, len=0;
    // if there is no username, then there can be no password
    if (mUsername.mLen > 0) {
        pos = mUsername.mPos;
        len = mUsername.mLen;
        if (mPassword.mLen >= 0)
            len += (mPassword.mLen + 1);
        if (includeDelim)
            len++;
    }
    return Substring(mSpec, pos, len);
}

inline const nsDependentSingleFragmentCSubstring
nsStandardURL::Hostport()
{
    PRUint32 pos=0, len=0;
    if (mAuthority.mLen > 0) {
        pos = mHost.mPos;
        len = mAuthority.mPos + mAuthority.mLen - pos;
    }
    return Substring(mSpec, pos, len);
}

inline const nsDependentSingleFragmentCSubstring
nsStandardURL::Host()
{
    PRUint32 pos=0, len=0;
    if (mHost.mLen > 0) {
        pos = mHost.mPos;
        len = mHost.mLen;
        if (mSpec.CharAt(pos) == '[' && mSpec.CharAt(pos + len - 1) == ']') {
            pos++;
            len -= 2;
        }
    }
    return Substring(mSpec, pos, len);
}

inline const nsDependentSingleFragmentCSubstring
nsStandardURL::Filename()
{
    PRUint32 pos=0, len=0;
    // if there is no basename, then there can be no extension
    if (mBasename.mLen > 0) {
        pos = mBasename.mPos;
        len = mBasename.mLen;
        if (mExtension.mLen >= 0)
            len += (mExtension.mLen + 1);
    }
    return Substring(mSpec, pos, len);
}

#endif // nsStandardURL_h__
