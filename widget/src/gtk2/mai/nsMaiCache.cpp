/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2002 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Bolian Yin (bolian.yin@sun.com)
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMaiCache.h"

MaiCache::MaiCache()
{
    /* cache index always point to the last item */
    mCacheIndex = -1;
    for (int index = 0; index < MAI_CACHE_SIZE; index++)
        mCache[index]= 0;
}

MaiCache::~MaiCache()
{
    MaiObject *maiObj = NULL;
    for (int index = 0; index < MAI_CACHE_SIZE; index++) {
        if ((maiObj = MaiHashTable::Lookup(mCache[index]))) {
            MAI_LOG_DEBUG(("Mai Cache: de-caching, uid=0x%x, maiObj=0x%x \n",
                           mCache[index], maiObj));
            mCache[index] = 0;
            g_object_unref(maiObj->GetAtkObject());
        }
    }
}

PRBool
MaiCache::Add(MaiObject *aMaiObj)
{
    g_return_val_if_fail(aMaiObj != NULL, PR_FALSE);

    guint uid = aMaiObj->GetNSAccessibleUniqueID();
    NS_ASSERTION((uid > 0), "Invalid nsAccessible ID");
    if (uid < 0)
        return PR_FALSE;

    gint counter = 0;
    /* if it has been in cache */
    while (counter < MAI_CACHE_SIZE) {
        ++counter;
        mCacheIndex = (++mCacheIndex) % MAI_CACHE_SIZE;
        if ((mCache[mCacheIndex] == uid))
            return PR_TRUE;
    }

    /* try to find a vacant place */
    counter = 0;
    while (counter < MAI_CACHE_SIZE) {
        ++counter;
        mCacheIndex = (++mCacheIndex) % MAI_CACHE_SIZE;
        if ((mCache[mCacheIndex] == 0))
            break;
    }
    /* if fail to find a vacant place, remove an old one*/
    if (counter >= MAI_CACHE_SIZE) {
        mCacheIndex = (++mCacheIndex) % MAI_CACHE_SIZE;
        MaiObject *tmpMaiObj = MaiHashTable::Lookup(mCache[mCacheIndex]);
        NS_ASSERTION(tmpMaiObj, "Fail to lookup from hash table");

        MAI_LOG_DEBUG(("Mai Cache: de-caching, uid=0x%x, maiObj=0x%x \n",
                       mCache[mCacheIndex], tmpMaiObj));
        MAI_LOG_DEBUG(("Mai Cache: added in %d, replace", mCacheIndex));
        g_object_unref(tmpMaiObj->GetAtkObject());
    }
    else
        MAI_LOG_DEBUG(("Mai Cache: added in %d, vacant", mCacheIndex));

    g_object_ref(aMaiObj->GetAtkObject());
    mCache[mCacheIndex] = uid;

    MAI_LOG_DEBUG(("Mai Cache: Add in Cache, aMaiObj=0x%x, uid=%x\n",
                   (guint)aMaiObj, mCache[mCacheIndex]));

    return PR_TRUE;
}

/**************************************
  nsMaiHashTable
*************************************/

PLHashTable *MaiHashTable::mMaiObjectHashTable = NULL;
PRBool MaiHashTable::mInitialized = PR_FALSE;

static PLHashNumber IntHashKey(PRInt32 key);

#ifdef MAI_LOGGING
static PRIntn printHashEntry(PLHashEntry *he, PRIntn index, void *arg);
#endif

PRBool
MaiHashTable::Init()
{
    if (!mInitialized) {
        mMaiObjectHashTable = PL_NewHashTable(0,
                                              (PLHashFunction)IntHashKey,
                                              PL_CompareValues,
                                              PL_CompareValues,
                                              0, 0);
        NS_ASSERTION(mMaiObjectHashTable, "Fail to create Hash Table");
        mInitialized = PR_TRUE;
    }
    return mInitialized;
}

void
MaiHashTable::Destroy()
{
    if (mInitialized && mMaiObjectHashTable) {

        mInitialized = PR_FALSE;

#ifdef MAI_LOGGING
        MAI_LOG_DEBUG(("Destroying hash table, but some objs are in it:\n"));
        gint count = PL_HashTableEnumerateEntries(mMaiObjectHashTable,
                                                  printHashEntry,
                                                  (void*)NULL);
        MAI_LOG_DEBUG(("Total %d entries still in the hash table\n", count));
#endif /* #ifdef MAI_LOGGING */

        PL_HashTableDestroy(mMaiObjectHashTable);
        mMaiObjectHashTable = nsnull;
    }
}

PRBool
MaiHashTable::Add(MaiObject *aMaiObject)
{
    if (!mInitialized)
        return FALSE;

    guint uid = aMaiObject->GetNSAccessibleUniqueID();
    PLHashEntry *newEntry = PL_HashTableAdd(mMaiObjectHashTable,
                                            GINT_TO_POINTER(uid),
                                            GINT_TO_POINTER(aMaiObject));
    MAI_LOG_DEBUG(("--Add in hash table uid=0x%x, obj=0x%x\n",
                   uid, (guint)aMaiObject));
    return newEntry ? PR_TRUE : PR_FALSE;
}

PRBool
MaiHashTable::Remove(MaiObject *aMaiObject)
{
    if (!mInitialized)
        return FALSE;

    guint uid = aMaiObject->GetNSAccessibleUniqueID();
    MAI_LOG_DEBUG(("--Remove in hash table uid=0x%x, obj=0x%x\n",
                   uid, (guint)aMaiObject));
    return PL_HashTableRemove(mMaiObjectHashTable, GINT_TO_POINTER(uid));
}

MaiObject *
MaiHashTable::Lookup(guint uid)
{
    if (!mInitialized)
        return FALSE;

    return NS_REINTERPRET_CAST(MaiObject*,
                               PL_HashTableLookup(mMaiObjectHashTable,
                                                  GINT_TO_POINTER(uid)));
}

MaiObject *
MaiHashTable::Lookup(nsIAccessible *aAcc)
{
    return Lookup(::GetNSAccessibleUniqueID(aAcc));
}

PLHashNumber
IntHashKey(PRInt32 key)
{
    return PLHashNumber(key);
}

#ifdef MAI_LOGGING
PRIntn
printHashEntry(PLHashEntry *he, PRIntn index, void *arg)
{
    if (he) {
        MAI_LOG_DEBUG(("         Entry: uid=0x%x, maiObj=0x%x\n",
                       (guint)he->key, (guint)he->value));
    }
    return HT_ENUMERATE_NEXT;
}
#endif /* #ifdef MAI_LOGGING */
