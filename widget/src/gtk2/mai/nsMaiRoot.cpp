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

#include "nsMaiRoot.h"

MaiRoot::MaiRoot()
{
    /* for mai root, mAccessible is always NULL */
    mMaiTopLevelList = NULL;

#ifdef  DEBUG_MAI
    g_print("real type created: MaiRoot\n");
#endif
}

MaiRoot::~MaiRoot()
{
    if (mMaiTopLevelList) {
        GList *tmp_list1, *tmp_list2;
        MaiObject * maiObject;

        tmp_list1 = tmp_list2 = mMaiTopLevelList;
        mMaiTopLevelList = NULL;

        while (tmp_list1) {
            maiObject = (MaiObject*)tmp_list1->data;
            tmp_list1 = tmp_list1->next;

            g_object_unref(ATK_OBJECT(maiObject->GetAtkObject()));
        }
        g_list_free(tmp_list2);

    }
#ifdef  DEBUG_MAI
    g_print("real type deleted: MaiRoot\n");
#endif
}

gboolean
MaiRoot::AddTopLevelAccessible(nsIAccessible *toplevel)
{
    g_return_val_if_fail(toplevel != NULL, PR_FALSE);

    MaiTopLevel *mai_top_level;
  
    mai_top_level = new MaiTopLevel(toplevel);
    g_return_val_if_fail(mai_top_level != NULL, PR_FALSE); 

    mMaiTopLevelList = g_list_append(mMaiTopLevelList, mai_top_level);

    return PR_TRUE;
}

gboolean
MaiRoot::RemoveTopLevelAccessible(nsIAccessible *toplevel)
{
    g_return_val_if_fail(toplevel != NULL, PR_TRUE);
  
    MaiObject *maiObject = nsnull;
    GList *list = mMaiTopLevelList;
    while (list) {
        maiObject = (MaiObject*)(list->data);
        if (maiObject->GetNSAccessible() == toplevel)
            break;
        list = list->next;
    }
    if (list) {
        mMaiTopLevelList = g_list_remove(mMaiTopLevelList, maiObject);
        g_object_unref(ATK_OBJECT(maiObject->GetAtkObject()));
        return PR_TRUE;
    }
    return PR_FALSE;
}


/* virtual functions */

AtkObject *
MaiRoot::GetAtkObject(void)
{
    // When the maiRoot created, no nsIAccessible exist yet,
    // just create the root atk object
    if (!mMaiAtkObject) {
        mMaiAtkObject = (MaiAtkObject*) g_object_new(MAI_TYPE_ROOT,NULL);
        g_return_val_if_fail(mMaiAtkObject != NULL, NULL);

        ATK_OBJECT(mMaiAtkObject)->role = ATK_ROLE_INVALID;
        ATK_OBJECT(mMaiAtkObject)->layer = ATK_LAYER_INVALID;

        atk_object_initialize(ATK_OBJECT(mMaiAtkObject), this);

        atk_object_set_name(ATK_OBJECT(mMaiAtkObject),
                            "Mozilla Accessible Root");
    }
    return ATK_OBJECT(mMaiAtkObject);
}

void
MaiRoot::Initialize(void)
{
    g_return_if_fail(mMaiAtkObject != NULL);
    g_return_if_fail(mAccessible == NULL);

    atk_object_set_name(ATK_OBJECT(mMaiAtkObject), "Mozilla Root Accessibile");
    atk_object_set_description(ATK_OBJECT(mMaiAtkObject),
                               "the first accessible, and parent of all others");
}

void
MaiRoot::Finalize(void)
{
    MaiObject::Finalize();
}
