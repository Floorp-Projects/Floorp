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
#include "nsMaiTopLevel.h"
#include "nsIAccessibleEventReceiver.h"

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
MaiTopLevel::HandleEvent(PRUint32 aEvent, nsIAccessible *aAccessible)
{
    MaiObject *maiObject;

    MAI_LOG_DEBUG(("MaiTopLevel::HandleEvent, aEvent=%d\n", aEvent));

    if (mAccessible == aAccessible)
        maiObject = this;
    else
        maiObject = CreateMaiObjectFor(aAccessible);
    if (!maiObject)
        return NS_OK;

    if (aEvent == nsIAccessibleEventListener::EVENT_FOCUS ||
        aEvent == nsIAccessibleEventListener::EVENT_SELECTION ||
        aEvent == nsIAccessibleEventListener::EVENT_SELECTION_ADD ||
        aEvent == nsIAccessibleEventListener::EVENT_SELECTION_REMOVE ||
        aEvent == nsIAccessibleEventListener::EVENT_STATE_CHANGE ||
        aEvent == nsIAccessibleEventListener::EVENT_NAME_CHANGE ||
        aEvent == nsIAccessibleEventListener::EVENT_SELECTION ||
        aEvent == nsIAccessibleEventListener::EVENT_MENUSTART ||
        aEvent == nsIAccessibleEventListener::EVENT_MENUEND ||
        aEvent == nsIAccessibleEventListener::EVENT_MENUPOPUPSTART ||
        aEvent == nsIAccessibleEventListener::EVENT_MENUPOPUPEND) {
        atk_focus_tracker_notify(ATK_OBJECT((maiObject)->GetAtkObject()));
    }
    else
        maiObject->EmitAccessibilitySignal(aEvent);

    /* it is not needed to cache!  */
    /* maiObject is deleted when its atkobject is unrefed to zero! */

    if (mAccessible != aAccessible) {
        GObject *obj = G_OBJECT(maiObject->GetAtkObject());
        g_object_unref(obj);
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
    MaiObject *newMaiObject = NULL;

    newMaiObject = new MaiWidget(aAccessible);

    return newMaiObject;
}

/* virtual functions to ATK callbacks */

/* not implemented yet */
