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


class nsCacheEntry;


class nsCacheDevice {
public:
    nsCacheDevice(PRUint32  deviceID) : mDeviceID(deviceID){}
    virtual ~nsCacheDevice() = 0;

    //** decide on strings or ints for IDs
    virtual PRUint32 GetDeviceID(void)     { return mDeviceID; }

    virtual nsresult ActivateEntryIfFound( nsCacheEntry * entry ) = 0;
    virtual nsresult DeactivateEntry( nsCacheEntry * entry ) = 0;

    virtual nsresult BindEntry( nsCacheEntry * entry ) = 0;

    //** need to define stream factory methods

    //** need to define methods for enumerating entries

    //** need notification methods for size changes on non-streamBased entries

protected:
    PRUint32  mDeviceID;
};

#endif // _nsCacheDevice_h_
