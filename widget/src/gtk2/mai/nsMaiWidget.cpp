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
#include "nsMaiWidget.h"

GType
mai_atk_widget_get_type(void)
{
    static GType type = 0;

    if (!type) {
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

        static const GInterfaceInfo atk_component_info = {
            (GInterfaceInitFunc) MaiWidget::atkComponentInterfaceInitCB,
            (GInterfaceFinalizeFunc) NULL,
            NULL
        };

        type = g_type_register_static(MAI_TYPE_ATK_OBJECT,
                                      "MaiAtkWidget", &tinfo, GTypeFlags(0));
        g_type_add_interface_static(type, ATK_TYPE_COMPONENT,
                                    &atk_component_info);
    }
    return type;
}

gulong MaiWidget::mAtkTypeNameIndex = 0;

MaiWidget::MaiWidget(nsIAccessible *aAcc): MaiObject(aAcc)
{
    mMaiInterfaceCount = 0;
    for (int i=0; i < MAI_INTERFACE_NUM; i++) {
        mMaiInterface[i] = NULL;
    }

#ifdef  DEBUG_MAI
    g_print("object =%u of type MaiWidget created\n", this);
#endif
}

MaiWidget::~MaiWidget()
{
    for (int i=0; i < MAI_INTERFACE_NUM; i++) {
        if (mMaiInterface[i])
            delete mMaiInterface[i];
    }

#ifdef  DEBUG_MAI
    g_print("object=%u of type MaiWidget deleted\n", this);
#endif
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

    type = g_type_register_static(MAI_TYPE_ATK_WIDGET,
                                  MaiWidget::GetUniqueMaiAtkTypeName(),
                                  &tinfo, GTypeFlags(0));

    if ( mMaiInterfaceCount == 0)
        return MAI_TYPE_ATK_WIDGET;

    for (int i=0; i < MAI_INTERFACE_NUM; i++) {
        if (mMaiInterface[i]) {
            g_type_add_interface_static(type, mMaiInterface[i]->GetAtkType(),
                                        mMaiInterface[i]->GetInterfaceInfo());
        }
    }
    return type;
}

AtkObject *
MaiWidget::GetAtkObject(void)
{
    g_return_val_if_fail(mAccessible != NULL, NULL);
 
    if (mMaiAtkObject)
        return ATK_OBJECT(mMaiAtkObject);

    nsCOMPtr<nsIAccessible> accessIf(do_QueryInterface(mAccessible));
    if (!accessIf) {
#ifdef DEBUG_MAI
        g_print("MaiWidget::GetAtkObject: NOT nsIAccessible!\n");
#endif
        return NULL;
    }

    CreateMaiInterfaces();
    mMaiAtkObject =  (MaiAtkObject*) g_object_new(GetMaiAtkType(), NULL);
    g_return_val_if_fail(mMaiAtkObject != NULL, NULL);

    atk_object_initialize(ATK_OBJECT(mMaiAtkObject), this);
    ATK_OBJECT(mMaiAtkObject)->role = ATK_ROLE_INVALID;
    ATK_OBJECT(mMaiAtkObject)->layer = ATK_LAYER_INVALID;

#ifdef DEBUG_MAI
    g_print("MaiWidget=%u, GetAtkObject obj=%u, ref=%d\n",
            this, mMaiAtkObject, G_OBJECT(mMaiAtkObject)->ref_count);
#endif
    return ATK_OBJECT(mMaiAtkObject);
}

void
MaiWidget::AddMaiInterface(MaiInterface *maiIf)
{
    g_return_if_fail(maiIf != NULL);
    MaiInterfaceType maiIfType = maiIf->GetType();
 
    // if same type of If has been added, release previous one
    if (mMaiInterface[maiIfType]) {
        delete mMaiInterface[maiIfType];
    }
    mMaiInterface[maiIfType] = maiIf;
    mMaiInterfaceCount++;
}

void
MaiWidget::CreateMaiInterfaces()
{
    g_return_if_fail(mAccessible != NULL);

    // Add Interfaces for each nsIAccessible.ext interfaces

    /*
      nsCOMPtr<nsIAccessibleAction> accessIfAction(do_QueryInterface(mAccessible));
      if (accessIfAction) {
      MaiInterfaceAction maiIfAction = new MaiInterfaceAction(this);
      AddMaiInterface(maiIfAction);
      }

      // all the interfaces follow here
      nsIAccessibleAction.idl
      nsIAccessibleEditableText.idl
      nsIAccessibleHyperLink.idl
      nsIAccessibleHyperText.idl
      nsIAccessibleSelection.idl
      nsIAccessibleTable.idl
      nsIAccessibleText.idl
      nsIAccessibleValue.idl

    */
}

/* virtual functions */
void
MaiWidget::GetExtents(gint           *x,
                      gint           *y,
                      gint           *width,
                      gint           *height,
                      AtkCoordType   coord_type)
{
    g_return_if_fail(mAccessible != NULL);

    nsresult rv = mAccessible->AccGetBounds(x, y, width, height);
    if (NS_FAILED(rv))
        return;

    // or ATK_XY_SCREEN  what is definition this in nsIAccessible?
    if (coord_type == ATK_XY_WINDOW) {
        /* deal with the coord type */
    }
}

/* static functions */

void
MaiWidget::atkComponentInterfaceInitCB(AtkComponentIface *iface)
{
    g_return_if_fail(iface != NULL);

    /*
     * Use default implementation for contains and get_position
     */
    iface->get_extents = MaiWidget::getExtentsCB;
}

void
MaiWidget::getExtentsCB(AtkComponent   *component,
                        gint           *x,
                        gint           *y,
                        gint           *width,
                        gint           *height,
                        AtkCoordType   coord_type)
{
    g_return_if_fail(MAI_IS_ATK_WIDGET(component));

    MaiWidget * maiWidget = (MaiWidget*)MAI_ATK_OBJECT(component)->maiObject;
    g_return_if_fail(maiWidget!=NULL);

    maiWidget->GetExtents(x, y, width, height, coord_type);
}

/* static */
gchar*
MaiWidget::GetUniqueMaiAtkTypeName(void)
{
#define MAI_ATK_TYPE_NAME_LEN (30)     /* 10+sizeof(gulong)/4+1 < 30 */

    static gchar namePrefix[] = "MaiAtkType";  /* size = 10 */
    static gchar name[MAI_ATK_TYPE_NAME_LEN+1];

    sprintf(name, "%s%x", namePrefix, mAtkTypeNameIndex++);
    name[MAI_ATK_TYPE_NAME_LEN] = '\0';

#ifdef DEBUG_MAI
    g_print("MaiWidget::LastedTypeName=%s\n", name);
#endif
    return name;
}
