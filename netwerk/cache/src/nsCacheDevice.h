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
 * The Original Code is nsCacheDevice.h, released February 22, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan, 22-February-2001
 */

#ifndef _nsCacheDevice_h_
#define _nsCacheDevice_h_

#include "nspr.h"
#include "nsError.h"
#include "nsICache.h"

class nsIFile;
class nsCString;
class nsCacheEntry;
class nsICacheVisitor;
class nsITransport;

/******************************************************************************
* nsCacheDevice
*******************************************************************************/
class nsCacheDevice {
public:
    nsCacheDevice() { MOZ_COUNT_CTOR(nsCacheDevice); }
    virtual ~nsCacheDevice() { MOZ_COUNT_DTOR(nsCacheDevice); }

    virtual nsresult Init() = 0;
    virtual nsresult Shutdown() = 0;

    virtual const char *   GetDeviceID(void) = 0;
    virtual nsCacheEntry * FindEntry( nsCString * key ) = 0;

    virtual nsresult DeactivateEntry( nsCacheEntry * entry ) = 0;
    virtual nsresult BindEntry( nsCacheEntry * entry ) = 0;
    virtual void     DoomEntry( nsCacheEntry * entry ) = 0;

    virtual nsresult GetTransportForEntry( nsCacheEntry * entry,
                                           nsCacheAccessMode mode,
                                           nsITransport **result ) = 0;

    virtual nsresult GetFileForEntry( nsCacheEntry *    entry,
                                      nsIFile **        result ) = 0;

    virtual nsresult OnDataSizeChange( nsCacheEntry * entry, PRInt32 deltaSize ) = 0;

    virtual nsresult Visit(nsICacheVisitor * visitor) = 0;

    /**
     * Device must evict entries associated with clientID.  If clientID == nsnull, all
     * entries must be evicted.  Active entries must be doomed, rather than evicted.
     */
    virtual nsresult EvictEntries(const char * clientID) = 0;
};

#endif // _nsCacheDevice_h_
