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

#include "nsMaiObject.h"
#include "nsIAccessibleEventListener.h"
#include "nsString.h"

#ifdef MAI_LOGGING
gint num_created_mai_object = 0;
gint num_deleted_mai_object = 0;
#endif

G_BEGIN_DECLS
/* callbacks for MaiAtkObject */
static void classInitCB(AtkObjectClass *aClass);
static void initializeCB(AtkObject *aObj, gpointer aData);
static void finalizeCB(GObject *aObj);

/* callbacks for AtkObject virtual functions */
static const gchar*        getNameCB (AtkObject *aObj);
static const gchar*        getDescriptionCB (AtkObject *aObj);
static AtkObject*          getParentCB(AtkObject *aObj);
static gint                getChildCountCB(AtkObject *aObj);
static AtkObject*          refChildCB(AtkObject *aObj, gint aChildIndex);
static gint                getIndexInParentCB(AtkObject *aObj);
//static AtkRelationSet*     refRelationSetCB(AtkObject *aObj);
//static AtkRole             getRoleCB(AtkObject *aObj);
//static AtkLayer            getLayerCB(AtkObject *aObj);
//static gint                getMdiZorderCB(AtkObject *aObj);

/* the missing atkobject virtual functions */
/*
  static AtkStateSet*        refStateSetCB(AtkObject *aObj);
  static void                SetNameCB(AtkObject *aObj,
  const gchar *name);
  static void                SetDescriptionCB(AtkObject *aObj,
  const gchar *description);
  static void                SetParentCB(AtkObject *aObj,
  AtkObject *parent);
  static void                SetRoleCB(AtkObject *aObj,
  AtkRole role);
  static guint               ConnectPropertyChangeHandlerCB(
  AtkObject  *aObj,
  AtkPropertyChangeHandler *handler);
  static void                RemovePropertyChangeHandlerCB(
  AtkObject *aObj,
  guint handler_id);
  static void                InitializeCB(AtkObject *aObj,
  gpointer data);
  static void                ChildrenChangedCB(AtkObject *aObj,
  guint change_index,
  gpointer changed_child);
  static void                FocusEventCB(AtkObject *aObj,
  gboolean focus_in);
  static void                PropertyChangeCB(AtkObject *aObj,
  AtkPropertyValues *values);
  static void                StateChangeCB(AtkObject *aObj,
  const gchar *name,
  gboolean state_set);
  static void                VisibleDataChangedCB(AtkObject *aObj);
*/
G_END_DECLS

static gpointer parent_class = NULL;

GType
mai_atk_object_get_type(void)
{
    static GType type = 0;

    if (!type) {
        static const GTypeInfo tinfo = {
            sizeof(MaiAtkObjectClass),
            (GBaseInitFunc)NULL,
            (GBaseFinalizeFunc)NULL,
            (GClassInitFunc)classInitCB,
            (GClassFinalizeFunc)NULL,
            NULL, /* class data */
            sizeof(MaiAtkObject), /* instance size */
            0, /* nb preallocs */
            (GInstanceInitFunc)NULL,
            NULL /* value table */
        };

        type = g_type_register_static(ATK_TYPE_OBJECT,
                                      "MaiAtkObject", &tinfo, GTypeFlags(0));
    }
    return type;
}

MaiObject::MaiObject(nsIAccessible *aAcc)
{
    mAccessible = aAcc;
    mMaiAtkObject = NULL;

#ifdef MAI_LOGGING
    num_created_mai_object++;
#endif
    MAI_LOG_DEBUG(("====MaiObject creating this=0x%x,total =%d= created\n",
                   (unsigned int)this, num_created_mai_object));
}

MaiObject::~MaiObject()
{
    /* mAccessible will get released here automatically */
    mMaiAtkObject = NULL;

#ifdef MAI_LOGGING
    num_deleted_mai_object++;
#endif
    MAI_LOG_DEBUG(("====MaiObject deleting this=0x%x, total =%d= deleted\n",
                   (unsigned int)this, num_deleted_mai_object));
}

