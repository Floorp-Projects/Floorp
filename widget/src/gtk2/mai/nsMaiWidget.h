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

#ifndef __MAI_WIDGET_H__
#define __MAI_WIDGET_H__

#include "nsMaiObject.h"
#include "nsMaiInterface.h"
#include "nsAccessibleEventData.h"

/* MaiAtkWidget */

#define MAI_TYPE_ATK_WIDGET            (mai_atk_widget_get_type ())
#define MAI_ATK_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                       MAI_TYPE_ATK_WIDGET, MaiAtkWidget))
#define MAI_ATK_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                       MAI_TYPE_ATK_WIDGET, MaiAtkWidgetClass))
#define MAI_IS_ATK_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                       MAI_TYPE_ATK_WIDGET))
#define MAI_IS_ATK_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                       MAI_TYPE_ATK_WIDGET))
#define MAI_ATK_WIDGET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                       MAI_TYPE_ATK_WIDGET, MaiAtkWidgetClass))

typedef struct _MaiAtkWidget                MaiAtkWidget;
typedef struct _MaiAtkWidgetClass           MaiAtkWidgetClass;

struct _MaiAtkWidget
{
    MaiAtkObject parent;
};

struct _MaiAtkWidgetClass
{
    MaiAtkObjectClass parent_class;
};

GType mai_atk_widget_get_type(void);

/* MaiWidget is General MaiObject for all the Widgets/Domnodes (through
 * nsIAccessible) except for root, the mozilla application.
 * a MaiWidget can have one or more interfaces on it.
 */

class MaiWidget: public MaiObject
{
public:
    MaiWidget(nsIAccessible *aAcc);
    virtual ~MaiWidget();
    static MaiWidget *Create(nsIAccessible *aAcc);

#ifdef MAI_LOGGING
    virtual void DumpMaiObjectInfo(int aDepth);
#endif

    virtual guint GetNSAccessibleUniqueID();
    MaiInterface *GetMaiInterface(MaiInterfaceType aInterfacefaceType);
    AtkRole GetAtkRole();
    static MaiWidget *CreateAndCache(nsIAccessible *aAcc);
    void ChildrenChange(AtkChildrenChange *event);
public:
    /* callbacks and their virtual functions */

    /* virtual functions from MaiObject */
    virtual AtkObject *GetAtkObject(void);
    virtual MaiObject *GetParent(void);
    virtual gint GetChildCount(void);
    virtual MaiObject *RefChild(gint aChildIndex);
    virtual gint GetIndexInParent();
    /* new ones */
    virtual PRUint32 RefStateSet();
    virtual PRUint32 GetRole();

private:
    /* Interfaces */
    MaiInterface *mMaiInterface[MAI_INTERFACE_NUM];
    gint mMaiInterfaceCount;

    void CreateMaiInterfaces(void);
    void AddMaiInterface(MaiInterface *);
    GType GetMaiAtkType(void);

    static gulong mAtkTypeNameIndex;
    static gchar *GetUniqueMaiAtkTypeName(void);

private:
    /* the hash table is not the real cache for children, it only remember
     * the uid of some children
     */
    GHashTable *mChildren;
    guint GetChildUniqueID(gint aChildIndex);
    void SetChildUniqueID(gint aChildIndex, guint aChildUid);
};

#endif /* __MAI_WIDGET_H__ */
