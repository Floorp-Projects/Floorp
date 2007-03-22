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

#ifndef _NS_INSTALLDLG_H_
#define _NS_INSTALLDLG_H_

#include "nsXInstallerDlg.h"
#include "XIErrors.h"
#include "nsRunApp.h"

class nsComponent;

class nsInstallDlg : public nsXInstallerDlg
{
public:
    nsInstallDlg();
    ~nsInstallDlg();

/*------------------------------------------------------------------*
 *   Navigation
 *------------------------------------------------------------------*/
    static void     Back(GtkWidget *aWidget, gpointer aData);
    static void     Next(GtkWidget *aWidget, gpointer aData);

    int             Parse(nsINIParser* aParser);

    int             Show();
    int             Hide();
    int             ShowTable();
    int             HideTable();

    static void     XPIProgressCB(const char *aMsg, int aVal, int aMax);
    static void     MajorProgressCB(char *aName, int aNum, int aTotal, 
                                    int aActivity);
    static int      DownloadCB(int aBytesRd, int aTotal);
    static void     SetDownloadComp(nsComponent *aComp, int aURLIndex,
                                    int aNum, int aTotal);
    static void     ClearRateLabel();
    static int      CancelOrPause();

    void            ReInitUI();
    void            ShowCRCDlg();
    void            DestroyCRCDlg();

    enum
    {
        ACT_DOWNLOAD,
        ACT_EXTRACT,
        ACT_INSTALL,
        ACT_COMPLETE
    };

    enum
    {
        E_DL_PAUSE      = -1101,
        E_DL_CANCEL     = -1102,
        E_DL_DROP_CXN   = -1103
    };

/*------------------------------------------------------------------*
 *   INI Properties
 *------------------------------------------------------------------*/
    int             SetMsg0(char *aMsg);
    char            *GetMsg0();

private:
    static int      PerformInstall(void); // install start
    static void     SaveModulesToggled(GtkWidget *aWidget, gpointer aData);
    static void     ShowProxySettings(GtkWidget *aWidget, gpointer aData);
    static void     PSDlgOK    (GtkWidget *aWidget, gpointer aData);
    static void     PSDlgCancel(GtkWidget *aWidget, gpointer aData);
    static void     RunApps(nsRunApp *aRunAppList, int aSequential);
    static void     FreeRunAppList(nsRunApp *aRunAppList);
    int             AppendRunApp(nsRunApp **aRunAppList, nsRunApp *aNewRunApp);
    static void     DLPause(GtkWidget *aWidget, gpointer aData);
    static void     DLResume(GtkWidget *aWidget, gpointer aData);
    static void     DLCancel(GtkWidget *aWidget, gpointer aData);
    static int      ShowCxnDroppedDlg();
    static void     CxnDroppedOK(GtkWidget *aWidget, gpointer aData);
    static void     CRCOKCb(GtkWidget *aWidget, gpointer aData);
    static void     HideNavButtons();
    static void     InitInstallProgress();
    static int      TotalDLSize();
    static void     CompressToFit(char *aOrigStr, char *aOutStr, 
                        int aOutStrLen);
    static void     InitDLProgress(int IsFirst);

    char            *mMsg0;
};

#endif /* _NS_INSTALLDLG_H_ */
