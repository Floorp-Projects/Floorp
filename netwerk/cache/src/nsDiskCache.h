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
 * The Original Code is nsCacheDevice.h, released March 9, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Patrick Beard   <beard@netscape.com>
 *    Gordon Sheridan <gordon@netscape.com>
 */


#ifndef _nsDiskCache_h_
#define _nsDiskCache_h_

#include "nsCacheEntry.h"


class nsDiskCache {
public:
    enum {
            kCurrentVersion = 0x00010005      // format = 16 bits major version/16 bits minor version
    };

    enum { kData, kMetaData };

    static PLDHashNumber    Hash(const char* key);
    static nsresult         Truncate(PRFileDesc *  fd, PRUint32  newEOF);
};

#endif // _nsDiskCache_h_
