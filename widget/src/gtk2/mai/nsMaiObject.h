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

#ifndef __MAI_OBJECT_H__
#define __MAI_OBJECT_H__

#include <atk/atk.h>
#include <glib.h>
#include <glib-object.h>

#include "nsCOMPtr.h"
#include "nsIAccessible.h"

#include "prlog.h"

#ifdef PR_LOGGING
#define MAI_LOGGING
#endif /* #ifdef PR_LOGGING */

extern PRLogModuleInfo *gMaiLog;

#define MAI_LOG(level, args) PR_LOG(gMaiLog, (level), args)
#define MAI_LOG_DEBUG(args) PR_LOG(gMaiLog, PR_LOG_DEBUG, args)
#define MAI_LOG_WARNING(args) PR_LOG(gMaiLog, PR_LOG_WARNING, args)
#define MAI_LOG_ERROR(args) PR_LOG(gMaiLog, PR_LOG_ERROR, args)

class MaiObject;

/* MaiAtkObject */

#define MAI_TYPE_ATK_OBJECT             (mai_atk_object_get_type ())
#define MAI_ATK_OBJECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                         MAI_TYPE_ATK_OBJECT, MaiAtkObject))
#define MAI_ATK_OBJECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                         MAI_TYPE_ATK_OBJECT, \
                                         MaiAtkObjectClass))
#define MAI_IS_ATK_OBJECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                         MAI_TYPE_ATK_OBJECT))
#define MAI_IS_ATK_OBJECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                         MAI_TYPE_ATK_OBJECT))
#define MAI_ATK_OBJECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                         MAI_TYPE_ATK_OBJECT, \
                                         MaiAtkObjectClass))

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

/* MaiObject is the base class for all MAI object class (but not for MAI
 * interface class). MaiObject is nothing but a connection between
 * nsIAccessible and AtkObject. It maps one nsIAccessible object
 * with some nsIAccessible Interfaces (e.g. nsIAccessibleAction,
 * nsIAccessileText) to right AtkObject with right Atk interfaces (i.e.
 * AtkAction, AtkText), and provide methods to make the pair of
 * nsIAccessilbe object and Atk object communicate with each
 * other.
 *
 * MaiObject, and its descendents provide the implementation of AtkObject.
 */

class MaiObject
{
public:
    MaiObject(nsIAccessible *aAcc = NULL);
    virtual ~MaiObject();

#ifdef MAI_LOGGING
    virtual void DumpMaiObjectInfo(int aDepth) = 0;
#endif
    virtual guint GetNSAccessibleUniqueID() = 0;

public:
    virtual AtkObject *GetAtkObject(void) = 0;
    virtual nsIAccessible *GetNSAccessible(void);
    static void TranslateStates(PRUint32 aAccState,
                                AtkStateSet *state_set);

    /* virtual functions called by callbacks */
    virtual void Initialize(void);
    virtual void Finalize(void);

    /* the virtual functions should not use the type of AtkObject* as return
     * value or Parameters, they should use the type of MaiObject
     * its the callbacks's repsonsibility to make convertion when needed
     */
    virtual gchar *GetName(void);
    virtual gchar *GetDescription(void);
    virtual PRUint32 GetRole(void);
    virtual MaiObject *GetParent(void) = 0;
    virtual gint GetChildCount(void);
    virtual MaiObject *RefChild(gint aChildIndex);
    virtual gint GetIndexInParent();

protected:
    nsCOMPtr<nsIAccessible> mAccessible;
    MaiAtkObject *mMaiAtkObject;
};

#define MAI_CHECK_ATK_OBJECT_RETURN_VAL_IF_FAIL(obj, val) \
    do {\
        g_return_val_if_fail(MAI_IS_ATK_OBJECT(obj), (val));\
        MaiObject * tmpMaiObjPassedIn = MAI_ATK_OBJECT(obj)->maiObject;\
        g_return_val_if_fail(tmpMaiObjPassedIn != NULL, (val));\
        g_return_val_if_fail(tmpMaiObjPassedIn->GetAtkObject() ==\
                             (AtkObject*)(obj), (val));\
    } while (0)

#define MAI_CHECK_ATK_OBJECT_RETURN_IF_FAIL(obj) \
    do {\
        g_return_if_fail(MAI_IS_ATK_OBJECT(obj));\
        MaiObject * tmpMaiObjPassedIn = MAI_ATK_OBJECT(obj)->maiObject;\
        g_return_if_fail(tmpMaiObjPassedIn != NULL);\
        g_return_if_fail(tmpMaiObjPassedIn->GetAtkObject() ==\
                         (AtkObject*)(obj));\
    } while (0)

guint GetNSAccessibleUniqueID(nsIAccessible *aObj);

#ifdef MAI_LOGGING
extern gint num_created_mai_object;
extern gint num_deleted_mai_object;
#endif

#endif /* __MAI_OBJECT_H__ */
