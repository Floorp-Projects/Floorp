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

#include <atk/atk.h>
#include <glib.h>
#include <glib-object.h>

#include "nsCOMPtr.h"
#include "nsIAccessible.h"

#ifndef __MAI_OBJECT_H__
#define __MAI_OBJECT_H__

class MaiObject;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /* MaiAtkObject */

#define MAI_TYPE_ATK_OBJECT             (mai_atk_object_get_type ())
#define MAI_ATK_OBJECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                        MAI_TYPE_ATK_OBJECT, MaiAtkObject))
#define MAI_ATK_OBJECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                        MAI_TYPE_ATK_OBJECT, MaiAtkObjectClass))
#define MAI_IS_ATK_OBJECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                        MAI_TYPE_ATK_OBJECT))
#define MAI_IS_ATK_OBJECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                        MAI_TYPE_ATK_OBJECT))
#define MAI_ATK_OBJECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                        MAI_TYPE_ATK_OBJECT, MaiAtkObjectClass))

typedef struct _MaiAtkObject                MaiAtkObject;
typedef struct _MaiAtkObjectClass           MaiAtkObjectClass;

/**
 * This MaiAtkObject is a thin wrapper, in the MAI namespace, for AtkObject
 */

struct _MaiAtkObject
{
    AtkObject parent;

    /*
     * The MaiObject whose properties and features are exported via this
     * object instance.
     */
    MaiObject *maiObject;
};

struct _MaiAtkObjectClass
{
    AtkObjectClass parent_class;
};

GType mai_atk_object_get_type(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

class MaiObject
{
public:
    MaiObject(nsIAccessible *aAcc = NULL);
    virtual ~MaiObject();

public:

    virtual void EmitAccessibilitySignal(PRUint32 aEvent);
    virtual AtkObject *GetAtkObject(void) = 0;
    virtual nsIAccessible *GetNSAccessible(void);
    virtual gchar* GetAtkSignalName(PRUint32 aEvent);

    /* virtual functions called by callbacks */
    virtual gchar *GetName(void);
    virtual gchar *GetDescription(void);
    virtual gint GetChildCount(void);
    virtual MaiObject *RefChild(gint i);
    virtual void Initialize(void);
    virtual void Finalize(void);
protected:
    nsCOMPtr<nsIAccessible> mAccessible;
    MaiAtkObject *mMaiAtkObject;

public:

    /* callbacks for AtkObject */
    static void classInitCB(AtkObjectClass *klass);
    static void initializeCB(AtkObject *obj,
                             gpointer data);
    static void finalizeCB(GObject *obj);


    static const gchar* getNameCB (AtkObject *obj);
    static gchar* getDescriptionCB (AtkObject *obj);
    static AtkObject* getParent(AtkObject *obj);
    static gint                getChildCountCB(AtkObject *obj);
    static AtkObject*          refChildCB(AtkObject *obj, gint  i);
    static gint                getIndexInParentCB(AtkObject *obj);
    static AtkRelationSet*     refRelationSetCB(AtkObject *obj);
    static AtkRole             getRoleCB(AtkObject *obj);
    static AtkLayer            getLayerCB(AtkObject *obj);
    static gint                getMdiZorder(AtkObject *obj);
};

#ifdef DEBUG_MAI

extern gint num_created_mai_object;
extern gint num_deleted_mai_object;

#endif


#endif /* __MAI_OBJECT_H__ */
