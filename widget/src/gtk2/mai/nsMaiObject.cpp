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
 * Contributor(s): John Sun (john.sun@sun.com)
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
#include "nsMaiUtil.h"
#include "nsMaiCache.h"
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

/* implemented by MaiWidget */
//static AtkStateSet*        refStateSetCB(AtkObject *aObj);
//static AtkRole             getRoleCB(AtkObject *aObj);

//static AtkRelationSet*     refRelationSetCB(AtkObject *aObj);
//static AtkLayer            getLayerCB(AtkObject *aObj);
//static gint                getMdiZorderCB(AtkObject *aObj);

/* the missing atkobject virtual functions */
/*
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
    NS_ASSERTION((mMaiAtkObject == NULL),
                 "Cannot Del MaiObject: alive atk object");

#ifdef MAI_LOGGING
    num_deleted_mai_object++;
#endif
    MAI_LOG_DEBUG(("====MaiObject deleting this=0x%x, total =%d= deleted\n",
                   (unsigned int)this, num_deleted_mai_object));
}

nsIAccessible *
MaiObject::GetNSAccessible(void)
{
    return mAccessible;
}

/* virtual functions to ATK callbacks */

/* for those get functions who have a related memeber vairable in AtkObject
 * (see atkobject.h), save a copy of the value in atkobject, return them
 * when available, create one and save if not.
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
    //here we know that the action is originated from the MaiAtkObject,
    //and the MaiAtkObject itself will be destroyed.
    //Mark MaiAtkObject to nil
    mMaiAtkObject = NULL;

    delete this;
}

/* global */
guint
GetNSAccessibleUniqueID(nsIAccessible *aObj)
{
    g_return_val_if_fail(aObj != NULL, 0);

    PRInt32 accId = 0;
    aObj->GetAccId(&accId);
    return accId;
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
    MAI_LOG_DEBUG(("====release MaiAtkObject=0x%x, MaiObject=0x%x\n",
                   (guint)aObj, (guint)maiObject));

    MaiHashTable::Remove(maiObject);

    maiObject->Finalize();

    // never call MaiObject later
    MAI_ATK_OBJECT(aObj)->maiObject = NULL;

    // call parent finalize function
    // finalize of GObjectClass will unref the accessible parent if has
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

    return childObject->GetAtkObject();
}

gint
getIndexInParentCB(AtkObject *aObj)
{
    MAI_CHECK_ATK_OBJECT_RETURN_VAL_IF_FAIL(aObj, -1);
    MaiObject *maiObject = MAI_ATK_OBJECT(aObj)->maiObject;

    return maiObject->GetIndexInParent();
}

/*******************************************************************************
The following nsIAccessible states aren't translated, just ignored.
  STATE_MIXED:         For a three-state check box.
  STATE_READONLY:      The object is designated read-only.
  STATE_HOTTRACKED:    Means its appearance has changed to indicate mouse
                       over it.
  STATE_DEFAULT:       Represents the default button in a window.
  STATE_FLOATING:      Not supported yet.
  STATE_MARQUEED:      Indicate scrolling or moving text or graphics.
  STATE_ANIMATED:
  STATE_OFFSCREEN:     Has no on-screen representation.
  STATE_MOVEABLE:
  STATE_SELFVOICING:   The object has self-TTS.
  STATE_LINKED:        The object is formatted as a hyperlink.
  STATE_TRAVERSE:      The object is a hyperlink that has been visited.
  STATE_EXTSELECTABLE: Indicates that an object extends its selectioin.
  STATE_ALERT_LOW:     Not supported yet.
  STATE_ALERT_MEDIUM:  Not supported yet.
  STATE_ALERT_HIGH:    Not supported yet.
  STATE_PROTECTED:     The object is a password-protected edit control.
  STATE_HASPOPUP:      Object displays a pop-up menu or window when invoked.

Returned AtkStatusSet never contain the following AtkStates.
  ATK_STATE_ARMED:     Indicates that the object is armed.
  ATK_STATE_DEFUNCT:   Indicates the user interface object corresponding to thus
                       object no longer exists.
  ATK_STATE_EDITABLE:  Indicates the user can change the contents of the object.
  ATK_STATE_HORIZONTAL:Indicates the orientation of this object is horizontal.
  ATK_STATE_ICONIFIED:
  ATK_STATE_OPAQUE:     Indicates the object paints every pixel within its
                        rectangular region
  ATK_STATE_STALE:      The index associated with this object has changed since
                        the user accessed the object
******************************************************************************/

