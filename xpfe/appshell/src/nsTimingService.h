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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsITimingService.h"
#include "nsHashtable.h"

#define NS_TIMINGSERVICE_CID \
{ /* ea74eba8-efb7-4ac5-a10d-25917e3d6ee4 */ \
    0xea74eba8, \
        0xefb7, \
        0x4ac5, \
        {0xa1, 0x0d, 0x25, 0x91, 0x7e, 0x3d, 0x6e, 0xe4}}

class nsTimingService : public nsITimingService
{
 public:
    nsTimingService() { NS_INIT_ISUPPORTS();};
    virtual ~nsTimingService();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSITIMINGSERVICE

protected:

    nsHashtable mTimers;
};
