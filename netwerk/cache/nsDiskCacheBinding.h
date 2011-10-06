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
 * The Original Code is nsDiskCacheBinding.h, released
 * May 10, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gordon Sheridan  <gordon@netscape.com>
 *   Patrick C. Beard <beard@netscape.com>
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


#ifndef _nsDiskCacheBinding_h_
#define _nsDiskCacheBinding_h_

#include "nspr.h"
#include "pldhash.h"

#include "nsISupports.h"
#include "nsCacheEntry.h"

#include "nsDiskCacheMap.h"
#include "nsDiskCacheStreams.h"


/******************************************************************************
 *  nsDiskCacheBinding
 *
 *  Created for disk cache specific data and stored in nsCacheEntry.mData as
 *  an nsISupports.  Also stored in nsDiskCacheHashTable, with collisions
 *  linked by the PRCList.
 *
 *****************************************************************************/

class nsDiskCacheDeviceDeactivateEntryEvent;

class nsDiskCacheBinding : public nsISupports, public PRCList {
public:
    NS_DECL_ISUPPORTS

    nsDiskCacheBinding(nsCacheEntry* entry, nsDiskCacheRecord * record);
    virtual ~nsDiskCacheBinding();

    nsresult EnsureStreamIO();
    bool     IsActive() { return mCacheEntry != nsnull;}

// XXX make friends
public:
    nsCacheEntry*           mCacheEntry;    // back pointer to parent nsCacheEntry
    nsDiskCacheRecord       mRecord;
    nsDiskCacheStreamIO*    mStreamIO;      // strong reference
    bool                    mDoomed;        // record is not stored in cache map
    PRUint8                 mGeneration;    // possibly just reservation

    // If set, points to a pending event which will deactivate |mCacheEntry|.
    // If not set then either |mCacheEntry| is not deactivated, or it has been
    // deactivated but the device returned it from FindEntry() before the event
    // fired. In both two latter cases this binding is to be considered valid.
    nsDiskCacheDeviceDeactivateEntryEvent *mDeactivateEvent;
};


/******************************************************************************
 *  Utility Functions
 *****************************************************************************/

nsDiskCacheBinding *   GetCacheEntryBinding(nsCacheEntry * entry);



/******************************************************************************
 *  nsDiskCacheBindery
 *
 *  Used to keep track of nsDiskCacheBinding associated with active/bound (and
 *  possibly doomed) entries.  Lookups on 4 byte disk hash to find collisions
 *  (which need to be doomed, instead of just evicted.  Collisions are linked
 *  using a PRCList to keep track of current generation number.
 *
 *  Used to detect hash number collisions, and find available generation numbers.
 *
 *  Not all nsDiskCacheBinding have a generation number.
 *
 *  Generation numbers may be aquired late, or lost (when data fits in block file)
 *
 *  Collisions can occur:
 *      BindEntry()       - hashnumbers collide (possibly different keys)
 *
 *  Generation number required:
 *      DeactivateEntry() - metadata written to disk, may require file
 *      GetFileForEntry() - force data to require file
 *      writing to stream - data size may require file
 *
 *  Binding can be kept in PRCList in order of generation numbers.
 *  Binding with no generation number can be Appended to PRCList (last).
 *
 *****************************************************************************/

class nsDiskCacheBindery {
public:
    nsDiskCacheBindery();
    ~nsDiskCacheBindery();

    nsresult                Init();
    void                    Reset();

    nsDiskCacheBinding *    CreateBinding(nsCacheEntry *       entry,
                                          nsDiskCacheRecord *  record);

    nsDiskCacheBinding *    FindActiveBinding(PRUint32  hashNumber);
    void                    RemoveBinding(nsDiskCacheBinding * binding);
    bool                    ActiveBindings();
    
private:
    nsresult                AddBinding(nsDiskCacheBinding * binding);

    // member variables
    static PLDHashTableOps ops;
    PLDHashTable           table;
    bool                   initialized;
};

#endif /* _nsDiskCacheBinding_h_ */
