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

#include "nsCOMPtr.h"
#include "nsMaiUtil.h"
#include "nsMaiCache.h"
#include "nsMaiTopLevel.h"
#include "nsIAccessibleEventReceiver.h"
#include "nsAccessibleEventData.h"

/*
 * Must keep sychronization with
 * enumerate AtkProperty in mozilla/accessible/src/base/nsRootAccessible.h
 */
static char * pAtkPropertyNameArray[PROP_LAST] = {
    0,
    "accessible_name",
    "accessible_description",
    "accessible_parent",
    "accessible_value",
    "accessible_role",
    "accessible_layer",
    "accessible_mdi_zorder",
    "accessible_table_caption",
    "accessible_table_column_description",
    "accessible_table_column_header",
    "accessible_table_row_description",
    "accessible_table_row_header",
    "accessible_table_summary"
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(MaiTopLevel, nsIAccessibleEventListener)

MaiTopLevel::MaiTopLevel(nsIAccessible *aAcc):MaiWidget(aAcc)
{
    NS_INIT_ISUPPORTS();

    nsCOMPtr<nsIAccessibleEventReceiver>
        receiver(do_QueryInterface(mAccessible));
    if (receiver)
        receiver->AddAccessibleEventListener(this);
}

MaiTopLevel::~MaiTopLevel()
{
    nsCOMPtr<nsIAccessibleEventReceiver>
        receiver(do_QueryInterface(mAccessible));
    if (receiver)
        receiver->RemoveAccessibleEventListener();
}

NS_IMETHODIMP
MaiTopLevel::HandleEvent(PRUint32 aEvent, nsIAccessible *aAccessible,
                         AccessibleEventData * aEventData)
{
    MaiObject *pMaiObject;
    AtkTableChange * pAtkTableChange;

    MAI_LOG_DEBUG(("Received accessary data ptr is:%x\n", aEventData));

    if (mAccessible == aAccessible)
        pMaiObject = this;
    else
        pMaiObject = CreateMaiObjectFor(aAccessible);

    if (!pMaiObject)
        return NS_ERROR_FAILURE;

    switch (aEvent) {
    case nsIAccessibleEventListener::EVENT_FOCUS:
        atk_focus_tracker_notify(ATK_OBJECT(pMaiObject->GetAtkObject()));
        break;
    
    case nsIAccessibleEventListener::EVENT_STATE_CHANGE:
        AtkStateChange *pAtkStateChange;
        AtkStateType atkState;

        MAI_LOG_DEBUG(("Receiving event EVENT_STATE_CHANGE\n\n"));
        if (!aEventData)
            return NS_ERROR_FAILURE;

        pAtkStateChange = NS_REINTERPRET_CAST(AtkStateChange *, aEventData);

        switch (pAtkStateChange->state) {
        case nsIAccessible::STATE_INVISIBLE:
            atkState = ATK_STATE_VISIBLE;
            break;
        case nsIAccessible::STATE_UNAVAILABLE:
            atkState = ATK_STATE_ENABLED;
            break;
        default:
            atkState = TranslateAState(pAtkStateChange->state);
        }

        atk_object_notify_state_change(ATK_OBJECT(pMaiObject->GetAtkObject()),
                                       atkState, pAtkStateChange->enable);
        break;
      
        /*
         * More complex than I ever thought.
         * Need handle them separately.
         */
    case nsIAccessibleEventListener::EVENT_ATK_PROPERTY_CHANGE :
        AtkPropertyChange *pAtkPropChange;
        AtkPropertyValues values;

        MAI_LOG_DEBUG(("Receiving event EVENT_ATK_PROPERTY_CHANGE\n\n"));
        if (!aEventData)
            return NS_ERROR_FAILURE;

        pAtkPropChange = NS_REINTERPRET_CAST(AtkPropertyChange *, aEventData);
        values.property_name = pAtkPropertyNameArray[pAtkPropChange->type];
        
        MAI_LOG_DEBUG(("the type of EVENT_ATK_PROPERTY_CHANGE: %d\n\n",
                       pAtkPropChange->type));
        switch (pAtkPropChange->type) {
        case PROP_TABLE_CAPTION:
        case PROP_TABLE_SUMMARY:
            MaiObject *aOldMaiObj, *aNewMaiObj;

            if (pAtkPropChange->oldvalue)
                aOldMaiObj = CreateMaiObjectFor(NS_REINTERPRET_CAST
                                                (nsIAccessible *,
                                                 pAtkPropChange->oldvalue));

            if (pAtkPropChange->newvalue)
                aNewMaiObj = CreateMaiObjectFor(NS_REINTERPRET_CAST
                                                (nsIAccessible *,
                                                 pAtkPropChange->newvalue));

            if (!aOldMaiObj || !aNewMaiObj )
                return NS_ERROR_FAILURE;

            g_value_init(&values.old_value, G_TYPE_POINTER);
            g_value_set_pointer(&values.old_value,
                                ATK_OBJECT(aOldMaiObj->GetAtkObject()));
            g_value_init(&values.new_value, G_TYPE_POINTER);
            g_value_set_pointer(&values.new_value,
                                ATK_OBJECT(aNewMaiObj->GetAtkObject()));
            break;

        case PROP_TABLE_COLUMN_DESCRIPTION:
        case PROP_TABLE_COLUMN_HEADER:
        case PROP_TABLE_ROW_HEADER:
        case PROP_TABLE_ROW_DESCRIPTION:
            g_value_init(&values.new_value, G_TYPE_INT);
            g_value_set_int(&values.new_value,
                            *NS_REINTERPRET_CAST(gint *,
                                                 pAtkPropChange->newvalue));
            break;
  
            //Perhaps need more cases in the future
        default:
            g_value_init (&values.old_value, G_TYPE_POINTER);
            g_value_set_pointer (&values.old_value, pAtkPropChange->oldvalue);
            g_value_init (&values.new_value, G_TYPE_POINTER);
            g_value_set_pointer (&values.new_value, pAtkPropChange->newvalue);
        }

        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              g_strconcat("property_change::",
                                          values.property_name),
                              &values, NULL);

        break;

    case nsIAccessibleEventListener::EVENT_ATK_SELECTION_CHANGE:
        MAI_LOG_DEBUG(("Receiving event EVENT_ATK_SELECTION_CHANGE\n\n"));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "selection_changed");
        break;

    case nsIAccessibleEventListener::EVENT_ATK_TEXT_CHANGE:
        AtkTextChange *pAtkTextChange;

        MAI_LOG_DEBUG(("Receiving event EVENT_ATK_TEXT_CHANGE\n\n"));
        if (!aEventData)
            return NS_ERROR_FAILURE;

        pAtkTextChange = NS_REINTERPRET_CAST(AtkTextChange *, aEventData);
        g_signal_emit_by_name (ATK_OBJECT(pMaiObject->GetAtkObject()),
                               pAtkTextChange->add ? \
                               "text_changed:insert":"text_changed::delete",
                               pAtkTextChange->start,
                               pAtkTextChange->length);

        break;

    case nsIAccessibleEventListener::EVENT_ATK_TEXT_SELECTION_CHANGE:
        MAI_LOG_DEBUG(("Receiving event EVENT_ATK_TEXT_SELECTION_CHANGE\n\n"));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "text_selection_changed");
        break;

    case nsIAccessibleEventListener::EVENT_ATK_TEXT_CARET_MOVE:
        MAI_LOG_DEBUG(("Receiving event EVENT_ATK_TEXT_CARET_MOVE\n\n"));
        if (!aEventData)
            return NS_ERROR_FAILURE;

        MAI_LOG_DEBUG(("Caret postion: %d", *(gint *)aEventData ));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "text_caret_moved",
                              // Curent caret position
                              *(gint *)aEventData);
        break;

    case nsIAccessibleEventListener::EVENT_ATK_TABLE_MODEL_CHANGE:
        MAI_LOG_DEBUG(("Receiving event EVENT_ATK_TABLE_MODEL_CHANGE\n\n"));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "model_changed");
        break;

    case nsIAccessibleEventListener::EVENT_ATK_TABLE_ROW_INSERT:
        MAI_LOG_DEBUG(("Receiving event EVENT_ATK_TABLE_ROW_INSERT\n\n"));
        if (!aEventData)
            return NS_ERROR_FAILURE;

        pAtkTableChange = NS_REINTERPRET_CAST(AtkTableChange *, aEventData);

        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "row_inserted",
                              // After which the rows are inserted
                              pAtkTableChange->index,
                              // The number of the inserted
                              pAtkTableChange->count);
        break;
        
    case nsIAccessibleEventListener::EVENT_ATK_TABLE_ROW_DELETE:
        MAI_LOG_DEBUG(("Receiving event EVENT_ATK_TABLE_ROW_DELETE\n\n"));
        if (!aEventData)
            return NS_ERROR_FAILURE;

        pAtkTableChange = NS_REINTERPRET_CAST(AtkTableChange *, aEventData);

        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "row_deleted",
                              // After which the rows are deleted
                              pAtkTableChange->index,
                              // The number of the deleted
                              pAtkTableChange->count);
        break;
        
    case nsIAccessibleEventListener::EVENT_ATK_TABLE_ROW_REORDER:
        MAI_LOG_DEBUG(("Receiving event EVENT_ATK_TABLE_ROW_REORDER\n\n"));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "row_reordered");
        break;

    case nsIAccessibleEventListener::EVENT_ATK_TABLE_COLUMN_INSERT:
        MAI_LOG_DEBUG(("Receiving event EVENT_ATK_TABLE_COLUMN_INSERT\n\n"));
        if (!aEventData)
            return NS_ERROR_FAILURE;

        pAtkTableChange = NS_REINTERPRET_CAST(AtkTableChange *, aEventData);

        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "column_inserted",
                              // After which the columns are inserted
                              pAtkTableChange->index,
                              // The number of the inserted
                              pAtkTableChange->count);
        break;

    case nsIAccessibleEventListener::EVENT_ATK_TABLE_COLUMN_DELETE:
        MAI_LOG_DEBUG(("Receiving event EVENT_ATK_TABLE_COLUMN_DELETE\n\n"));
        if (!aEventData)
            return NS_ERROR_FAILURE;

        pAtkTableChange = NS_REINTERPRET_CAST(AtkTableChange *, aEventData);

        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "column_deleted",
                              // After which the columns are deleted
                              pAtkTableChange->index,
                              // The number of the deleted
                              pAtkTableChange->count);
        break;
        
    case nsIAccessibleEventListener::EVENT_ATK_TABLE_COLUMN_REORDER:
        MAI_LOG_DEBUG(("Receiving event EVENT_ATK_TABLE_COLUMN_REORDER\n\n"));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "column_reordered");
        break;

    case nsIAccessibleEventListener::EVENT_ATK_VISIBLE_DATA_CHANGE:
        MAI_LOG_DEBUG(("Receiving event EVENT_ATK_VISIBLE_DATA_CHANGE\n\n"));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "visible_data_changed");
        break;
        
        // Is a superclass of ATK event children_changed
    case nsIAccessibleEventListener::EVENT_REORDER:
        AtkChildrenChange *pAtkChildrenChange;

        MAI_LOG_DEBUG(("Receiving event EVENT_REORDER\n\n"));
        if (!aEventData)
            return NS_ERROR_FAILURE;

        pAtkChildrenChange = NS_REINTERPRET_CAST(AtkChildrenChange *,
                                                 aEventData);

        MaiObject *aChildMaiObject;
        aChildMaiObject = CreateMaiObjectFor(pAtkChildrenChange->child);
        if (!aChildMaiObject)
            g_signal_emit_by_name (ATK_OBJECT(pMaiObject->GetAtkObject()),
                                   pAtkChildrenChange->add ? \
                                   "children_changed::add" : \
                                   "children_changed::remove",
                                   pAtkChildrenChange->index,
                                   ATK_OBJECT(aChildMaiObject->GetAtkObject()),
                                   NULL);
        
        break;

        /*
         * Because dealing with menu is very different between nsIAccessible
         * and ATK, and the menu activity is important, specially transfer the
         * following two event.
         * Need more verification by AT test.
         */
    case nsIAccessibleEventListener::EVENT_MENUSTART:
        MAI_LOG_DEBUG(("Receiving event EVENT_MENUSTART\n\n"));
        atk_focus_tracker_notify(ATK_OBJECT(pMaiObject->GetAtkObject()));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "selection_changed");
        break;

    case nsIAccessibleEventListener::EVENT_MENUEND:
        MAI_LOG_DEBUG(("Receiving event EVENT_MENUEND\n\n"));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "selection_changed");
        break;

    default:
        // Don't transfer others
        MAI_LOG_DEBUG(("Receiving a event not need to be translated\n\n"));
        break;
    }

    return NS_OK;
}

