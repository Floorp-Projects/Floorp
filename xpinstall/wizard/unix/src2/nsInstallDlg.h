/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Samir Gehani <sgehani@netscape.com>
 */

#ifndef _NS_INSTALLDLG_H_
#define _NS_INSTALLDLG_H_

#include "nsXInstallerDlg.h"
#include "XIErrors.h"

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

    int             Show(int aDirection);
    int             Hide(int aDirection);

    static void     XPIProgressCB(const char *aMsg, int aVal, int aMax);
    static void     MajorProgressCB(char *aName, int aNum, int aTotal, 
                                    int aActivity);
    static int      DownloadCB(int aBytesRd, int aTotal);
    static void     SetDownloadComp(char *aName, int aNum, int aTotal);
    static void     ClearRateLabel();

    enum
    {
        ACT_DOWNLOAD,
        ACT_EXTRACT,
        ACT_INSTALL
    };

/*------------------------------------------------------------------*
 *   INI Properties
 *------------------------------------------------------------------*/
    int             SetMsg0(char *aMsg);
    char            *GetMsg0();

private:
    static void     *WorkDammitWork(void *arg); // install start
    static void     ShowCompleteDlg();
    static void     CompleteOK(GtkWidget *aWidget, gpointer aData);

    char            *mMsg0;
};

#endif /* _NS_INSTALLDLG_H_ */
