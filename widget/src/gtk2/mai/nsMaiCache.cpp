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
    for (int index = 0; index < MAI_CACHE_SIZE; index++) {
        mCache[index].uid = 0;
        mCache[index].maiObject = NULL;
    }
}

MaiCache::~MaiCache()
{
    AtkObject *tmpAtkObj = NULL;
    for (int index = 0; index < MAI_CACHE_SIZE; index++) {
        if (mCache[index].maiObject && mCache[index].uid != 0) {
            tmpAtkObj = mCache[index].maiObject->GetAtkObject();
            MAI_LOG_DEBUG(("Mai Cache: de-caching, maiAtkObj=0x%x, ref=%d\
                            maiObj=0x%x, uid=%x\n", (guint)tmpAtkObj,
                           G_OBJECT(tmpAtkObj)->ref_count,
                           (guint)mCache[index].maiObject, mCache[index].uid));
            g_object_unref(tmpAtkObj);
            mCache[index].uid = 0;
            mCache[index].maiObject = NULL;
        }
    }
}

/* more advanced replacing algorithm can be employed for performance
 * later in MaiCache::Add
 */
gboolean
MaiCache::Add(MaiObject *aMaiObj)
{
    g_return_val_if_fail(aMaiObj != NULL, FALSE);
    // different nsIAccessible object can have the same ID,
    // but we deem them equal for accessible user.
    if (Fetch(aMaiObj)) {
        MAI_LOG_DEBUG(("Mai Cache: already in Cache: aMaiObj=0x%x, uid=%x\n",
                       (guint)aMaiObj, aMaiObj->GetNSAccessibleUniqueID()));
        return TRUE;
    }
    gint counter = 0;
    /* try to find a vacant place */
    while (counter < MAI_CACHE_SIZE) {
        counter++;
        mCacheIndex = (++mCacheIndex) % MAI_CACHE_SIZE;
        if ((mCache[mCacheIndex].maiObject == NULL) &&
            (mCache[mCacheIndex].uid == 0))
            break;
    }
    /* if fail to find a vacant place, remove the old */
    AtkObject *tmpAtkObj = NULL;
    if (counter >= MAI_CACHE_SIZE) {
        mCacheIndex = (++mCacheIndex) % MAI_CACHE_SIZE;
        tmpAtkObj = mCache[mCacheIndex].maiObject->GetAtkObject();
        MAI_LOG_DEBUG(("Mai Cache: de-caching, maiAtkObj=0x%x, ref=%d \
                        maiObj=0x%x, uid=%x\n", (guint)tmpAtkObj,
                       G_OBJECT(tmpAtkObj)->ref_count,
                       (guint)mCache[mCacheIndex].maiObject,
                       (guint)mCache[mCacheIndex].uid));
        MAI_LOG_DEBUG(("Mai Cache: added in %d, replace", mCacheIndex));
        g_object_unref(tmpAtkObj);
    }
    else
        MAI_LOG_DEBUG(("Mai Cache: added in %d, vacant", mCacheIndex));

    g_object_ref(aMaiObj->GetAtkObject());
    mCache[mCacheIndex].uid = aMaiObj->GetNSAccessibleUniqueID();
    mCache[mCacheIndex].maiObject = aMaiObj;

    MAI_LOG_DEBUG(("Mai Cache: Add in Cache, aMaiObj=0x%x, uid=%x\n",
                   (guint)aMaiObj, mCache[mCacheIndex].uid));

    return TRUE;
}

gboolean
MaiCache::Remove(MaiObject *aMaiObj)
{
    g_return_val_if_fail(aMaiObj != NULL, FALSE);
    guint uid = aMaiObj->GetNSAccessibleUniqueID();

    for (int index = 0; index < MAI_CACHE_SIZE; index++) {
        if (mCache[index].uid == uid && mCache[index].maiObject) {
            g_object_unref(mCache[index].maiObject->GetAtkObject());
            mCache[index].uid = 0;
            mCache[index].maiObject = NULL;
            return TRUE;
        }
    }
    return FALSE;
}

/* the Unique ID of nsIAccessible is the only way to judge if two MaiObject
   are equal. So the Fetch(guint uid) is the base for all the other Fetchs
*/
MaiObject *
MaiCache::Fetch(guint uid)
{
    for (int index = 0; index < MAI_CACHE_SIZE; index++) {
        if (mCache[index].uid == uid)
            return mCache[index].maiObject;
    }
    return NULL;
}

MaiObject *
MaiCache::Fetch(MaiObject *aMaiObj)
{
    return Fetch(aMaiObj->GetNSAccessibleUniqueID());
}

MaiObject *
MaiCache::Fetch(nsIAccessible *aAccess)
{
    return Fetch(GetNSAccessibleUniqueID(aAccess));
}

MaiObject *
MaiCache::Fetch(AtkObject *aAtkObj)
{
    MAI_CHECK_ATK_OBJECT_RETURN_VAL_IF_FAIL(aAtkObj, NULL);
    return Fetch(MAI_ATK_OBJECT(aAtkObj)->maiObject);
}