/******************************************************************
 * MaiObject *
 * MaiTopLevel::CreateMaiObjectFor(nsIAccessible* aAccessible)
 *
 ******************************************************************/
MaiObject *
MaiTopLevel::CreateMaiObjectFor(nsIAccessible *aAccessible)
{
    return MaiWidget::CreateAndCache(aAccessible);
}

/*static*/
//////////////////////////////////////////////////////////////////////
// See the comments in
// MaiWidget::CreateAndCache(nsIAccessible *aAcc);
/////////////////////////////////////////////////////////////////////
MaiTopLevel *
MaiTopLevel::CreateAndCache(nsIAccessible *aAcc)
{
    if (!aAcc)
        return NULL;

    MaiCache *maiCache = mai_get_cache();
    if (!maiCache)
        return NULL;

    MaiTopLevel *retWidget =
        NS_STATIC_CAST(MaiTopLevel*, maiCache->Fetch(aAcc));
    //there is a maiWidget in cache for the nsIAccessible already.
    if (retWidget) {
        MAI_LOG_DEBUG(("MaiTopLevel::CreateAndCache, already added\n"));
        return retWidget;
    }

    //create one, and cache it.
    retWidget = new MaiTopLevel(aAcc);
    NS_ASSERTION(retWidget, "Fail to create mai object");
    MAI_LOG_DEBUG(("MaiTopLevel::CreateAndCache, new one created\n"));

    maiCache->Add(retWidget);
    //cache should have add ref, release ours
    g_object_unref(retWidget->GetAtkObject());

    return retWidget;
}

