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
#include "nsString2.h"

#ifdef  DEBUG_MAI
gint num_created_mai_object = 0;
gint num_deleted_mai_object = 0;
#endif

GType
mai_atk_object_get_type(void)
{
    static GType type = 0;

    if (!type) {
        static const GTypeInfo tinfo = {
            sizeof(MaiAtkObjectClass),
            (GBaseInitFunc)NULL,
            (GBaseFinalizeFunc)NULL,
            (GClassInitFunc)MaiObject::classInitCB,
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
    mMaiAtkObject = nsnull;

#ifdef  DEBUG_MAI
    num_created_mai_object++;
    g_print("====MaiObject creating this=%u,total =%d= created\n",
            this,num_created_mai_object);
#endif
}

MaiObject::~MaiObject()
{
    /* destory this */

    /* mAccessible will get released here automatically */
    /* "mAccessible = nsnull" is not needed */
    if (mMaiAtkObject)
        ;   /* Should we call the parent->finalize ?? */

#ifdef  DEBUG_MAI
    num_deleted_mai_object++;
    g_print("====MaiObject deleting this=%u, total =%d= deleted\n",
            this,num_deleted_mai_object);
#endif
}

void
MaiObject::EmitAccessibilitySignal(PRUint32 aEvent)
{
    if (mMaiAtkObject || GetAtkObject()) {
        gchar * name = NULL;
        name = GetAtkSignalName(aEvent);
        if (name) {
            g_signal_emit_by_name(ATK_OBJECT(mMaiAtkObject), name);

#ifdef DEBUG_MAI
            g_print("MaiObject, emit signal %s\n", name );
#endif
        }
    }
}

nsIAccessible *
MaiObject::GetNSAccessible(void)
{
    return mAccessible;
}

#if 0
AtkObject *
MaiObject::GetAtkObject (void)
{
    g_return_val_if_fail(mAccessible != NULL, NULL);
  
    if (mMaiAtkObject)
        return ATK_OBJECT(mMaiAtkObject);

    nsCOMPtr<nsIAccessible> accessIf(do_QueryInterface(mAccessible));
    if (accessIf) {
        /* the atk object should be created according to accessIf, not yet */
        mMaiAtkObject = (MaiAtkObject*)g_object_new(MAI_TYPE_ATK_OBJECT,NULL);
        g_return_val_if_fail(mMaiAtkObject != NULL, NULL);

        atk_object_initialize(ATK_OBJECT(mMaiAtkObject), this);
        ATK_OBJECT(mMaiAtkObject)->role = ATK_ROLE_INVALID;
        ATK_OBJECT(mMaiAtkObject)->layer = ATK_LAYER_INVALID;

        return ATK_OBJECT(mMaiAtkObject);
    }
    return NULL;
}
#endif

gchar*
MaiObject::GetAtkSignalName(PRUint32 aEvent)
{
    gchar * name = NULL;

    switch (aEvent) {
    default:
        return NULL;
    }
    return name;
}

/* virtual functions to ATK callbacks */
gchar*
MaiObject::GetName(void)
{
    gchar *new_str;
    gint len;

    g_return_val_if_fail(mAccessible != NULL, NULL);

    nsAutoString uniName;

    /* nsIAccessible is responsible for the non-NULL name */
    nsresult rv = mAccessible->GetAccName(uniName);
    if (NS_FAILED (rv))
        return NULL;
    len = uniName.Length();
    new_str = g_new(gchar, len + 1);
    uniName.ToCString(new_str, len);
    new_str[len] = '\0';
    return new_str;
}

gchar*
MaiObject::GetDescription(void)
{
    gchar *new_str;
    gint len;

    g_return_val_if_fail(mAccessible != NULL, NULL);

    nsAutoString uniDescription;
  
    /* nsIAccessible is responsible for the non-NULL description */
    nsresult rv = mAccessible->GetAccDescription(uniDescription);
    if (NS_FAILED(rv))
        return NULL;
    len = uniDescription.Length();
    new_str = g_new(gchar, len + 1);
    uniDescription.ToCString(new_str, len);
    new_str[len] = '\0';
    return new_str;
}

gint
MaiObject::GetChildCount(void)
{
    return 0;
}

MaiObject*
MaiObject::RefChild(gint i)
{
    return NULL;
}

void
MaiObject::Initialize(void)
{
    g_return_if_fail(mAccessible != NULL);
    g_return_if_fail(mMaiAtkObject != NULL);

    gchar default_name[] = "no name";
    gchar default_description[] = "no description";

    /* we can set name, description, etc for atkobject here */
    gchar * name, *description;
    name = GetName();
    description = GetDescription();

    atk_object_set_name(ATK_OBJECT(mMaiAtkObject),
                        (!name || strlen(name) == 0)? default_name : name);
    atk_object_set_description(ATK_OBJECT(mMaiAtkObject),
                               (!description || strlen(description) == 0)?
                               default_description : description);
    if (name)
        g_free(name);
    if (description)
        g_free(description);
}

void
MaiObject::Finalize(void)
{
    delete this;
}

/* static functions for ATK callback */

void
MaiObject::classInitCB(AtkObjectClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    //  klass->get_name = MaiObject::getNameCB;
    klass->get_n_children = MaiObject::getChildCountCB;
    klass->ref_child = MaiObject::refChildCB;
    //  klass->ref_state_set = MaiObject::refStateSetCB;
    klass->initialize = MaiObject::initializeCB;

    gobject_class->finalize = MaiObject::finalizeCB;
}

/*
  const gchar*
  MaiObject::getNameCB(AtkObject *obj)
  {
  g_return_val_if_fail(MAI_IS_ATK_OBJECT(obj), NULL);

  MaiObject * maiObject = MAI_ATK_OBJECT(obj)->maiObject;
  g_return_val_if_fail(maiObject!=NULL, NULL);
    
  return maiObject->GetName();
  }
*/

gint
MaiObject::getChildCountCB(AtkObject* obj)
{
    g_return_val_if_fail(MAI_IS_ATK_OBJECT(obj), 0);

    MaiObject * maiObject = MAI_ATK_OBJECT(obj)->maiObject;
    g_return_val_if_fail(maiObject!=NULL, 0);
    
    return maiObject->GetChildCount();
}

AtkObject*
MaiObject::refChildCB(AtkObject *obj, gint i)
{
    g_return_val_if_fail(MAI_IS_ATK_OBJECT(obj), NULL);

    MaiObject * maiObject = MAI_ATK_OBJECT(obj)->maiObject;
    g_return_val_if_fail(maiObject!=NULL, NULL);
    
    MaiObject * childObject = maiObject->RefChild(i);
    g_return_val_if_fail(childObject!=NULL, NULL);
  
    return childObject->GetAtkObject();
}

void
MaiObject::initializeCB(AtkObject *obj,
                        gpointer   data)
{
    g_return_if_fail(MAI_IS_ATK_OBJECT(obj));
    g_return_if_fail(data != NULL);

    MAI_ATK_OBJECT(obj)->maiObject = (MaiObject*)data;
    ((MaiObject*)data)->Initialize();
}

void
MaiObject::finalizeCB(GObject  *obj)
{
    g_return_if_fail(MAI_IS_ATK_OBJECT(obj));

    MaiObject * maiObject = MAI_ATK_OBJECT(obj)->maiObject;
    g_return_if_fail(maiObject!=NULL);
  
    maiObject->Finalize();
}
