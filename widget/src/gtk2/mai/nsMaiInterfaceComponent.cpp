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

#include "nsMaiInterfaceComponent.h"

/* helpers */
static MaiInterfaceComponent *getComponent(AtkComponent *aIface);

G_BEGIN_DECLS

/* component interface callbacks */
static void interfaceInitCB(AtkComponentIface *aIface);
static AtkObject *refAccessibleAtPointCB(AtkComponent *aComponent,
                                         gint aAccX, gint aAccY,
                                         AtkCoordType aCoordType);
static void getExtentsCB(AtkComponent *aComponent,
                         gint *aAccX, gint *aAccY,
                         gint *aAccWidth, gint *aAccHeight,
                         AtkCoordType aCoordType);
/* the "contains", "get_position", "get_size" can take advantage of
 * "get_extents", there is no need to implement them now.
 */
static gboolean grabFocusCB(AtkComponent *aComponent);

/* what are missing now for atk component */

/* ==================================================
 * add_focus_handler
 * remove_focus_handler
 * set_extents
 * set_position
 * set_size
 * get_layer
 * get_mdi_zorder
 * ==================================================
 */
G_END_DECLS

MaiInterfaceComponent::MaiInterfaceComponent(MaiWidget *aMaiWidget):
    MaiInterface(aMaiWidget)
{
}

MaiInterfaceComponent::~MaiInterfaceComponent()
{
}

MaiInterfaceType
MaiInterfaceComponent::GetType()
{
    return MAI_INTERFACE_COMPONENT;
}

const GInterfaceInfo *
MaiInterfaceComponent::GetInterfaceInfo()
{
    static const GInterfaceInfo atk_if_component_info = {
        (GInterfaceInitFunc)interfaceInitCB,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };
    return &atk_if_component_info;
}

MaiObject *
MaiInterfaceComponent::RefAccessibleAtPoint(gint aAccX, gint aAccY,
                                            AtkCoordType aCoordType)
{
    nsIAccessible *accessible = GetNSAccessible();
    g_return_val_if_fail(accessible != NULL, NULL);

    // or ATK_XY_SCREEN  what is definition this in nsIAccessible?
    if (aCoordType == ATK_XY_WINDOW) {
        /* deal with the coord type */
    }

    nsCOMPtr<nsIAccessible> pointAcc;
    nsresult rv = accessible->AccGetAt(aAccX, aAccY, getter_AddRefs(pointAcc));
    if (NS_FAILED(rv))
        return NULL;

    /* ??? when to free ??? */
    MaiWidget *maiWidget = new MaiWidget(pointAcc);
    return maiWidget;
}

void
MaiInterfaceComponent::GetExtents(gint *aAccX,
                                  gint *aAccY,
                                  gint *aAccWidth,
                                  gint *aAccHeight,
                                  AtkCoordType aCoordType)
{
    nsIAccessible *accessible = GetNSAccessible();
    g_return_if_fail(accessible != NULL);

    PRInt32 nsAccX, nsAccY, nsAccWidth, nsAccHeight;
    nsresult rv = accessible->AccGetBounds(&nsAccX, &nsAccY,
                                           &nsAccWidth, &nsAccHeight);
    if (NS_FAILED(rv))
        return;

    // or ATK_XY_SCREEN  what is definition this in nsIAccessible?
    if (aCoordType == ATK_XY_WINDOW) {
        /* deal with the coord type */
    }

    *aAccX = nsAccX;
    *aAccY = nsAccY;
    *aAccWidth = nsAccWidth;
    *aAccHeight = nsAccHeight;
}

gboolean
MaiInterfaceComponent::GrabFocus()
{
    nsIAccessible *accessible = GetNSAccessible();
    g_return_val_if_fail(accessible != NULL, FALSE);

    nsresult rv = accessible->AccTakeFocus();
    return (NS_FAILED(rv)) ? FALSE : TRUE;
}

/* static functions */

/* do general checking for callbacks functions
 * return the MaiInterfaceComponent extracted from atk component
 */
MaiInterfaceComponent *
getComponent(AtkComponent *aComponent)
{
    g_return_val_if_fail(MAI_IS_ATK_WIDGET(aComponent), NULL);
    MaiWidget *maiWidget = (MaiWidget*)(MAI_ATK_OBJECT(aComponent)->maiObject);
    g_return_val_if_fail(maiWidget != NULL, NULL);
    g_return_val_if_fail(maiWidget->GetAtkObject() == (AtkObject*)aComponent,
                         NULL);
    MaiInterfaceComponent *maiInterfaceComponent = (MaiInterfaceComponent*)
        maiWidget->GetMaiInterface(MAI_INTERFACE_COMPONENT);
    return maiInterfaceComponent;
}

void
interfaceInitCB(AtkComponentIface *aIface)
{
    g_return_if_fail(aIface != NULL);

    /*
     * Use default implementation in atk for contains, get_position,
     * and get_size
     */
    aIface->ref_accessible_at_point = refAccessibleAtPointCB;
    aIface->get_extents = getExtentsCB;
    aIface->grab_focus = grabFocusCB;
}

AtkObject *
refAccessibleAtPointCB(AtkComponent *aComponent,
                       gint aAccX, gint aAccY,
                       AtkCoordType aCoordType)
{
    MaiInterfaceComponent *maiInterfaceComponent = getComponent(aComponent);
    if (!maiInterfaceComponent)
        return NULL;

    MaiObject *maiObj =
        maiInterfaceComponent->RefAccessibleAtPoint(aAccX, aAccY, aCoordType);
    if (!maiObj)
        return NULL;

    AtkObject *atkObj = maiObj->GetAtkObject();
    if (!atkObj)
        return NULL;
    g_object_ref(atkObj);
    return atkObj;
}

void
getExtentsCB(AtkComponent *aComponent,
             gint *aAccX,
             gint *aAccY,
             gint *aAccWidth,
             gint *aAccHeight,
             AtkCoordType aCoordType)
{
    MaiInterfaceComponent *maiInterfaceComponent = getComponent(aComponent);
    if (maiInterfaceComponent)
        maiInterfaceComponent->GetExtents(aAccX, aAccY, aAccWidth, aAccHeight,
                                          aCoordType);
}

gboolean
grabFocusCB(AtkComponent *aComponent)
{
    MaiInterfaceComponent *maiInterfaceComponent = getComponent(aComponent);
    if (!maiInterfaceComponent)
        return FALSE;
    return maiInterfaceComponent->GrabFocus();
}
