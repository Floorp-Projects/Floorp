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

#ifdef MAI_LOGGING
    static PRUint32 sMaiTopCount = 0;
#endif

/* Implementation file */
NS_IMPL_ISUPPORTS1(MaiTopLevel, nsIAccessibleEventListener)

MaiTopLevel::MaiTopLevel(nsIAccessible *aAcc):MaiWidget(aAcc)
{
    NS_INIT_ISUPPORTS();

#ifdef MAI_LOGGING
    ++sMaiTopCount;
#endif
    MAI_LOG_DEBUG(("MaiTopLevel+++>%d, mycount=%d, acc=0x%x\n",
                   sMaiTopCount, mRefCnt, aAcc));

    nsCOMPtr<nsIAccessibleEventReceiver>
        receiver(do_QueryInterface(mAccessible));
    if (receiver)
        receiver->AddAccessibleEventListener(this);
}

MaiTopLevel::~MaiTopLevel()
{
    nsCOMPtr<nsIAccessibleEventReceiver>
        receiver(do_QueryInterface(mAccessible));

#ifdef MAI_LOGGING
    --sMaiTopCount;
#endif

    MAI_LOG_DEBUG(("MaiTopLevel--->%d, mycount=%d, acc=0x%x\n",
                   sMaiTopCount, mRefCnt, GetNSAccessible()));

    if (receiver)
        receiver->RemoveAccessibleEventListener();
}

void
MaiTopLevel::Finalize(void)
{
    //here we know that the action is originated from the MaiAtkObject,
    //and the MaiAtkObject itself will be destroyed.
    //Mark MaiAtkObject to nil
    mMaiAtkObject = NULL;

    //release the refnt increased by MaiTopLevel::Create
    NS_RELEASE_THIS();
}

//////////////////////////////////////////////////////////////////////
// See the comments in
// MaiWidget::Create(nsIAccessible *aAcc);
/////////////////////////////////////////////////////////////////////
MaiTopLevel *
MaiTopLevel::Create(nsIAccessible *aAcc)
{
    if (!aAcc)
        return NULL;

    MaiTopLevel *maiTopLevel = NS_REINTERPRET_CAST(MaiTopLevel *,
                                                   MaiHashTable::Lookup(aAcc));
    if (!maiTopLevel) {
        maiTopLevel = new MaiTopLevel(aAcc);
        NS_ASSERTION(maiTopLevel, "Fail to create Object\n");

        // the refcnt will be released in MaiTopLevel::Finalize when the
        // related maiAtkObject ready to destroy.
        NS_IF_ADDREF(maiTopLevel);
        MaiHashTable::Add(maiTopLevel);
    }
    else {
        g_object_ref(maiTopLevel->GetAtkObject());
    }
    return maiTopLevel;
}

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
    MaiTopLevel *retWidget = MaiTopLevel::Create(aAcc);
    maiCache->Add(retWidget);

    return retWidget;
}

