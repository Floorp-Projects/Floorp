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

#include "nsCOMPtr.h"
#include "nsIAccessibleAction.h"

#include "nsMaiWidget.h"
#include "nsMaiAppRoot.h"
#include "nsMaiInterfaceComponent.h"
#include "nsMaiUtil.h"

GType
mai_atk_widget_get_type(void)
{
    static GType type = 0;

    if (!type) {
        static const GTypeInfo tinfo = {
            sizeof(MaiAtkWidgetClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) NULL,
            (GClassFinalizeFunc) NULL,
            NULL, /* class data */
            sizeof(MaiAtkWidget), /* instance size */
            0, /* nb preallocs */
            (GInstanceInitFunc) NULL,
            NULL /* value table */
        };

        type = g_type_register_static(MAI_TYPE_ATK_OBJECT,
                                      "MaiAtkWidget", &tinfo, GTypeFlags(0));
    }
    return type;
}

gulong MaiWidget::mAtkTypeNameIndex = 0;

MaiWidget::MaiWidget(nsIAccessible *aAcc): MaiObject(aAcc)
{
    mMaiInterfaceCount = 0;
    for (int index = 0; index < MAI_INTERFACE_NUM; index++) {
        mMaiInterface[index] = NULL;
    }
    mChildren = g_hash_table_new(g_direct_hash, NULL);
}

MaiWidget::~MaiWidget()
{
    for (int index = 0; index < MAI_INTERFACE_NUM; index++) {
        if (mMaiInterface[index])
            delete mMaiInterface[index];
    }
    g_hash_table_destroy(mChildren);
}

#ifdef MAI_LOGGING
void
MaiWidget::DumpMaiObjectInfo(gint aDepth)
{
    --aDepth;
    if (aDepth < 0)
        return;
    g_print("<<<<<Begin of MaiObject: this=0x%x, aDepth=%d, type=%s\n",
            (unsigned int)this, aDepth, "MaiWidget");

    g_print("NSAccessible UniqueID=%x\n", GetNSAccessibleUniqueID());
    g_print("Interfaces Supported:");
    for (int ifaces = 0; ifaces < MAI_INTERFACE_NUM; ifaces++) {
        if (mMaiInterface[ifaces])
            g_print(" : %d", ifaces);
    }
    g_print("\n");
    gint nChild = GetChildCount();
    gint nIndexInParent = GetIndexInParent();
    g_print("== Name: %s, IndexInParent=%d, ChildNum=%d \n",
            GetName(),nIndexInParent, nChild);

    MaiObject *maiChild;
    for (int index = 0; index < nChild; index++) {
        maiChild = RefChild(index);
        if (maiChild) {
            maiChild->DumpMaiObjectInfo(aDepth);
        }
    }
    g_print(">>>>>End of MaiObject: this=0x%x, type=%s\n",
            (unsigned int)this, "MaiWidget");

    //the interface info
}
#endif

guint
MaiWidget::GetNSAccessibleUniqueID()
{
    return ::GetNSAccessibleUniqueID(mAccessible);
}

MaiInterface *
MaiWidget::GetMaiInterface(MaiInterfaceType aIfaceType)
{
    return mMaiInterface[aIfaceType];
}

GType
MaiWidget::GetMaiAtkType(void)
{
    GType type;
    static const GTypeInfo tinfo = {
        sizeof(MaiAtkWidgetClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) NULL,
        (GClassFinalizeFunc) NULL,
        NULL, /* class data */
        sizeof(MaiAtkWidget), /* instance size */
        0, /* nb preallocs */
        (GInstanceInitFunc) NULL,
        NULL /* value table */
    };

    type = g_type_register_static(MAI_TYPE_ATK_WIDGET,
                                  MaiWidget::GetUniqueMaiAtkTypeName(),
                                  &tinfo, GTypeFlags(0));

    if (mMaiInterfaceCount == 0)
        return MAI_TYPE_ATK_WIDGET;

    for (int index = 0; index < MAI_INTERFACE_NUM; index++) {
        if (!mMaiInterface[index])
            continue;
        g_type_add_interface_static(type,
                                    mMaiInterface[index]->GetAtkType(),
                                    mMaiInterface[index]->GetInterfaceInfo());
    }
    return type;
}