/* static */
AtkStateType
MaiTopLevel::TranslateAState(PRUint32 aAccState)
{
    switch (aAccState) {
    case nsIAccessible::STATE_SELECTED:
        return ATK_STATE_SELECTED;
    case nsIAccessible::STATE_FOCUSED:
        return ATK_STATE_FOCUSED;
    case nsIAccessible::STATE_PRESSED:
        return ATK_STATE_PRESSED;
    case nsIAccessible::STATE_CHECKED:
        return ATK_STATE_CHECKED;
    case nsIAccessible::STATE_EXPANDED:
        return ATK_STATE_EXPANDED;
    case nsIAccessible::STATE_COLLAPSED:
        return ATK_STATE_EXPANDABLE;
        // The control can't accept input at this time
    case nsIAccessible::STATE_BUSY:
        return ATK_STATE_BUSY;
    case nsIAccessible::STATE_FOCUSABLE:
        return ATK_STATE_FOCUSABLE;
    case nsIAccessible::STATE_SELECTABLE:
        return ATK_STATE_SELECTABLE;
    case nsIAccessible::STATE_SIZEABLE:
        return ATK_STATE_RESIZABLE;
    case nsIAccessible::STATE_MULTISELECTABLE:
        return ATK_STATE_MULTISELECTABLE;

#if 0
        // The following two state need to deal specially
    case nsIAccessible::STATE_INVISIBLE:
        return !ATK_STATE_VISIBLE;

    case nsIAccessible::STATE_UNAVAILABLE:
        return !ATK_STATE_ENABLED;
#endif

        // The following state is
        // Extended state flags (for non-MSAA, for Java and Gnome/ATK support)
        // They are only the states that are not already  mapped in MSAA
        // See www.accessmozilla.org/article.php?sid=11 for information on the
        // mappings between accessibility API state

    case nsIAccessible::STATE_ACTIVE:
        return ATK_STATE_ACTIVE;
    case nsIAccessible::STATE_EXPANDABLE:
        return ATK_STATE_EXPANDABLE;
#if 0
        // Need change definitions in nsIAccessible.idl to avoid
        // duplicate value
    case nsIAccessible::STATE_MODAL:
        return ATK_STATE_MODAL;
#endif
    case nsIAccessible::STATE_MULTI_LINE:
        return ATK_STATE_MULTI_LINE;
    case nsIAccessible::STATE_SENSITIVE:
        return ATK_STATE_SENSITIVE;
    case nsIAccessible::STATE_RESIZABLE:
        return ATK_STATE_RESIZABLE;
    case nsIAccessible::STATE_SHOWING:
        return ATK_STATE_SHOWING;
    case nsIAccessible::STATE_SINGLE_LINE:
        return ATK_STATE_SINGLE_LINE;
    case nsIAccessible::STATE_TRANSIENT:
        return ATK_STATE_TRANSIENT;
    case nsIAccessible::STATE_VERTICAL:
        return ATK_STATE_VERTICAL;
    default:
        return ATK_STATE_INVALID;
    }
}
