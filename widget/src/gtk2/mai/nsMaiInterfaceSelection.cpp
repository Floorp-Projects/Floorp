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
 * Original Author: Silvia Zhao (silvia.zhao@sun.com)
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

#include "nsMaiInterfaceSelection.h"
#include "nsIAccessibleSelectable.h"

/* helpers */
static MaiInterfaceSelection *getSelection(AtkSelection *aIface);

G_BEGIN_DECLS

/* selection interface callbacks */

static void interfaceInitCB(AtkSelectionIface *aIface);
static gboolean addSelectionCB(AtkSelection *aSelection,
                               gint i);
static gboolean clearSelectionCB(AtkSelection *aSelection);
static AtkObject *refSelectionCB(AtkSelection *aSelection,
                                 gint i);
static gint getSelectionCountCB(AtkSelection *aSelection);
static gboolean isChildSelectedCB(AtkSelection *aSelection,
                                  gint i);
static gboolean removeSelectionCB(AtkSelection *aSelection,
                                  gint i);
static gboolean selectAllSelectionCB(AtkSelection *aSelection);

G_END_DECLS

MaiInterfaceSelection::MaiInterfaceSelection(MaiWidget *aMaiWidget):
    MaiInterface(aMaiWidget)
{
}

MaiInterfaceSelection::~MaiInterfaceSelection()
{
}

MaiInterfaceType
MaiInterfaceSelection::GetType()
{
    return MAI_INTERFACE_SELECTION;
}

const GInterfaceInfo *
MaiInterfaceSelection::GetInterfaceInfo()
{
    static const GInterfaceInfo atk_if_selection_info = {
        (GInterfaceInitFunc) interfaceInitCB,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };
    return &atk_if_selection_info;
}

#define MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, retvalue) \
    nsIAccessible *tmpAccess = GetNSAccessible(); \
    nsCOMPtr<nsIAccessibleSelectable> \
        accessIface(do_QueryInterface(tmpAccess)); \
    if (!(accessIface)) \
        return (retvalue)

/*interface virtual functions*/
gboolean
MaiInterfaceSelection::AddSelection(gint i)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessInterfaceSelectable, FALSE);
    return NS_SUCCEEDED(accessInterfaceSelectable->AddSelection(i)) != 0;
}

gboolean
MaiInterfaceSelection::ClearSelection()
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessInterfaceSelectable, FALSE);
    return NS_SUCCEEDED(accessInterfaceSelectable->ClearSelection()) != 0;
}

MaiObject *
MaiInterfaceSelection::RefSelection(gint i)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessInterfaceSelectable, NULL);

    nsCOMPtr<nsIAccessible> aSelection;
    nsresult rv =
        accessInterfaceSelectable->RefSelection(i, getter_AddRefs(aSelection));
    MaiWidget *maiWidget;
    if (NS_SUCCEEDED(rv))
        maiWidget = new MaiWidget(aSelection);
    return (maiWidget != NULL) ? maiWidget : NULL;
}

gint
MaiInterfaceSelection::GetSelectionCount()
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessInterfaceSelectable, 0);

    PRInt32 num = 0;
    nsresult rv = accessInterfaceSelectable->GetSelectionCount(&num);
    return (NS_FAILED(rv)) ? -1 : num;
}

gboolean
MaiInterfaceSelection::IsChildSelected(gint i)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessInterfaceSelectable, FALSE);

    PRBool result = FALSE;
    nsresult rv = accessInterfaceSelectable->IsChildSelected(i, &result);
    return (NS_FAILED(rv)) ? FALSE : result;
}

gboolean
MaiInterfaceSelection::RemoveSelection(gint i)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessInterfaceSelectable, FALSE);

    nsresult rv = accessInterfaceSelectable->RemoveSelection(i);
    return (NS_FAILED(rv)) ? FALSE : TRUE;
}

gboolean
MaiInterfaceSelection::SelectAllSelection()
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessInterfaceSelectable, FALSE);

    PRBool result = FALSE;
    nsresult rv = accessInterfaceSelectable->SelectAllSelection(&result);
    return (NS_FAILED(rv)) ? FALSE : result;
}

/* static functions */

/* do general checking for callbacks functions
 * return the MaiInterfaceSelection extracted from atk selection
 */

MaiInterfaceSelection *
getSelection(AtkSelection *aSelection)
{
    MAI_CHECK_ATK_OBJECT_RETURN_VAL_IF_FAIL(aSelection, NULL);
    g_return_val_if_fail(MAI_IS_ATK_WIDGET(aSelection), NULL);
    MaiWidget *maiWidget = (MaiWidget*)(MAI_ATK_OBJECT(aSelection)->maiObject);
    
    MaiInterfaceSelection *maiInterfaceSelection = (MaiInterfaceSelection*)
        maiWidget->GetMaiInterface(MAI_INTERFACE_SELECTION);
    return maiInterfaceSelection;
}

void
interfaceInitCB(AtkSelectionIface *aIface)
{
    g_return_if_fail(aIface != NULL);
    aIface->add_selection = addSelectionCB;
    aIface->clear_selection = clearSelectionCB;
    aIface->ref_selection = refSelectionCB;
    aIface->get_selection_count = getSelectionCountCB;
    aIface->is_child_selected = isChildSelectedCB;
    aIface->remove_selection = removeSelectionCB;
    aIface->select_all_selection = selectAllSelectionCB;
}

gboolean
addSelectionCB(AtkSelection *aSelection, gint i)
{
    MaiInterfaceSelection *maiInterfaceSelection = getSelection(aSelection);

    return (maiInterfaceSelection) ? maiInterfaceSelection->AddSelection(i) :
        FALSE;

}

gboolean
clearSelectionCB(AtkSelection *aSelection)
{
    MaiInterfaceSelection *maiInterfaceSelection = getSelection(aSelection);

    return (maiInterfaceSelection) ? maiInterfaceSelection->ClearSelection() :
        FALSE;
}

AtkObject *
refSelectionCB(AtkSelection *aSelection, gint i)
{
    MaiInterfaceSelection *maiInterfaceSelection = getSelection(aSelection);

    if (!maiInterfaceSelection)
        return NULL;

    MaiObject *maiObj =
        maiInterfaceSelection->RefSelection(i);
    if (!maiObj)
        return NULL;

    AtkObject *atkObj = maiObj->GetAtkObject();
    if (!atkObj)
        return NULL;
    g_object_ref(atkObj);
    return atkObj;

}

gint
getSelectionCountCB(AtkSelection *aSelection)
{
    MaiInterfaceSelection *maiInterfaceSelection = getSelection(aSelection);
    
    return (maiInterfaceSelection) ?
        maiInterfaceSelection->GetSelectionCount() : -1;
}

gboolean
isChildSelectedCB(AtkSelection *aSelection, gint i)
{
    MaiInterfaceSelection *maiInterfaceSelection = getSelection(aSelection);
    
    return (maiInterfaceSelection) ?
        maiInterfaceSelection->IsChildSelected(i) : FALSE;
}

gboolean
removeSelectionCB(AtkSelection *aSelection, gint i)
{
    MaiInterfaceSelection *maiInterfaceSelection = getSelection(aSelection);

    return (maiInterfaceSelection) ?
        maiInterfaceSelection->RemoveSelection(i) : FALSE;
}

gboolean
selectAllSelectionCB(AtkSelection *aSelection)
{
    MaiInterfaceSelection *maiInterfaceSelection = getSelection(aSelection);

    return (maiInterfaceSelection) ?
        maiInterfaceSelection->SelectAllSelection() : FALSE;
}
