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
#include "nsIAccessibleText.h"
#include "nsIAccessibleEditableText.h"
#include "nsIAccessibleTable.h"
#include "nsIAccessibleSelectable.h"
#include "nsIAccessibleHyperText.h"
#include "nsIAccessibleValue.h"

#include "nsMaiWidget.h"
#include "nsMaiAppRoot.h"
#include "nsMaiInterfaceComponent.h"
#include "nsMaiInterfaceAction.h"
#include "nsMaiInterfaceText.h"
#include "nsMaiInterfaceEditableText.h"
#include "nsMaiInterfaceTable.h"
#include "nsMaiInterfaceSelection.h"
#include "nsMaiInterfaceHypertext.h"
#include "nsMaiInterfaceValue.h"

#include "nsMaiUtil.h"

G_BEGIN_DECLS

void classInitCB(AtkObjectClass *aClass);
void initializeCB(AtkObject *aObj, gpointer aData);
void finalizeCB(GObject *aObj);

/* more callbacks for atkobject */
static AtkStateSet*        refStateSetCB(AtkObject *aObj);
static AtkRole             getRoleCB(AtkObject *aObj);

G_END_DECLS

static gpointer parent_class = NULL;

GType
mai_atk_widget_get_type(void)
{
    static GType type = 0;

    if (!type) {
        static const GTypeInfo tinfo = {
            sizeof(MaiAtkWidgetClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) classInitCB,
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
    g_print("%s: this=0x%x, aDepth=%d, type=%s", GetName(),
            (unsigned int)this, aDepth, "MaiWidget");

    g_print(", UID=%x", GetNSAccessibleUniqueID());
    g_print(", Iface");
    for (int ifaces = 0; ifaces < MAI_INTERFACE_NUM; ifaces++) {
        if (mMaiInterface[ifaces])
            g_print(" : %d", ifaces);
    }
    gint nChild = GetChildCount();
    gint nIndexInParent = GetIndexInParent();
    g_print(", IndexInParent=%d, #child=%d\n", nIndexInParent, nChild);

    g_print("<ul>\n");
    MaiObject *maiChild;
    for (int index = 0; index < nChild; index++) {
        maiChild = RefChild(index);
        if (maiChild) {
            g_print("<li>");
            maiChild->DumpMaiObjectInfo(aDepth);
        }
    }
    g_print("</ul>\n");

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

/*static*/
//////////////////////////////////////////////////////////////////////
// MaiWidget *
// MaiWidget::CreateAndCache(nsIAccessible *aAcc);
//
// A helper to create a new MaiWidget and cache it.
//-------------------------------------------------------------------
// This Method gets a cached MaiWidget for the nsIAccessible, create one
// if needed and cache it. Only when create a new MaiWidget, |aAcc| will
// be addrefed.
//
// **Note**
// The returned cached MaiWidget object is NOT guaranteed to be there when
// you fetch it next time. Accordingly, DO NOT keep the returned pointer
// and use it later. Typically, you pass the result of this method to a
// callback.
// If it is really needed to keep the result for later use, please use
//    g_object_ref(maiWidget->GetAtkObject());
// to ensure the |maiWidget| pointer is still valid, even when the MaiWidget
// is removed from the cache. And when you do not need it, you have to use:
//    g_object_unref(maiWidget->GetAtkObject());
// to release your reference.
////////////////////////////////////////////////////////////////////////
MaiWidget *
MaiWidget::CreateAndCache(nsIAccessible *aAcc)
{
    if (!aAcc)
        return NULL;

    MaiCache *maiCache = mai_get_cache();
    if (!maiCache)
        return NULL;

    MaiWidget *retWidget = NS_STATIC_CAST(MaiWidget*, maiCache->Fetch(aAcc));
    //there is a maiWidget in cache for the nsIAccessible already.
    if (retWidget)
        return retWidget;

    //create one, and cache it.
    retWidget = new MaiWidget(aAcc);
    NS_ASSERTION(retWidget, "Fail to create mai object");

    maiCache->Add(retWidget);
    //cache should have add ref, release ours
    g_object_unref(retWidget->GetAtkObject());

    return retWidget;
}

void
MaiWidget::ChildrenChange(AtkChildrenChange *event)
{
    MaiWidget *maiChild;
    if (event && event->child &&
        (maiChild = CreateAndCache(event->child))) {
        //update the specified child, but now use the easiest way.
        g_hash_table_destroy(mChildren);
        mChildren = g_hash_table_new(g_direct_hash, NULL);
    }
    else {
        //update whole child list
        g_hash_table_destroy(mChildren);
        mChildren = g_hash_table_new(g_direct_hash, NULL);
    }
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

    if (mMaiInterfaceCount == 0)
        return MAI_TYPE_ATK_WIDGET;

    type = g_type_register_static(MAI_TYPE_ATK_WIDGET,
                                  MaiWidget::GetUniqueMaiAtkTypeName(),
                                  &tinfo, GTypeFlags(0));

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

    // the Component and interface are supported by all nsIAccessible

    // Add Interfaces for each nsIAccessible.ext interfaces
    MaiInterfaceComponent *maiInterfaceComponent =
        new MaiInterfaceComponent(this);
    NS_ASSERTION(maiInterfaceComponent, "Fail to add component interface");
    AddMaiInterface(maiInterfaceComponent);

    MaiInterfaceAction *maiInterfaceAction = new MaiInterfaceAction(this);
    NS_ASSERTION(maiInterfaceAction, "Fail to add Action interface");
    AddMaiInterface(maiInterfaceAction);

    //nsIAccessibleText
    nsCOMPtr<nsIAccessibleText>
        accessInterfaceText(do_QueryInterface(mAccessible));
    if (accessInterfaceText) {
        MaiInterfaceText *maiInterfaceText = new MaiInterfaceText(this);
        NS_ASSERTION(maiInterfaceText, "Fail to add text interface");
        AddMaiInterface(maiInterfaceText);
    }

    //nsIAccessibleEditableText
    nsCOMPtr<nsIAccessibleEditableText>
        accessInterfaceEditableText(do_QueryInterface(mAccessible));
    if (accessInterfaceEditableText) {
        MaiInterfaceEditableText *maiInterfaceEditableText =
            new MaiInterfaceEditableText(this);
        NS_ASSERTION(maiInterfaceEditableText,
                     "Fail to add editabletext interface");
        AddMaiInterface(maiInterfaceEditableText);
    }

    //nsIAccessibleTable
    nsCOMPtr<nsIAccessibleTable>
        accessInterfaceTable(do_QueryInterface(mAccessible));
    if (accessInterfaceTable) {
        MaiInterfaceTable *maiInterfaceTable = new MaiInterfaceTable(this);
        NS_ASSERTION(maiInterfaceTable, "Fail to add table interface");
        AddMaiInterface(maiInterfaceTable);
    }

    //nsIAccessibleSelection
    nsCOMPtr<nsIAccessibleSelectable>
        accessInterfaceSelection(do_QueryInterface(mAccessible));
    if (accessInterfaceSelection) {
        MaiInterfaceSelection *maiInterfaceSelection =
            new MaiInterfaceSelection(this);
        NS_ASSERTION(maiInterfaceSelection, "Fail to add selection interface");
        AddMaiInterface(maiInterfaceSelection);
    }

    //nsIAccessibleHypertext
    nsCOMPtr<nsIAccessibleHyperText>
        accessInterfaceHypertext(do_QueryInterface(mAccessible));
    if (accessInterfaceHypertext) {
        MaiInterfaceHypertext *maiInterfaceHypertext =
            new MaiInterfaceHypertext(this);
        NS_ASSERTION(maiInterfaceHypertext, "Fail to add hypertext interface");
        AddMaiInterface(maiInterfaceHypertext);
    }

    //nsIAccessibleValue
    nsCOMPtr<nsIAccessibleValue>
        accessInterfaceValue(do_QueryInterface(mAccessible));
    if (accessInterfaceValue) {
        MaiInterfaceValue *maiInterfaceValue =
            new MaiInterfaceValue(this);
        NS_ASSERTION(maiInterfaceValue, "Fail to add value interface");
        AddMaiInterface(maiInterfaceValue);
    }
}

/* virtual functions of MaiObject*/

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
    MAI_LOG_DEBUG(("MaiWidget: Create MaiAtkObject=0x%x for MaiWidget 0x%x\n",
                   (guint)mMaiAtkObject, (guint)this));

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

    /* create a maiWidget for parent */
    return CreateAndCache(accParent);
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
            MAI_LOG_DEBUG(("got child 0x%x from cache\n", (guint)maiChild));
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

    // not cached, create one
    if (!maiChild) {
        maiChild = CreateAndCache(accChild);
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

PRUint32
MaiWidget::RefStateSet()
{
    g_return_val_if_fail(mAccessible != NULL, 0);
    PRUint32 accState;
    nsresult rv = mAccessible->GetAccState(&accState);
    return (NS_FAILED(rv)) ? 0 : accState;
}

PRUint32
MaiWidget::GetRole()
{
    g_return_val_if_fail(mAccessible != NULL, 0);
    PRUint32 accRole;
    nsresult rv = mAccessible->GetAccRole(&accRole);
    return (NS_FAILED(rv)) ? 0 : accRole;
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
    //If the key already exists in the GHashTable,
    //the old value is replaced.
    g_hash_table_insert(mChildren,
                        NS_REINTERPRET_CAST(void*, aChildIndex),
                        NS_REINTERPRET_CAST(void*, aChildUid));
}

void
classInitCB(AtkObjectClass *aClass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(aClass);

    parent_class = g_type_class_peek_parent(aClass);

    aClass->ref_state_set = refStateSetCB;
    aClass->get_role = getRoleCB;

    // aClass->initialize = initializeCB;

    // gobject_class->finalize = finalizeCB;
}

void
initializeCB(AtkObject *aObj, gpointer aData)
{
    g_return_if_fail(MAI_IS_ATK_WIDGET(aObj));
    g_return_if_fail(aData != NULL);

    /* call parent init function */
    /* AtkObjectClass has not a "initialize" function now,
     * maybe it has later
     */

    if (ATK_OBJECT_CLASS(parent_class)->initialize)
        ATK_OBJECT_CLASS(parent_class)->initialize(aObj, aData);

}

#define MAI_ATK_WIDGET_RETURN_VAL_IF_FAIL(obj, val) \
    do {\
        g_return_val_if_fail(MAI_IS_ATK_WIDGET(obj), val);\
        MaiObject * tmpMaiObjectIn = MAI_ATK_OBJECT(obj)->maiObject; \
        g_return_val_if_fail(tmpMaiObjectIn != NULL, val);\
        g_return_val_if_fail(tmpMaiObjectIn->GetAtkObject() == obj, val); \
    } while (0)

#define MAI_ATK_WIDGET_RETURN_IF_FAIL(obj) \
    do {\
        g_return_if_fail(MAI_IS_ATK_WIDGET(obj));\
        MaiObject * tmpMaiObjectIn = MAI_ATK_OBJECT(obj)->maiObject; \
        g_return_if_fail(tmpMaiObjectIn != NULL);\
        g_return_if_fail(tmpMaiObjectIn->GetAtkObject() == obj); \
    } while (0)

void
finalizeCB(GObject *aObj)
{
    //    MAI_ATK_WIDGET_RETURN_IF_FAIL(aObj);

    // call parent finalize function
    // finalize of GObjectClass will unref the accessible parent if has
    if (G_OBJECT_CLASS (parent_class)->finalize)
        G_OBJECT_CLASS (parent_class)->finalize(aObj);
}

AtkStateSet *
refStateSetCB(AtkObject *aObj)
{
    MAI_ATK_WIDGET_RETURN_VAL_IF_FAIL(aObj, NULL);

    AtkStateSet *state_set;

    state_set = ATK_OBJECT_CLASS (parent_class)->ref_state_set (aObj);

    MaiWidget *maiWidget = NS_STATIC_CAST(MaiWidget*,
                                          MAI_ATK_OBJECT(aObj)->maiObject);
    PRUint32 accState = maiWidget->RefStateSet();
    if (accState != 0)
        MaiObject::TranslateStates(accState, state_set);
    return state_set;
}

AtkRole
getRoleCB(AtkObject *aObj)
{
    MAI_ATK_WIDGET_RETURN_VAL_IF_FAIL(aObj, ATK_ROLE_INVALID);

    if (aObj->role != ATK_ROLE_INVALID)
        return aObj->role;
    MaiWidget *maiWidget = NS_STATIC_CAST(MaiWidget*,
                                          MAI_ATK_OBJECT(aObj)->maiObject);

    AtkRole atkRole = NS_STATIC_CAST(AtkRole, maiWidget->GetRole());
    aObj->role = atkRole;
    return atkRole;
}