void
MaiWidget::AddMaiInterface(MaiInterface *aMaiIface)
{
    g_return_if_fail(aMaiIface != NULL);
    MaiInterfaceType aMaiIfaceType = aMaiIface->GetType();

    // if same type of If has been added, release previous one
    if (mMaiInterface[aMaiIfaceType]) {
        delete mMaiInterface[aMaiIfaceType];
    }
    mMaiInterface[aMaiIfaceType] = aMaiIface;
    mMaiInterfaceCount++;
}

void
MaiWidget::CreateMaiInterfaces(void)
{
    g_return_if_fail(mAccessible != NULL);

    // the Component interface are supported by all nsIAccessible

    // Add Interfaces for each nsIAccessible.ext interfaces
    MaiInterfaceComponent *maiInterfaceComponent =
        new MaiInterfaceComponent(this);
    AddMaiInterface(maiInterfaceComponent);

    /*
    nsCOMPtr<nsIAccessibleAction>
        accessInterfaceAction(do_QueryInterface(mAccessible));
    if (accessInterfaceAction) {
        MaiInterfaceAction *maiInterfaceAction = new MaiInterfaceAction(this);
        AddMaiInterface(maiInterfaceAction);
    }
    */

    /*
    // all the other interfaces follow here
    nsIAccessibleAction.idl
    nsIAccessibleEditableText.idl
    nsIAccessibleHyperLink.idl
    nsIAccessibleHyperText.idl
    nsIAccessibleSelection.idl
    nsIAccessibleTable.idl
    nsIAccessibleText.idl
    nsIAccessibleValue.idl

    */
}

/* virtual functions */

AtkObject *
MaiWidget::GetAtkObject(void)
{
    g_return_val_if_fail(mAccessible != NULL, NULL);

    if (mMaiAtkObject)
        return ATK_OBJECT(mMaiAtkObject);

    nsCOMPtr<nsIAccessible> accessIf(do_QueryInterface(mAccessible));
    if (!accessIf) {
        return NULL;
    }

    CreateMaiInterfaces();
    mMaiAtkObject = (MaiAtkObject*) g_object_new(GetMaiAtkType(), NULL);
    g_return_val_if_fail(mMaiAtkObject != NULL, NULL);

    atk_object_initialize(ATK_OBJECT(mMaiAtkObject), this);
    ATK_OBJECT(mMaiAtkObject)->role = ATK_ROLE_INVALID;
    ATK_OBJECT(mMaiAtkObject)->layer = ATK_LAYER_INVALID;

    return ATK_OBJECT(mMaiAtkObject);
}

MaiObject *
MaiWidget::GetParent(void)
{
    g_return_val_if_fail(mAccessible != NULL, NULL);

    AtkObject *atkObj = GetAtkObject();

    /* MaiTopLevel should always has accessible_parent set to root */
    if (atkObj->accessible_parent) {
        MAI_CHECK_ATK_OBJECT_RETURN_VAL_IF_FAIL(atkObj->accessible_parent,
                                                NULL);
        return MAI_ATK_OBJECT(atkObj->accessible_parent)->maiObject;
    }

    /* try to get parent for it */
    nsCOMPtr<nsIAccessible> accParent(nsnull);
    nsresult rv = mAccessible->GetAccParent(getter_AddRefs(accParent));
    if (NS_FAILED(rv) || !accParent)
        return NULL;

    /* create a new mai parent */
    /* ??? when the new one get freed? */
    MaiWidget *maiParent = new MaiWidget(accParent);
    return maiParent;
}

gint
MaiWidget::GetChildCount(void)
{
    g_return_val_if_fail(mAccessible != NULL, 0);

    gint count = 0;
    mAccessible->GetAccChildCount(&count);
    return count;
}