void
MaiObject::EmitAccessibilitySignal(PRUint32 aEvent)
{
    AtkObject *atkObj = GetAtkObject();
    if (atkObj) {
        gchar *name = NULL;
        name = GetAtkSignalName(aEvent);
        if (name) {
            g_signal_emit_by_name(atkObj, name);

            MAI_LOG_DEBUG(("MaiObject, emit signal %s\n", name));
        }
    }
}

nsIAccessible *
MaiObject::GetNSAccessible(void)
{
    return mAccessible;
}

gchar *
MaiObject::GetAtkSignalName(PRUint32 aEvent)
{
    gchar *name = NULL;

    switch (aEvent) {
    default:
        return NULL;
    }
    return name;
}

/* virtual functions to ATK callbacks */

/* for those get functions who have a related memeber vairable in AtkObject
 * (see atkobject.h), save a copy of the value in atkobject, return them
 * when available, get a new one and save if not.
 *
 * virtual functions that fall into this scope:
 *   GetName x
 *   GetDescription x
 *   GetParent x
 *   GetRole
 *   GetRelationSet
 *   GetLayer
 */

gchar *
MaiObject::GetName(void)
{
    g_return_val_if_fail(mAccessible != NULL, NULL);

    AtkObject *atkObject = (AtkObject*)mMaiAtkObject;
    static gchar default_name[] = "no name";

    if (!atkObject->name) {
        gint len;
        nsAutoString uniName;

        /* nsIAccessible is responsible for the non-NULL name */
        nsresult rv = mAccessible->GetAccName(uniName);
        if (NS_FAILED(rv))
            return NULL;
        len = uniName.Length();
        if (len > 0) {
            atk_object_set_name(atkObject,
                                NS_ConvertUCS2toUTF8(uniName).get());
        }
        else {
            atk_object_set_name(atkObject, default_name);
        }
    }
    return atkObject->name;
}

gchar *
MaiObject::GetDescription(void)
{
    g_return_val_if_fail(mAccessible != NULL, NULL);

    AtkObject *atkObject = (AtkObject*)mMaiAtkObject;

    if (!atkObject->description) {
        gchar default_description[] = "no description";
        gint len;
        nsAutoString uniDesc;

        /* nsIAccessible is responsible for the non-NULL description */
        nsresult rv = mAccessible->GetAccDescription(uniDesc);
        if (NS_FAILED(rv))
            return NULL;
        len = uniDesc.Length();
        if (len > 0) {
            atk_object_set_description(atkObject,
                                       NS_ConvertUCS2toUTF8(uniDesc).get());
        }
        else {
            atk_object_set_description(atkObject, default_description);
        }
    }
    return atkObject->description;
}

gint
MaiObject::GetChildCount(void)
{
    return 0;
}

MaiObject *
MaiObject::RefChild(gint aChildIndex)
{
    return NULL;
}

gint
MaiObject::GetIndexInParent()
{
    return -1;
}

void
MaiObject::Initialize(void)
{
}

void
MaiObject::Finalize(void)
{
    delete this;
}

/* global */
guint
GetNSAccessibleUniqueID(nsIAccessible *aObj)
{
    g_return_val_if_fail(aObj != NULL, 0);

    nsCOMPtr<nsIDOMNode> domNode;
    aObj->AccGetDOMNode(getter_AddRefs(domNode));
    guint uid = -NS_REINTERPRET_CAST(gint, (domNode.get()));
    return uid;
}

/* static functions for ATK callbacks */

void
classInitCB(AtkObjectClass *aClass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(aClass);

    parent_class = g_type_class_peek_parent(aClass);

    aClass->get_name = getNameCB;
    aClass->get_description = getDescriptionCB;
    aClass->get_parent = getParentCB;
    aClass->get_n_children = getChildCountCB;
    aClass->ref_child = refChildCB;
    aClass->get_index_in_parent = getIndexInParentCB;
    //  aClass->ref_state_set = refStateSetCB;
    aClass->initialize = initializeCB;

    gobject_class->finalize = finalizeCB;
}

