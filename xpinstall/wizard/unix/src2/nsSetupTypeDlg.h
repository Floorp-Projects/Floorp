/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Samir Gehani <sgehani@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _NS_SETUPTYPEDLG_H_
#define _NS_SETUPTYPEDLG_H_

#include <sys/stat.h>
#include "nsXInstallerDlg.h"
#include "nsSetupType.h"
#include "nsObjectIgnore.h"
#include "nsLegacyCheck.h"

class nsSetupTypeDlg : public nsXInstallerDlg
{
public:
    nsSetupTypeDlg();
    ~nsSetupTypeDlg();

/*---------------------------------------------------------------------*
 *   Navigation
 *---------------------------------------------------------------------*/
    static void         Next(GtkWidget *aWidget, gpointer aData);

    int                 Parse(nsINIParser *aParser);

    int                 Show();
    int                 Hide();

    static void         SelectFolder(GtkWidget *aWidget, gpointer aData);
    static void         SelectFolderOK(GtkWidget *aWidget, 
                                       GtkFileSelection *aFileSel);
    static void         SelectFolderCancel(GtkWidget *aWidget, 
                                       GtkFileSelection *aFileSel);
    static void         RadBtnToggled(GtkWidget *aWidget, gpointer aData);
    static int          VerifyDestination();
    static void         NoPermsOK(GtkWidget *aWidget, gpointer aData);
    static void         CreateDestYes(GtkWidget *aWidget, gpointer aData);
    static void         CreateDestNo(GtkWidget *aWidget, gpointer aData);
    static int          DeleteOldInst();
    static void         DeleteInstDelete(GtkWidget *aWidget, gpointer aData);
    static void         DeleteInstCancel(GtkWidget *aWidget, gpointer aData);
    static int          ConstructPath(char *aDest, char *aTrunk, char *aLeaf);
    static int          CheckDestEmpty();
    static int          VerifyDiskSpace();
    static int          DSAvailable();
    static int          DSRequired();
    static void         NoDiskSpaceOK(GtkWidget *aWidget, gpointer aData);

/*---------------------------------------------------------------------*
 *   INI Properties
 *---------------------------------------------------------------------*/
    int                 SetMsg0(char *aMsg);
    char                *GetMsg0();

    int                 AddSetupType(nsSetupType *aSetupType);
    nsSetupType         *GetSetupTypeList();
    int                 GetNumSetupTypes();
    nsSetupType         *GetSelectedSetupType();

private:
    void                FreeSetupTypeList();
    void                FreeLegacyChecks();

    char                    *mMsg0;
    nsSetupType             *mSetupTypeList;
};

#endif /* _NS_SETUPTYPEDLG_H_ */
