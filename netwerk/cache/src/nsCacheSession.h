/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is nsCacheSession.h, released February 23, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan <gordon@netscape.com>
 *    Patrick Beard   <beard@netscape.com>
 *    Darin Fisher    <darin@netscape.com>
 */

#ifndef _nsCacheSession_h_
#define _nsCacheSession_h_

#include "nspr.h"
#include "nsError.h"
#include "nsICacheSession.h"
#include "nsString.h"

class nsCacheSession : public nsICacheSession
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHESESSION
    
    nsCacheSession(const char * clientID, nsCacheStoragePolicy storagePolicy, PRBool streamBased);
    virtual ~nsCacheSession();
    
    nsCString *           ClientID()      { return &mClientID; }

    enum SessionInfo {
        eStoragePolicyMask        = 0x000000FF,
        eStreamBasedMask          = 0x00000100,
        eDoomEntriesIfExpiredMask = 0x00001000
    };

    void   MarkStreamBased()  { mInfo |=  eStreamBasedMask; }
    void   ClearStreamBased() { mInfo &= ~eStreamBasedMask; }
    PRBool IsStreamBased()    { return (mInfo & eStreamBasedMask) != 0; }

    void   MarkDoomEntriesIfExpired()  { mInfo |=  eDoomEntriesIfExpiredMask; }
    void   ClearDoomEntriesIfExpired() { mInfo &= ~eDoomEntriesIfExpiredMask; }
    PRBool WillDoomEntriesIfExpired()  { return (mInfo & eDoomEntriesIfExpiredMask); }

    nsCacheStoragePolicy  StoragePolicy()
    {
        return (nsCacheStoragePolicy)(mInfo & eStoragePolicyMask);
    }

    void SetStoragePolicy(nsCacheStoragePolicy policy)
    {
        NS_ASSERTION(policy <= 0xFF, "too many bits in nsCacheStoragePolicy");
        mInfo &= ~eStoragePolicyMask; // clear storage policy bits
        mInfo |= policy;
    }

private:
    nsCString               mClientID;
    PRUint32                mInfo;
};

#endif // _nsCacheSession_h_
