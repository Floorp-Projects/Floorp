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

#ifndef __MAI_APP_ROOT_H__
#define __MAI_APP_ROOT_H__

#include "nsIAccessibleEventReceiver.h"
#include "nsMaiTopLevel.h"

#define MAI_TYPE_APP_ROOT (MAI_TYPE_ATK_OBJECT)

/* MaiAppRoot is the MaiObject class for Mozilla, the whole application. Only
 * one instance of MaiAppRoot exists for one Mozilla instance. And the one
 * should be created when Mozilla Startup (with accessibility feature
 * enabled) and destroyed when Mozilla Shutdown.
 *
 * All the accessibility objects of toplevel window should be children of
 * the MaiAppRoot instance.
 */
class MaiAppRoot: public MaiObject
{
public:
    MaiAppRoot();
    virtual ~MaiAppRoot();
#ifdef MAI_LOGGING
    virtual void DumpMaiObjectInfo(int aDepth);
#endif

    virtual guint GetNSAccessibleUniqueID();

    PRBool AddMaiTopLevel(MaiTopLevel *aToplevel);
    PRBool RemoveMaiTopLevelByID(guint aID);
    PRBool LookupTopLevelID(guint aID);
public:
    /* virtual functions for MaiObject */
    virtual AtkObject *GetAtkObject(void);
    virtual void Initialize(void);
    virtual void Finalize(void);

    virtual gchar *GetName(void);
    virtual gchar *GetDescription(void);
    virtual MaiObject *GetParent(void);
    virtual gint GetChildCount(void);
    virtual MaiObject *RefChild(gint aChildIndex);
private:
    GList *mTopLevelList;
};

#endif   /* __MAI_APP_ROOT_H__ */
