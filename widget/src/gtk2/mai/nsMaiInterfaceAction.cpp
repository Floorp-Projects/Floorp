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

#include "nsMaiInterfaceAction.h"

/* helpers */
static MaiInterfaceAction *getAction(AtkAction *aAction);

G_BEGIN_DECLS

static void interfaceInitCB(AtkActionIface *aIface);
/* action interface callbacks */
static gboolean doActionCB(AtkAction *aAction, gint aActionIndex);
static gint getActionCountCB(AtkAction *aAction);
static const gchar *getDescriptionCB(AtkAction *aAction, gint aActionIndex);
static const gchar *getNameCB(AtkAction *aAction, gint aActionIndex);
static const gchar *getKeybindingCB(AtkAction *aAction, gint aActionIndex);
static gboolean     setDescriptionCB(AtkAction *aAction, gint aActionIndex,
                                     const gchar *aDesc);
G_END_DECLS

MaiInterfaceAction::MaiInterfaceAction(MaiWidget *aMaiWidget):
    MaiInterface(aMaiWidget)
{
}

MaiInterfaceAction::~MaiInterfaceAction()
{
}

MaiInterfaceType
MaiInterfaceAction::GetType()
{
    return MAI_INTERFACE_ACTION;
}

const GInterfaceInfo *
MaiInterfaceAction::GetInterfaceInfo()
{
    static const GInterfaceInfo atk_if_action_info = {
        (GInterfaceInitFunc)interfaceInitCB,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };
    return &atk_if_action_info;
}

gboolean
MaiInterfaceAction::DoAction(gint aActionIndex)
{
    nsIAccessible *accessible = GetNSAccessible();
    g_return_val_if_fail(accessible != NULL, FALSE);

    nsresult rv = accessible->AccDoAction(aActionIndex);
    return (NS_FAILED(rv)) ? FALSE : TRUE;
}

gint
MaiInterfaceAction::GetActionCount()
{
    nsIAccessible *accessible = GetNSAccessible();
    g_return_val_if_fail(accessible != NULL, 0);

    PRUint8 num = 0;
    nsresult rv = accessible->GetAccNumActions(&num);
    return (NS_FAILED(rv)) ? 0 : NS_STATIC_CAST(gint, num);
}

const gchar *
MaiInterfaceAction::GetDescription(gint aActionIndex)
{
    // the interface in nsIAccessibleAction is empty
    // use getName as default description
    return GetName(aActionIndex);
}

const gchar *
MaiInterfaceAction::GetName(gint aActionIndex)
{
    nsIAccessible *accessible = GetNSAccessible();
    g_return_val_if_fail(accessible != NULL, NULL);

    if (!mName.IsEmpty())
        return mName.get();

    nsAutoString autoStr;
    nsresult rv = accessible->GetAccActionName(aActionIndex, autoStr);
    if (NS_FAILED(rv))
        return NULL;

    mName = NS_ConvertUCS2toUTF8(autoStr);
    return mName.get();
}

const gchar *
MaiInterfaceAction::GetKeybinding(gint aActionIndex)
{
    //return all Keybindings including accesskey and shortcut
    
    nsIAccessible *accessible = GetNSAccessible();
    g_return_val_if_fail(accessible != NULL, NULL);

    if (!mKeyBinding.IsEmpty())
        return mKeyBinding.get();

    nsAutoString allKeybinding;

    //get accesskey
    nsAutoString accessKey;
    nsresult rv = accessible->GetAccKeyboardShortcut(accessKey);

    if (NS_SUCCEEDED(rv) && !accessKey.IsEmpty()) {
        nsCOMPtr<nsIAccessible> parentAccessible;
        accessible->GetAccParent(getter_AddRefs(parentAccessible));
        if (parentAccessible) {
            PRUint32 role;
            parentAccessible->GetAccRole(&role);

            if (role == ATK_ROLE_MENU_BAR) {
                //it is topmenu, change from "Alt+f" to "f;<Alt>f"
                nsAutoString rightChar;
                accessKey.Right(rightChar, 1);
                allKeybinding = rightChar + NS_LITERAL_STRING(";<Alt>") +
                                rightChar;
            }
            else if ((role == ATK_ROLE_MENU) || (role == ATK_ROLE_MENU_ITEM)) {
                //it is submenu, change from "s" to "s;<Alt>fs"
                nsAutoString allKey = accessKey;
                nsCOMPtr<nsIAccessible> grandParentAcc = parentAccessible;

                while ((grandParentAcc) && (role != ATK_ROLE_MENU_BAR)) {
                    nsAutoString grandParentKey;
                    grandParentAcc->GetAccKeyboardShortcut(grandParentKey);

                    if (!grandParentKey.IsEmpty()) {
                        nsAutoString rightChar;
                        grandParentKey.Right(rightChar, 1);
                        allKey = rightChar + allKey;
                    }

                    nsCOMPtr<nsIAccessible> tempAcc = grandParentAcc;
                    tempAcc->GetAccParent(getter_AddRefs(grandParentAcc));
                    if (grandParentAcc)
                        grandParentAcc->GetAccRole(&role);
                }
                allKeybinding = accessKey + NS_LITERAL_STRING(";<Alt>") +
                                allKey;
            }
        }
        else {
            //default process, rarely happens.
            nsAutoString rightChar;
            accessKey.Right(rightChar, 1);
            allKeybinding = rightChar + NS_LITERAL_STRING(";<Alt>") + rightChar;
        }
    }
    else  //don't have accesskey
        allKeybinding = NS_LITERAL_STRING(";");

    //get shortcut
    nsAutoString keyBinding, subShortcut;
    rv = accessible->GetAccKeybinding(keyBinding);

    if (NS_SUCCEEDED(rv) && !keyBinding.IsEmpty()) {
        //change the shortcut from "Ctrl+Shift+L" to "<Control><Shift>L"
        PRInt32 oldPos, curPos=0;
        while ((curPos != -1) && (curPos < (PRInt32)keyBinding.Length())) {
            oldPos = curPos;
            nsAutoString subString;
            curPos = keyBinding.FindChar('+', oldPos);
            if (curPos == -1) {
                keyBinding.Mid(subString, oldPos, keyBinding.Length() - oldPos);
                subShortcut += subString;
            }
            else {
                keyBinding.Mid(subString, oldPos, curPos - oldPos);
      
                //change "Ctrl" to "Control"
                if (subString.EqualsIgnoreCase("ctrl"))
                    subString = NS_LITERAL_STRING("Control");
      
                subShortcut += NS_LITERAL_STRING("<") + subString +
                               NS_LITERAL_STRING(">");
                curPos++;
            }
        }
    }

    allKeybinding += NS_LITERAL_STRING(";") + subShortcut;
    mKeyBinding = NS_ConvertUCS2toUTF8(allKeybinding);

    return mKeyBinding.get();
}

