/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsIBrowsingProfile_h__
#define nsIBrowsingProfile_h__

#include "nsISupports.h"

struct nsBrowsingProfileCategoryDescriptor {
  PRUint16              mID;
  PRUint8               mVisitCount;
  PRUint8               mFlags;
};

// setting this to 56 brings the size of an nsBrowsingProfileVector
// struct up to 256 bytes
#define nsBrowsingProfile_CategoryCount 56

struct nsBrowsingProfileVector {
    union {
        PRUint8         mPadding[32];
        struct {
            PRUint32    mCheck;         // should always == nsBrowsingProfile_Check
            PRUint16    mMajorVersion;  // denotes a non-backward compatible change (e.g. structural change to this vector)
            PRUint16    mMinorVersion;  // denotes a backward compatible change to the meaning of this vector
        }               mInfo;
    }                   mHeader;
    nsBrowsingProfileCategoryDescriptor
                        mCategory[nsBrowsingProfile_CategoryCount];
};

#define nsBrowsingProfile_Check                 0xbaadf00d
#define nsBrowsingProfile_CurrentMajorVersion   1
#define nsBrowsingProfile_CurrentMinorVersion   0

enum nsBrowsingProfileFlag {
    nsBrowsingProfileFlag_ThisSession   = (1 << 0),
    nsBrowsingProfileFlag_Recent        = (1 << 1)
};

////////////////////////////////////////////////////////////////////////////////

// size of hex encoding:
const PRUint32 kBrowsingProfileCookieSize = sizeof(nsBrowsingProfileVector) * 2;

#define NS_IBROWSINGPROFILE_IID                      \
{ /* 4aef9ba2-e0af-11d2-8cca-0060b0fc14a3 */         \
    0x4aef9ba2,                                      \
    0xe0af,                                          \
    0x11d2,                                          \
    {0x8c, 0xca, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

class nsIBrowsingProfile : public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IBROWSINGPROFILE_IID; return iid; }
    
    NS_IMETHOD Init(const char* userProfileName) = 0;

    NS_IMETHOD GetVector(nsBrowsingProfileVector& result) = 0;

    NS_IMETHOD SetVector(nsBrowsingProfileVector& value) = 0;

    NS_IMETHOD GetCookieString(char buf[kBrowsingProfileCookieSize]) = 0;

    NS_IMETHOD SetCookieString(char buf[kBrowsingProfileCookieSize]) = 0;

    NS_IMETHOD GetDescription(char* *htmlResult) = 0;

    NS_IMETHOD CountPageVisit(const char* url) = 0;
};

// for component registration
#define NS_BROWSINGPROFILE_CID                       \
{ /* 7ef80dd0-e0af-11d2-8cca-0060b0fc14a3 */         \
    0x7ef80dd0,                                      \
    0xe0af,                                          \
    0x11d2,                                          \
    {0x8c, 0xca, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

////////////////////////////////////////////////////////////////////////////////

extern nsresult
NS_NewBrowsingProfile(const char* userProfileName,
                      nsIBrowsingProfile* *result);

#endif // nsIBrowsingProfile_h__
