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
#include "nsMaiCache.h"
#include "nsMaiAppRoot.h"

MaiAppRoot::MaiAppRoot()
{
    /* for mai root, mAccessible is always NULL */
    mTopLevelList = NULL;
}

MaiAppRoot::~MaiAppRoot()
{
    MAI_LOG_DEBUG(("Accessibility Shut down\n"));

    if (mTopLevelList) {
        GList *tmp_list1, *tmp_list2;
        MaiTopLevel *maiTopLevel;
        guint uid;

        tmp_list1 = tmp_list2 = mTopLevelList;
        mTopLevelList = NULL;

#ifdef MAI_LOGGING
        while (tmp_list1) {
            uid = (guint)tmp_list1->data;
            maiTopLevel = (MaiTopLevel *) MaiHashTable::Lookup(uid);
            if (maiTopLevel)
                MAI_LOG_DEBUG(("##toplevel=0x%x (uid=0x%x) still exist !\n",
                               (guint)maiTopLevel, uid));
            tmp_list1 = tmp_list1->next;
        }
#endif /* #ifdef MAI_LOGGING */

        g_list_free(tmp_list2);
    }
}

#ifdef MAI_LOGGING
void
MaiAppRoot::DumpMaiObjectInfo(int aDepth)
{
    --aDepth;
    if (aDepth < 0)
        return;
    g_print("MaiAppRoot: this=0x%x, aDepth=%d, type=%s\n",
            (unsigned int)this, aDepth, "MaiAppRoot");
    gint nChild = GetChildCount();
    g_print("#child=%d<br>\n", nChild);
    g_print("Iface num: 1=component, 2=action, 3=value, 4=editabletext,"
            "5=hyperlink, 6=hypertext, 7=selection, 8=table, 9=text\n");
    g_print("<ul>\n");

    MaiObject *maiChild;
    for (int childIndex = 0; childIndex < nChild; childIndex++) {
        maiChild = RefChild(childIndex);
        if (maiChild) {
            g_print("  <li>");
            maiChild->DumpMaiObjectInfo(aDepth);
        }
    }
    g_print("</ul>\n");
    g_print("End of MaiAppRoot: this=0x%x, type=%s\n<br>",
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

PRBool
MaiAppRoot::AddMaiTopLevel(MaiTopLevel *aTopLevel)
{
    g_return_val_if_fail(aTopLevel != NULL, FALSE);
    MAI_LOG_DEBUG(("MaiAppRoot: add MaiTopLevel = 0x%x", (guint)aTopLevel));

    guint uid = aTopLevel->GetNSAccessibleUniqueID();
    g_object_ref(aTopLevel->GetAtkObject());
    if (!LookupTopLevelID(uid)) {
        mTopLevelList = g_list_append(mTopLevelList, GINT_TO_POINTER(uid));
        atk_object_set_parent(aTopLevel->GetAtkObject(), GetAtkObject());
    }
    return TRUE;
}

PRBool
MaiAppRoot::RemoveMaiTopLevelByID(guint aID)
{
    if (!LookupTopLevelID(aID))
        return PR_FALSE;

    MaiTopLevel *toplevel = (MaiTopLevel *)MaiHashTable::Lookup(aID);
    NS_ASSERTION(toplevel, "Fail to get object from hash table");
    MAI_LOG_DEBUG(("MaiAppRoot: remove MaiTopLevel = 0x%x", (guint)toplevel));

    if (!toplevel)
        return PR_FALSE;

    g_object_unref(toplevel->GetAtkObject());
    return TRUE;
}

PRBool
MaiAppRoot::LookupTopLevelID(guint aID)
{
    if (mTopLevelList) {
        GList *tmp_list = mTopLevelList;
        while (tmp_list) {
            if (aID == (guint)(tmp_list->data))
                return PR_TRUE;
            tmp_list = tmp_list->next;
        }
    }
    return PR_FALSE;
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
        guint uid = GPOINTER_TO_UINT(g_list_nth_data(mTopLevelList,
                                                     aChildIndex));
        MaiTopLevel *maiTopLevel = (MaiTopLevel*)MaiHashTable::Lookup(uid);
        NS_ASSERTION(maiTopLevel, "Fail to get object from hash table");
        if (maiTopLevel)
            g_object_ref(maiTopLevel->GetAtkObject());
        return maiTopLevel;
    }
}
