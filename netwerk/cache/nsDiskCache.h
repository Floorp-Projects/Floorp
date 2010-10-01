/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is nsCacheDevice.h, released
 * March 9, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Beard   <beard@netscape.com>
 *   Gordon Sheridan <gordon@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef _nsDiskCache_h_
#define _nsDiskCache_h_

#include "nsCacheEntry.h"

#ifdef XP_WIN
#include <winsock.h>  // for htonl/ntohl
#endif


class nsDiskCache {
public:
    enum {
            kCurrentVersion = 0x0001000F      // format = 16 bits major version/16 bits minor version
    };

    enum { kData, kMetaData };

    // Parameter initval initializes internal state of hash function. Hash values are different
    // for the same text when different initval is used. It can be any random number.
    // 
    // It can be used for generating 64-bit hash value:
    //   (PRUint64(Hash(key, initval1)) << 32) | Hash(key, initval2)
    //
    // It can be also used to hash multiple strings:
    //   h = Hash(string1, 0);
    //   h = Hash(string2, h);
    //   ... 
    static PLDHashNumber    Hash(const char* key, PLDHashNumber initval=0);
    static nsresult         Truncate(PRFileDesc *  fd, PRUint32  newEOF);
};

#endif // _nsDiskCache_h_