MaiObject *
MaiWidget::RefChild(gint aChildIndex)
{
    g_return_val_if_fail(mAccessible != NULL, NULL);
    gint count = GetChildCount();
    if ((aChildIndex < 0) || (aChildIndex >= count))
        return NULL;

    MaiObject *maiChild = NULL;
    guint uid;
    // look in cache first
    MaiCache *maiCache = mai_get_cache();
    if (maiCache) {
        uid = GetChildUniqueID(aChildIndex);
        if (uid > 0 && (maiChild = maiCache->Fetch(uid))) {
            g_print("got child 0x%x from cache\n", (guint)maiChild);
            return maiChild;
        }
    }
    // :( not cached yet, get and cache it is possible
    // nsIAccessible child index starts with 1
    gint accChildIndex = 1;
    nsCOMPtr<nsIAccessible> accChild = NULL;
    nsCOMPtr<nsIAccessible> accTmpChild = NULL;
    mAccessible->GetAccFirstChild(getter_AddRefs(accChild));

    while (accChildIndex++ <= aChildIndex && accChild) {
        accChild->GetAccNextSibling(getter_AddRefs(accTmpChild));
        accChild = accTmpChild;
    }
    if (!accChild)
        return NULL;

    // children with different index may have same uid,
    // although they are different nsIAccessilbe objects for
    // different frames, they are deemed equal for accessible
    // user, since they point to the same dom node.
    // So, maybe the it has been cached.
    uid = ::GetNSAccessibleUniqueID(accChild);
    g_return_val_if_fail(uid != 0, NULL);
    if (maiCache)
        maiChild = maiCache->Fetch(uid);

    // not cached, create a new one
    if (!maiChild) {
        maiChild = new MaiWidget(accChild);
        g_return_val_if_fail(maiChild != NULL, NULL);
        if (maiCache) {
            maiCache->Add(maiChild);
            //cache should have add ref, release ours
            g_object_unref(maiChild->GetAtkObject());
        }
    }
    // update children uid list
    SetChildUniqueID(aChildIndex, uid);
    return maiChild;
}

gint
MaiWidget::GetIndexInParent()
{
    g_return_val_if_fail(mAccessible != NULL, -1);

    MaiObject *maiParent = GetParent();
    g_return_val_if_fail(maiParent != NULL, -1);

    gint childCountInParent = maiParent->GetChildCount();
    MaiWidget *maiSibling = NULL;
    for (int index = 0; index < childCountInParent; index++) {
        maiSibling = (MaiWidget*)maiParent->RefChild(index);
        if (maiSibling->GetNSAccessibleUniqueID() ==
            GetNSAccessibleUniqueID())
            return index;
    }
    return -1;
}

/* static functions */

gchar *
MaiWidget::GetUniqueMaiAtkTypeName(void)
{
#define MAI_ATK_TYPE_NAME_LEN (30)     /* 10+sizeof(gulong)*8/4+1 < 30 */

    static gchar namePrefix[] = "MaiAtkType";   /* size = 10 */
    static gchar name[MAI_ATK_TYPE_NAME_LEN + 1];

    sprintf(name, "%s%lx", namePrefix, mAtkTypeNameIndex++);
    name[MAI_ATK_TYPE_NAME_LEN] = '\0';

    MAI_LOG_DEBUG(("MaiWidget::LastedTypeName=%s\n", name));

    return name;
}

/* private */
guint
MaiWidget::GetChildUniqueID(gint aChildIndex)
{
    gpointer pValue=
        g_hash_table_lookup(mChildren,
                            NS_REINTERPRET_CAST(const void *, aChildIndex));
    if (!pValue)
        return 0;
    return NS_REINTERPRET_CAST(guint, pValue);
}

void
MaiWidget::SetChildUniqueID(gint aChildIndex, guint aChildUid)
{
    //If the key already exists in the GHashTable its current value is
    //replaced with the new value.
    g_hash_table_insert(mChildren,
                        NS_REINTERPRET_CAST(void*, aChildIndex),
                        NS_REINTERPRET_CAST(void*, aChildUid));
}