gboolean
MaiInterfaceAction::SetDescription(gint aActionIndex, const gchar *aDesc)
{
    nsIAccessible *accessible = GetNSAccessible();
    g_return_val_if_fail(accessible != NULL, FALSE);

    /* this is not supported in nsIAccessible yet */

    return FALSE;
}

/* static functions */

/* do general checking for callbacks functions
 * return the MaiInterfaceAction extracted from atk action
 */
MaiInterfaceAction *
getAction(AtkAction *aAction)
{
    g_return_val_if_fail(MAI_IS_ATK_WIDGET(aAction), NULL);
    MaiWidget *maiWidget = (MaiWidget*)(MAI_ATK_OBJECT(aAction)->maiObject);
    g_return_val_if_fail(maiWidget != NULL, NULL);
    g_return_val_if_fail(maiWidget->GetAtkObject() == (AtkObject*)aAction,
                         NULL);
    MaiInterfaceAction *maiInterfaceAction = (MaiInterfaceAction*)
        maiWidget->GetMaiInterface(MAI_INTERFACE_ACTION);
    return maiInterfaceAction;
}

void
interfaceInitCB(AtkActionIface *aIface)
{
    g_return_if_fail(aIface != NULL);

    aIface->do_action = doActionCB;
    aIface->get_n_actions = getActionCountCB;
    aIface->get_description = getDescriptionCB;
    aIface->get_keybinding = getKeybindingCB;
    aIface->get_name = getNameCB;
    aIface->set_description = setDescriptionCB;
}

gboolean
doActionCB(AtkAction *aAction, gint aActionIndex)
{
    MaiInterfaceAction *maiInterfaceAction = getAction(aAction);
    if (!maiInterfaceAction)
        return FALSE;
    return maiInterfaceAction->DoAction(aActionIndex);
}

gint
getActionCountCB(AtkAction *aAction)
{
    MaiInterfaceAction *maiInterfaceAction = getAction(aAction);
    if (!maiInterfaceAction)
        return 0;
    return maiInterfaceAction->GetActionCount();
}

const gchar *
getDescriptionCB(AtkAction *aAction, gint aActionIndex)
{
    MaiInterfaceAction *maiInterfaceAction = getAction(aAction);
    if (!maiInterfaceAction)
        return NULL;
    return maiInterfaceAction->GetDescription(aActionIndex);
}

const gchar *
getNameCB(AtkAction *aAction, gint aActionIndex)
{
    MaiInterfaceAction *maiInterfaceAction = getAction(aAction);
    if (!maiInterfaceAction)
        return NULL;
    return maiInterfaceAction->GetName(aActionIndex);
}

const gchar *
getKeybindingCB(AtkAction *aAction, gint aActionIndex)
{
    MaiInterfaceAction *maiInterfaceAction = getAction(aAction);
    if (!maiInterfaceAction)
        return NULL;
    return maiInterfaceAction->GetKeybinding(aActionIndex);
}

gboolean
setDescriptionCB(AtkAction *aAction, gint aActionIndex,
                 const gchar *aDesc)
{
    MaiInterfaceAction *maiInterfaceAction = getAction(aAction);
    if (!maiInterfaceAction)
        return FALSE;
    return maiInterfaceAction->SetDescription(aActionIndex, aDesc);
}