void
initializeCB(AtkObject *aObj, gpointer aData)
{
    g_return_if_fail(MAI_IS_ATK_OBJECT(aObj));
    g_return_if_fail(aData != NULL);

    /* call parent init function */
    /* AtkObjectClass has not a "initialize" function now,
     * maybe it has later
     */

    if (ATK_OBJECT_CLASS(parent_class)->initialize)
        ATK_OBJECT_CLASS(parent_class)->initialize(aObj, aData);

    /* initialize object */
    MAI_ATK_OBJECT(aObj)->maiObject = NS_STATIC_CAST(MaiObject*, aData);
    NS_STATIC_CAST(MaiObject*, aData)->Initialize();
}

void
finalizeCB(GObject *aObj)
{
    MAI_CHECK_ATK_OBJECT_RETURN_IF_FAIL(aObj);
    MaiObject *maiObject = MAI_ATK_OBJECT(aObj)->maiObject;
    maiObject->Finalize();

    /* call parent finalize function */
    if (G_OBJECT_CLASS (parent_class)->finalize)
        G_OBJECT_CLASS (parent_class)->finalize(aObj);
}

const gchar *
getNameCB(AtkObject *aObj)
{
    MAI_CHECK_ATK_OBJECT_RETURN_VAL_IF_FAIL(aObj, NULL);
    MaiObject *maiObject = MAI_ATK_OBJECT(aObj)->maiObject;
    return maiObject->GetName();
}

const gchar *
getDescriptionCB(AtkObject *aObj)
{
    MAI_CHECK_ATK_OBJECT_RETURN_VAL_IF_FAIL(aObj, NULL);
    MaiObject *maiObject = MAI_ATK_OBJECT(aObj)->maiObject;
    return maiObject->GetDescription();
}

AtkObject *
getParentCB(AtkObject *aObj)
{
    MAI_CHECK_ATK_OBJECT_RETURN_VAL_IF_FAIL(aObj, NULL);
    MaiObject *maiObject = MAI_ATK_OBJECT(aObj)->maiObject;

    MaiObject *parentObj = maiObject->GetParent();
    if (!parentObj)
        return NULL;

    AtkObject *parentAtkObj = parentObj->GetAtkObject();
    if (parentAtkObj && !aObj->accessible_parent) {
        atk_object_set_parent(aObj, parentAtkObj);
    }
    return parentAtkObj;
}

gint
getChildCountCB(AtkObject *aObj)
{
    MAI_CHECK_ATK_OBJECT_RETURN_VAL_IF_FAIL(aObj, 0);
    MaiObject * maiObject = MAI_ATK_OBJECT(aObj)->maiObject;
    return maiObject->GetChildCount();
}

AtkObject *
refChildCB(AtkObject *aObj, gint aChildIndex)
{
    MAI_CHECK_ATK_OBJECT_RETURN_VAL_IF_FAIL(aObj, NULL);
    MaiObject *maiObject = MAI_ATK_OBJECT(aObj)->maiObject;

    MaiObject *childObject = maiObject->RefChild(aChildIndex);
    if (!childObject)
        return NULL;

    AtkObject *childAtkObj = childObject->GetAtkObject();
    if (childAtkObj) {
        g_object_ref(childAtkObj);
        if (!childAtkObj->accessible_parent)
            atk_object_set_parent(childAtkObj, aObj);
    }
    return childAtkObj;
}

gint
getIndexInParentCB(AtkObject *aObj)
{
    MAI_CHECK_ATK_OBJECT_RETURN_VAL_IF_FAIL(aObj, -1);
    MaiObject *maiObject = MAI_ATK_OBJECT(aObj)->maiObject;

    return maiObject->GetIndexInParent();
}
