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

#include "nsISerializable.h"
#include "nsIFileChannel.h"
#include "nsIFile.h"
#include "nsIURLParser.h"
#include "nsDependentString.h"
#include "nsDependentSubstring.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsCOMPtr.h"

class nsIBinaryInputStream;
class nsIBinaryOutputStream;

//-----------------------------------------------------------------------------
// standard URL implementation
//-----------------------------------------------------------------------------

class nsStandardURL : public nsIFileURL
                    , public nsIStandardURL
                    , public nsISerializable
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIURI
    NS_DECL_NSIURL
    NS_DECL_NSIFILEURL
    NS_DECL_NSISTANDARDURL
    NS_DECL_NSISERIALIZABLE

    nsStandardURL();
    virtual ~nsStandardURL();

    static void InitGlobalObjects();
    static void ShutdownGlobalObjects();

private:
    // location and length of an url segment relative to mSpec
    struct URLSegment
    {
        PRUint32 mPos;
        PRInt32  mLen;

        URLSegment() : mPos(0), mLen(-1) {}
        URLSegment(PRUint32 pos, PRInt32 len) : mPos(pos), mLen(len) {}
        void Reset() { mPos = 0; mLen = -1; }
    };

    PRInt32  Port() { return mPort == -1 ? mDefaultPort : mPort; }
    void     Clear();
    PRInt32  EscapeSegment(const char *str, const URLSegment &, PRInt16 mask, nsCString &result);
    PRBool   EscapeHost(const char *host, nsCString &result);
    PRBool   FilterString(const char *str, nsCString &result);
    void     CoalescePath(char *path);
    PRUint32 AppendSegmentToBuf(char *, PRUint32, const char *, URLSegment &, const nsCString *esc=nsnull);
    PRUint32 AppendToBuf(char *, PRUint32, const char *, PRUint32);
    nsresult BuildNormalizedSpec(const char *spec);
    PRBool   SegmentIs(const URLSegment &s1, const char *val);
    PRBool   SegmentIs(const URLSegment &s1, const char *val, const URLSegment &s2);
    PRInt32  ReplaceSegment(PRUint32 pos, PRUint32 len, const char *val, PRUint32 valLen);
    nsresult ParseURL(const char *spec);
    nsresult ParsePath(const char *spec, PRUint32 pathPos, PRInt32 pathLen = -1);

    nsresult NewSubstring(PRUint32 pos, PRInt32 len, char **result, PRBool unescaped=PR_FALSE);
    nsresult NewSubstring(const URLSegment &s, char **result, PRBool unescaped=PR_FALSE)
    {
        return NewSubstring(s.mPos, s.mLen, result, unescaped);
    }

    char    *AppendToSubstring(PRUint32 pos, PRInt32 len, const char *tail, PRInt32 tailLen = -1);

    // shift the URLSegments to the right by diff
    void     ShiftFromAuthority(PRInt32 diff) { mAuthority.mPos += diff; ShiftFromUsername(diff); }
    void     ShiftFromUsername(PRInt32 diff)  { mUsername.mPos += diff; ShiftFromPassword(diff); }
    void     ShiftFromPassword(PRInt32 diff)  { mPassword.mPos += diff; ShiftFromHostname(diff); }
    void     ShiftFromHostname(PRInt32 diff)  { mHostname.mPos += diff; ShiftFromPath(diff); }
    void     ShiftFromPath(PRInt32 diff)      { mPath.mPos += diff; ShiftFromFilePath(diff); }
    void     ShiftFromFilePath(PRInt32 diff)  { mFilePath.mPos += diff; ShiftFromDirectory(diff); }
    void     ShiftFromDirectory(PRInt32 diff) { mDirectory.mPos += diff; ShiftFromBasename(diff); }
    void     ShiftFromBasename(PRInt32 diff)  { mBasename.mPos += diff; ShiftFromExtension(diff); }
    void     ShiftFromExtension(PRInt32 diff) { mExtension.mPos += diff; ShiftFromParam(diff); }
    void     ShiftFromParam(PRInt32 diff)     { mParam.mPos += diff; ShiftFromQuery(diff); }
    void     ShiftFromQuery(PRInt32 diff)     { mQuery.mPos += diff; ShiftFromRef(diff); }
    void     ShiftFromRef(PRInt32 diff)       { mRef.mPos += diff; }

    // fastload helper functions
    nsresult ReadSegment(nsIBinaryInputStream *, URLSegment &);
    nsresult WriteSegment(nsIBinaryOutputStream *, const URLSegment &);

    // mSpec contains the normalized version of the URL spec.
    nsCString mSpec;

    PRInt32    mDefaultPort;
    PRInt32    mPort;

    // url parts (relative to mSpec)
    URLSegment mScheme;
    URLSegment mAuthority;
    URLSegment mUsername;
    URLSegment mPassword;
    URLSegment mHostname;
    URLSegment mPath;
    URLSegment mFilePath;
    URLSegment mDirectory;
    URLSegment mBasename;
    URLSegment mExtension;
    URLSegment mParam;
    URLSegment mQuery;
    URLSegment mRef;

    PRUint32                mURLType;
    nsCOMPtr<nsIURLParser>  mParser;
    nsCOMPtr<nsIFile>       mFile; // cached result for nsIFileURL::GetFile

    // global objects.  don't use COMPtr as its destructor will cause a
    // coredump if we leak it.
    static nsIURLParser *gNoAuthParser;
    static nsIURLParser *gAuthParser;
    static nsIURLParser *gStdParser;
    static PRBool        gInitialized;
};

#define NS_THIS_STANDARDURL_IMPL_CID                 \
{ /* b8e3e97b-1ccd-4b45-af5a-79596770f5d7 */         \
    0xb8e3e97b,                                      \
    0x1ccd,                                          \
    0x4b45,                                          \
    {0xaf, 0x5a, 0x79, 0x59, 0x67, 0x70, 0xf5, 0xd7} \
}

#endif // nsStandardURL_h__
