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

#include "nsIAccessible.h"
#include "nsCOMPtr.h"

#include "nsMaiAppRoot.h"

MaiAppRoot::MaiAppRoot()
{
    /* for mai root, mAccessible is always NULL */
    mTopLevelList = NULL;
    mMaiCache = new MaiCache();
}

MaiAppRoot::~MaiAppRoot()
{
    MAI_LOG_DEBUG(("Accessibility Shut down\n"));

    if (mTopLevelList) {
        GList *tmp_list1, *tmp_list2;
        MaiTopLevel *maiTopLevel;

        tmp_list1 = tmp_list2 = mTopLevelList;
        mTopLevelList = NULL;

        while (tmp_list1) {
            maiTopLevel = ((TopLevelItem*)tmp_list1->data)->maiTopLevel;
            tmp_list1 = tmp_list1->next;

            g_object_unref(maiTopLevel->GetAtkObject());
        }
        g_list_free(tmp_list2);
    }
    if (mMaiCache)
        delete mMaiCache;
}

#ifdef MAI_LOGGING
void
MaiAppRoot::DumpMaiObjectInfo(int aDepth)
{
    --aDepth;
    if (aDepth < 0)
        return;
    g_print("<<<<<Begin of MaiObject: this=0x%x, aDepth=%d, type=%s\n",
            (unsigned int)this, aDepth, "MaiAppRoot");

    gint nChild = GetChildCount();
    g_print("== %d toplevel children\n", nChild);

    MaiObject *maiChild;
    for (int childIndex = 0; childIndex < nChild; childIndex++) {
        maiChild = RefChild(childIndex);
        if (maiChild) {
            maiChild->DumpMaiObjectInfo(aDepth);
        }
    }
    g_print(">>>>>End of MaiObject: this=0x%x, type=%s\n",
            (unsigned int)this, "MaiAppRoot");
}
#endif

guint
MaiAppRoot::GetNSAccessibleUniqueID()
{
    // there is no nsIAccessible for Root, return a fake Id
    // Note: 0 is invalid for UniqueID
    return 1;
}

gboolean
MaiAppRoot::AddMaiTopLevel(MaiTopLevel *aTopLevel)
{
    g_return_val_if_fail(aTopLevel != NULL, FALSE);

    /* if the nsIAccessible with the same UniqueID is already added,
     * we only add atk object ref of the one in list, not really
     * add the new mai object. They are think to be same (??)
     */

    TopLevelItem *item = FindTopLevelItem(aTopLevel->GetNSAccessible());

    if (!item) {
        item = new TopLevelItem();
        item->ref = 1;
        item->maiTopLevel = aTopLevel;

        mTopLevelList = g_list_append(mTopLevelList, item);
        AtkObject *atkObject = aTopLevel->GetAtkObject();
        if (atkObject) {
            atk_object_set_parent(atkObject, GetAtkObject());
            g_object_ref(atkObject);
        }
    }
    item->ref++;
    return TRUE;
}

gboolean
MaiAppRoot::RemoveMaiTopLevel(MaiTopLevel *aTopLevel)
{
    g_return_val_if_fail(aTopLevel != NULL, TRUE);
    TopLevelItem *item = FindTopLevelItem(aTopLevel->GetNSAccessible());
    if (!item)
        return FALSE;
    item->ref--;
    if (item->ref == 0) {
        mTopLevelList = g_list_remove(mTopLevelList, item);
        g_object_unref(item->maiTopLevel->GetAtkObject());
        delete item;
    }
    return TRUE;
}

MaiTopLevel *
MaiAppRoot::FindMaiTopLevel(MaiTopLevel *aTopLevel)
{
    g_return_val_if_fail(aTopLevel != NULL, FALSE);
    return FindMaiTopLevel(aTopLevel->GetNSAccessible());
}

MaiTopLevel *
MaiAppRoot::FindMaiTopLevel(nsIAccessible *aTopLevel)
{
    g_return_val_if_fail(aTopLevel != NULL, NULL);
    TopLevelItem *item = FindTopLevelItem(aTopLevel);

    if (!item)
        return NULL;

    return item->maiTopLevel;
}

TopLevelItem *
MaiAppRoot::FindTopLevelItem(nsIAccessible *aTopLevel)
{
    g_return_val_if_fail(aTopLevel != NULL, NULL);

    /* use Unique ID of nsIAccessible to judge equal */
    if (mTopLevelList) {
        GList *tmp_list;
        TopLevelItem *item;

        tmp_list = mTopLevelList;

        while (tmp_list) {
            item = (TopLevelItem*)tmp_list->data;
            tmp_list = tmp_list->next;
            if (item->maiTopLevel->GetNSAccessibleUniqueID() ==
                ::GetNSAccessibleUniqueID(aTopLevel))
                return item;
        }
    }
    return NULL;
}

MaiCache *
MaiAppRoot::GetCache(void)
{
    // return "NULL" will disable the caching.
    return mMaiCache;
}

/* virtual functions */

AtkObject *
MaiAppRoot::GetAtkObject(void)
{
    // When the maiAppRoot created, no nsIAccessible exist yet,
    // just create the root atk object
    if (!mMaiAtkObject) {
        mMaiAtkObject = (MaiAtkObject*) g_object_new(MAI_TYPE_APP_ROOT,NULL);
        g_return_val_if_fail(mMaiAtkObject != NULL, NULL);

        ATK_OBJECT(mMaiAtkObject)->role = ATK_ROLE_INVALID;
        ATK_OBJECT(mMaiAtkObject)->layer = ATK_LAYER_INVALID;

        atk_object_initialize(ATK_OBJECT(mMaiAtkObject), this);
    }
    return ATK_OBJECT(mMaiAtkObject);
}

void
MaiAppRoot::Initialize(void)
{
}

void
MaiAppRoot::Finalize(void)
{
    MaiObject::Finalize();
}

gchar *
MaiAppRoot::GetName(void)
{
    AtkObject *atkObject = (AtkObject*)mMaiAtkObject;

    if (!atkObject->name) {
        atk_object_set_name(atkObject, "MAI Root");
    }

#ifdef MAI_LOGGING
    //    DumpMaiObjectInfo(3);
#endif

    return atkObject->name;
}

gchar *
MaiAppRoot::GetDescription(void)
{
    AtkObject *atkObject = (AtkObject*)mMaiAtkObject;

    if (!atkObject->description) {
        atk_object_set_description(atkObject, "Mozilla Root Accessible");
    }
    return atkObject->description;
}

MaiObject *
MaiAppRoot::GetParent(void)
{
    return NULL;
}

gint
MaiAppRoot::GetChildCount(void)
{
    return g_list_length(mTopLevelList);
}

MaiObject *
MaiAppRoot::RefChild(gint aChildIndex)
{
    gint count = GetChildCount();
    if ((aChildIndex < 0) || (aChildIndex >= count)) {
        return NULL;
    }
    else {
        TopLevelItem *item = (TopLevelItem *)
            g_list_nth_data(mTopLevelList, aChildIndex);
        return item->maiTopLevel;
    }
}
