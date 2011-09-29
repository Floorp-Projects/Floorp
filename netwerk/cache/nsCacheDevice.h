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
 * February 22, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gordon Sheridan, 22-February-2001
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

#ifndef _nsCacheDevice_h_
#define _nsCacheDevice_h_

#include "nspr.h"
#include "nsError.h"
#include "nsICache.h"

class nsIFile;
class nsCString;
class nsCacheEntry;
class nsICacheVisitor;
class nsIInputStream;
class nsIOutputStream;

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
    virtual nsCacheEntry * FindEntry( nsCString * key, bool *collision ) = 0;

    virtual nsresult DeactivateEntry( nsCacheEntry * entry ) = 0;
    virtual nsresult BindEntry( nsCacheEntry * entry ) = 0;
    virtual void     DoomEntry( nsCacheEntry * entry ) = 0;

    virtual nsresult OpenInputStreamForEntry(nsCacheEntry *     entry,
                                             nsCacheAccessMode  mode,
                                             PRUint32           offset,
                                             nsIInputStream **  result) = 0;

    virtual nsresult OpenOutputStreamForEntry(nsCacheEntry *     entry,
                                              nsCacheAccessMode  mode,
                                              PRUint32           offset,
                                              nsIOutputStream ** result) = 0;

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
