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

#include "nsMaiInterfaceValue.h"
#include "nsIAccessibleValue.h"

/* helpers */
static MaiInterfaceValue *getValue(AtkValue *aIface);

G_BEGIN_DECLS
    
/*value interface callbacks*/

static void interfaceInitCB(AtkValueIface *aIface);
static void getCurrentValueCB(AtkValue *obj,
                              GValue *value);
static void getMaximumValueCB(AtkValue *obj,
                              GValue *value);
static void getMinimumValueCB(AtkValue *obj,
                              GValue *value);
static gboolean setCurrentValueCB(AtkValue *obj,
                                  const GValue *value);
G_END_DECLS

MaiInterfaceValue::MaiInterfaceValue(MaiWidget *aMaiWidget):
    MaiInterface(aMaiWidget)
{
}

MaiInterfaceValue::~MaiInterfaceValue()
{
}

MaiInterfaceType
MaiInterfaceValue::GetType()
{
    return MAI_INTERFACE_VALUE;
}

const GInterfaceInfo *
MaiInterfaceValue::GetInterfaceInfo()
{
    static const GInterfaceInfo atk_if_value_info = {
        (GInterfaceInitFunc) interfaceInitCB,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };
    return &atk_if_value_info;
}

#define MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, retvalue) \
    nsIAccessible *tmpAccess = GetNSAccessible(); \
    nsCOMPtr<nsIAccessibleValue> \
        accessIface(do_QueryInterface(tmpAccess)); \
    if (!(accessIface)) \
        return (retvalue)

#define MAI_IFACE_RETURN_IF_FAIL(accessIface) \
    nsIAccessible *tmpAccess = GetNSAccessible(); \
    nsCOMPtr<nsIAccessibleValue> \
        accessIface(do_QueryInterface(tmpAccess)); \
    if (!(accessIface)) \
        return

void
MaiInterfaceValue::GetCurrentValue(GValue *value)
{
    memset (value,  0, sizeof (GValue));
    MAI_IFACE_RETURN_IF_FAIL(accessInterfaceValue);
    double accValue;
    if (NS_FAILED(accessInterfaceValue->GetCurrentValue(&accValue)))
        return;
    g_value_init (value, G_TYPE_DOUBLE);
    g_value_set_double (value, accValue);
}

void
MaiInterfaceValue::GetMaximumValue(GValue *value)
{
    memset (value,  0, sizeof (GValue));
    MAI_IFACE_RETURN_IF_FAIL(accessInterfaceValue);
    double accValue;
    if (NS_FAILED(accessInterfaceValue->GetMaximumValue(&accValue)))
        return;
    g_value_init (value, G_TYPE_DOUBLE);
    g_value_set_double (value, accValue);
}

void
MaiInterfaceValue::GetMinimumValue(GValue *value)
{
    memset (value,  0, sizeof (GValue));
    MAI_IFACE_RETURN_IF_FAIL(accessInterfaceValue);
    double accValue;
    if (NS_FAILED(accessInterfaceValue->GetMinimumValue(&accValue)))
        return;
    g_value_init (value, G_TYPE_DOUBLE);
    g_value_set_double (value, accValue);
}

gboolean
MaiInterfaceValue::SetCurrentValue(const GValue *value)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessInterfaceValue, FALSE);

    PRBool aBool;
    double aValue;
    aValue = g_value_get_double (value);
    accessInterfaceValue->SetCurrentValue(aValue, &aBool);
    return aBool;
}

/* static functions */
/* do general checking for callbacks functions
 * return the MaiInterfaceValue extracted from atk value
 */

MaiInterfaceValue *getValue(AtkValue *atkValue)
{
    MAI_CHECK_ATK_OBJECT_RETURN_VAL_IF_FAIL(atkValue, NULL);
    g_return_val_if_fail(MAI_IS_ATK_WIDGET(atkValue), NULL);
    MaiWidget *maiWidget =
        NS_STATIC_CAST(MaiWidget*, (MAI_ATK_OBJECT(atkValue)->maiObject));

    MaiInterfaceValue *maiInterfaceValue =
        NS_STATIC_CAST(MaiInterfaceValue*,
                       maiWidget->GetMaiInterface(MAI_INTERFACE_VALUE));
    return maiInterfaceValue;
}

void
interfaceInitCB(AtkValueIface *aIface)
{
    g_return_if_fail(aIface != NULL);
    aIface->get_current_value = getCurrentValueCB;
    aIface->get_maximum_value = getMaximumValueCB;
    aIface->get_minimum_value = getMinimumValueCB;
    aIface->set_current_value = setCurrentValueCB;
}

void
getCurrentValueCB(AtkValue *obj, GValue *value)
{
    MaiInterfaceValue *maiInterfaceValue = getValue(obj);

    if (maiInterfaceValue)
        maiInterfaceValue->GetCurrentValue(value);

}

void
getMaximumValueCB(AtkValue *obj, GValue *value)
{
    MaiInterfaceValue *maiInterfaceValue = getValue(obj);

    if (maiInterfaceValue)
        maiInterfaceValue->GetMaximumValue(value);
}

void
getMinimumValueCB(AtkValue *obj, GValue *value)
{
    MaiInterfaceValue *maiInterfaceValue = getValue(obj);

    if (maiInterfaceValue)
        maiInterfaceValue->GetMinimumValue(value);
}

gboolean
setCurrentValueCB(AtkValue *obj, const GValue *value)
{
    MaiInterfaceValue *maiInterfaceValue = getValue(obj);
    if (!maiInterfaceValue)
        return FALSE;
    return maiInterfaceValue->SetCurrentValue(value);
}
