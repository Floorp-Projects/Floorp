/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *  
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Scott Furman, fur@netscape.com
 */

#ifndef _nsCacheEntryChannel_h_
#define _nsCacheEntryChannel_h_

#include "nsCOMPtr.h"
#include "nsITransport.h"
#include "nsCachedNetData.h"
#include "nsILoadGroup.h"

class nsIStreamListener;

// Override several nsITransport methods so that they interact with the cache manager
class nsCacheEntryTransport : public nsITransport {

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITRANSPORT

protected:
    nsCacheEntryTransport(nsCachedNetData* aCacheEntry,
                          nsITransport* aTransport,
                          nsILoadGroup* aLoadGroup);
    virtual ~nsCacheEntryTransport();

    friend class nsCachedNetData;

private:
    nsCOMPtr<nsCachedNetData>    mCacheEntry;
    nsCOMPtr<nsILoadGroup>       mLoadGroup;
    nsCOMPtr<nsITransport>       mTransport;
};

#endif // _nsCacheEntryChannel_h_