NS_IMETHODIMP
MaiTopLevel::HandleEvent(PRUint32 aEvent, nsIAccessible *aAccessible,
                         AccessibleEventData * aEventData)
{
    nsresult rv = NS_ERROR_FAILURE;
    MaiWidget *pMaiObject = NULL;
    AtkTableChange * pAtkTableChange;

    MAI_LOG_DEBUG(("\n\nReceived event: aEvent=%u, obj=0x%x, data=0x%x \n",
                   aEvent, aAccessible, aEventData));

    pMaiObject = CreateMaiWidgetFor(aAccessible);

    if (!pMaiObject) {
        MAI_LOG_DEBUG(("\n\nFail to Create MaiObject for  obj=0x%x\n",
                       aAccessible));
        return NS_ERROR_OUT_OF_MEMORY;
    }

    MaiObject *oldMaiObj = NULL, *newMaiObj = NULL;
    switch (aEvent) {
    case nsIAccessibleEventListener::EVENT_FOCUS:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_FOCUS\n"));
        atk_focus_tracker_notify(ATK_OBJECT(pMaiObject->GetAtkObject()));
        rv = NS_OK;
        break;

    case nsIAccessibleEventListener::EVENT_STATE_CHANGE:
        AtkStateChange *pAtkStateChange;
        AtkStateType atkState;

        MAI_LOG_DEBUG(("\n\nReceived: EVENT_STATE_CHANGE\n"));
        if (!aEventData)
            break;

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
        rv = NS_OK;
        break;
      
        /*
         * More complex than I ever thought.
         * Need handle them separately.
         */
    case nsIAccessibleEventListener::EVENT_ATK_PROPERTY_CHANGE :
        AtkPropertyChange *pAtkPropChange;
        AtkPropertyValues values;

        MAI_LOG_DEBUG(("\n\nReceived: EVENT_ATK_PROPERTY_CHANGE\n"));
        if (!aEventData)
            break;

        pAtkPropChange = NS_REINTERPRET_CAST(AtkPropertyChange *, aEventData);
        values.property_name = pAtkPropertyNameArray[pAtkPropChange->type];
        
        MAI_LOG_DEBUG(("\n\nthe type of EVENT_ATK_PROPERTY_CHANGE: %d\n\n",
                       pAtkPropChange->type));

        switch (pAtkPropChange->type) {
        case PROP_TABLE_CAPTION:
        case PROP_TABLE_SUMMARY:

            if (pAtkPropChange->oldvalue)
                oldMaiObj = CreateMaiWidgetFor(NS_REINTERPRET_CAST
                                               (nsIAccessible *,
                                                pAtkPropChange->oldvalue));

            if (pAtkPropChange->newvalue)
                newMaiObj = CreateMaiWidgetFor(NS_REINTERPRET_CAST
                                               (nsIAccessible *,
                                                pAtkPropChange->newvalue));

            if (oldMaiObj && newMaiObj) {
                g_value_init(&values.old_value, G_TYPE_POINTER);
                g_value_set_pointer(&values.old_value,
                                    ATK_OBJECT(oldMaiObj->GetAtkObject()));
                g_value_init(&values.new_value, G_TYPE_POINTER);
                g_value_set_pointer(&values.new_value,
                                    ATK_OBJECT(newMaiObj->GetAtkObject()));
                rv = NS_OK;
            }
            if (oldMaiObj)
                g_object_unref(oldMaiObj->GetAtkObject());
            if (newMaiObj)
                g_object_unref(newMaiObj->GetAtkObject());

            break;

        case PROP_TABLE_COLUMN_DESCRIPTION:
        case PROP_TABLE_COLUMN_HEADER:
        case PROP_TABLE_ROW_HEADER:
        case PROP_TABLE_ROW_DESCRIPTION:
            g_value_init(&values.new_value, G_TYPE_INT);
            g_value_set_int(&values.new_value,
                            *NS_REINTERPRET_CAST(gint *,
                                                 pAtkPropChange->newvalue));
            rv = NS_OK;
            break;
  
            //Perhaps need more cases in the future
        default:
            g_value_init (&values.old_value, G_TYPE_POINTER);
            g_value_set_pointer (&values.old_value, pAtkPropChange->oldvalue);
            g_value_init (&values.new_value, G_TYPE_POINTER);
            g_value_set_pointer (&values.new_value, pAtkPropChange->newvalue);
            rv = NS_OK;
        }
        if (NS_SUCCEEDED(rv))
            g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                                  g_strconcat("property_change::",
                                              values.property_name),
                                  &values, NULL);

        break;

    case nsIAccessibleEventListener::EVENT_ATK_SELECTION_CHANGE:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_ATK_SELECTION_CHANGE\n"));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "selection_changed");
        rv = NS_OK;
        break;

    case nsIAccessibleEventListener::EVENT_ATK_TEXT_CHANGE:
        AtkTextChange *pAtkTextChange;

        MAI_LOG_DEBUG(("\n\nReceived: EVENT_ATK_TEXT_CHANGE\n"));
        if (!aEventData)
            break;

        pAtkTextChange = NS_REINTERPRET_CAST(AtkTextChange *, aEventData);
        g_signal_emit_by_name (ATK_OBJECT(pMaiObject->GetAtkObject()),
                               pAtkTextChange->add ? \
                               "text_changed::insert":"text_changed::delete",
                               pAtkTextChange->start,
                               pAtkTextChange->length);
        rv = NS_OK;
        break;

    case nsIAccessibleEventListener::EVENT_ATK_TEXT_SELECTION_CHANGE:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_ATK_TEXT_SELECTION_CHANGE\n"));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "text_selection_changed");
        rv = NS_OK;
        break;

    case nsIAccessibleEventListener::EVENT_ATK_TEXT_CARET_MOVE:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_ATK_TEXT_CARET_MOVE\n"));
        if (!aEventData)
            break;

        MAI_LOG_DEBUG(("\n\nCaret postion: %d", *(gint *)aEventData ));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "text_caret_moved",
                              // Curent caret position
                              *(gint *)aEventData);
        rv = NS_OK;
        break;

    case nsIAccessibleEventListener::EVENT_ATK_TABLE_MODEL_CHANGE:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_ATK_TABLE_MODEL_CHANGE\n"));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "model_changed");
        rv = NS_OK;
        break;

    case nsIAccessibleEventListener::EVENT_ATK_TABLE_ROW_INSERT:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_ATK_TABLE_ROW_INSERT\n"));
        if (!aEventData)
            break;

        pAtkTableChange = NS_REINTERPRET_CAST(AtkTableChange *, aEventData);

        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "row_inserted",
                              // After which the rows are inserted
                              pAtkTableChange->index,
                              // The number of the inserted
                              pAtkTableChange->count);
        rv = NS_OK;
        break;
        
    case nsIAccessibleEventListener::EVENT_ATK_TABLE_ROW_DELETE:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_ATK_TABLE_ROW_DELETE\n"));
        if (!aEventData)
            break;

        pAtkTableChange = NS_REINTERPRET_CAST(AtkTableChange *, aEventData);

        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "row_deleted",
                              // After which the rows are deleted
                              pAtkTableChange->index,
                              // The number of the deleted
                              pAtkTableChange->count);
        rv = NS_OK;
        break;
        
    case nsIAccessibleEventListener::EVENT_ATK_TABLE_ROW_REORDER:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_ATK_TABLE_ROW_REORDER\n"));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "row_reordered");
        rv = NS_OK;
        break;

    case nsIAccessibleEventListener::EVENT_ATK_TABLE_COLUMN_INSERT:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_ATK_TABLE_COLUMN_INSERT\n"));
        if (!aEventData)
            break;

        pAtkTableChange = NS_REINTERPRET_CAST(AtkTableChange *, aEventData);

        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "column_inserted",
                              // After which the columns are inserted
                              pAtkTableChange->index,
                              // The number of the inserted
                              pAtkTableChange->count);
        rv = NS_OK;
        break;

    case nsIAccessibleEventListener::EVENT_ATK_TABLE_COLUMN_DELETE:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_ATK_TABLE_COLUMN_DELETE\n"));
        if (!aEventData)
            break;

        pAtkTableChange = NS_REINTERPRET_CAST(AtkTableChange *, aEventData);

        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "column_deleted",
                              // After which the columns are deleted
                              pAtkTableChange->index,
                              // The number of the deleted
                              pAtkTableChange->count);
        rv = NS_OK;
        break;

    case nsIAccessibleEventListener::EVENT_ATK_TABLE_COLUMN_REORDER:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_ATK_TABLE_COLUMN_REORDER\n"));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "column_reordered");
        rv = NS_OK;
        break;

    case nsIAccessibleEventListener::EVENT_ATK_VISIBLE_DATA_CHANGE:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_ATK_VISIBLE_DATA_CHANGE\n"));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "visible_data_changed");
        rv = NS_OK;
        break;

        // Is a superclass of ATK event children_changed
    case nsIAccessibleEventListener::EVENT_REORDER:
        AtkChildrenChange *pAtkChildrenChange;

        MAI_LOG_DEBUG(("\n\nReceived: EVENT_REORDER(children_change)\n"));

        pAtkChildrenChange = NS_REINTERPRET_CAST(AtkChildrenChange *,
                                                 aEventData);
        //update parent's list
        pMaiObject->ChildrenChange(pAtkChildrenChange);

        MaiObject *childMaiObject;
        if (pAtkChildrenChange && pAtkChildrenChange->child &&
            (childMaiObject = CreateMaiWidgetFor(pAtkChildrenChange->child))) {
            g_signal_emit_by_name (ATK_OBJECT(pMaiObject->GetAtkObject()),
                                   pAtkChildrenChange->add ? \
                                   "children_changed::add" : \
                                   "children_changed::remove",
                                   pAtkChildrenChange->index,
                                   ATK_OBJECT(childMaiObject->GetAtkObject()),
                                   NULL);
            g_object_unref(childMaiObject->GetAtkObject());
        }
        else {
            g_signal_emit_by_name (ATK_OBJECT(pMaiObject->GetAtkObject()),
                                   "children_changed",
                                   -1, NULL, NULL);
        }

        rv = NS_OK;
        break;

        /*
         * Because dealing with menu is very different between nsIAccessible
         * and ATK, and the menu activity is important, specially transfer the
         * following two event.
         * Need more verification by AT test.
         */
    case nsIAccessibleEventListener::EVENT_MENUSTART:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_MENUSTART\n"));
        atk_focus_tracker_notify(ATK_OBJECT(pMaiObject->GetAtkObject()));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "selection_changed");
        rv = NS_OK;
        break;

    case nsIAccessibleEventListener::EVENT_MENUEND:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_MENUEND\n"));
        g_signal_emit_by_name(ATK_OBJECT(pMaiObject->GetAtkObject()),
                              "selection_changed");
        rv = NS_OK;
        break;

    default:
        // Don't transfer others
        MAI_LOG_DEBUG(("\n\nReceived an unknown event=0x%u\n", aEvent));
        break;
    }

    if (pMaiObject)
        g_object_unref(pMaiObject->GetAtkObject());
    return rv;
}

/******************************************************************
 * MaiWidget *
 * MaiTopLevel::CreateMaiWidgetFor(nsIAccessible* aAccessible)
 *
 ******************************************************************/
MaiWidget *
MaiTopLevel::CreateMaiWidgetFor(nsIAccessible *aAccessible)
{
    return MaiWidget::CreateAndCache(aAccessible);
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