void
MaiObject::TranslateStates(PRUint32 aAccState, AtkStateSet *state_set)
{
    g_return_if_fail(state_set);

    if (aAccState & nsIAccessible::STATE_SELECTED)
        atk_state_set_add_state (state_set, ATK_STATE_SELECTED);

    if (aAccState & nsIAccessible::STATE_FOCUSED)
        atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);

    if (aAccState & nsIAccessible::STATE_PRESSED)
        atk_state_set_add_state (state_set, ATK_STATE_PRESSED);

    if (aAccState & nsIAccessible::STATE_CHECKED)
        atk_state_set_add_state (state_set, ATK_STATE_CHECKED);

    if (aAccState & nsIAccessible::STATE_EXPANDED)
        atk_state_set_add_state (state_set, ATK_STATE_EXPANDED);

    if (aAccState & nsIAccessible::STATE_COLLAPSED)
        atk_state_set_add_state (state_set, ATK_STATE_EXPANDABLE);
                   
    // The control can't accept input at this time
    if (aAccState & nsIAccessible::STATE_BUSY)
        atk_state_set_add_state (state_set, ATK_STATE_BUSY);

    if (aAccState & nsIAccessible::STATE_FOCUSABLE)
        atk_state_set_add_state (state_set, ATK_STATE_FOCUSABLE);

    if (!(aAccState & nsIAccessible::STATE_INVISIBLE))
        atk_state_set_add_state (state_set, ATK_STATE_VISIBLE);

    if (aAccState & nsIAccessible::STATE_SELECTABLE)
        atk_state_set_add_state (state_set, ATK_STATE_SELECTABLE);

    if (aAccState & nsIAccessible::STATE_SIZEABLE)
        atk_state_set_add_state (state_set, ATK_STATE_RESIZABLE);

    if (aAccState & nsIAccessible::STATE_MULTISELECTABLE)
        atk_state_set_add_state (state_set, ATK_STATE_MULTISELECTABLE);

    if (!(aAccState & nsIAccessible::STATE_UNAVAILABLE))
        atk_state_set_add_state (state_set, ATK_STATE_ENABLED);

    // The following state is
    // Extended state flags (for now non-MSAA, for Java and Gnome/ATK support)
    // This is only the states that there isn't already a mapping for in MSAA
    // See www.accessmozilla.org/article.php?sid=11 for information on the
    // mappings between accessibility API state
    if (aAccState & nsIAccessible::STATE_INVALID)
        atk_state_set_add_state (state_set, ATK_STATE_INVALID);

    if (aAccState & nsIAccessible::STATE_ACTIVE)
        atk_state_set_add_state (state_set, ATK_STATE_ACTIVE);

    if (aAccState & nsIAccessible::STATE_EXPANDABLE)
        atk_state_set_add_state (state_set, ATK_STATE_EXPANDABLE);

    if (aAccState & nsIAccessible::STATE_MODAL)
        atk_state_set_add_state (state_set, ATK_STATE_MODAL);

    if (aAccState & nsIAccessible::STATE_MULTI_LINE)
        atk_state_set_add_state (state_set, ATK_STATE_MULTI_LINE);

    if (aAccState & nsIAccessible::STATE_SENSITIVE)
        atk_state_set_add_state (state_set, ATK_STATE_SENSITIVE);

    if (aAccState & nsIAccessible::STATE_RESIZABLE)
        atk_state_set_add_state (state_set, ATK_STATE_RESIZABLE);

    if (aAccState & nsIAccessible::STATE_SHOWING)
        atk_state_set_add_state (state_set, ATK_STATE_SHOWING);

    if (aAccState & nsIAccessible::STATE_SINGLE_LINE)
        atk_state_set_add_state (state_set, ATK_STATE_SINGLE_LINE);

    if (aAccState & nsIAccessible::STATE_TRANSIENT)
        atk_state_set_add_state (state_set, ATK_STATE_TRANSIENT);

    if (aAccState & nsIAccessible::STATE_VERTICAL)
        atk_state_set_add_state (state_set, ATK_STATE_VERTICAL);

}
