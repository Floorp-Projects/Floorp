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

#ifndef _NS_XINSTALLERDLG_H_
#define _NS_XINSTALLERDLG_H_

#include <malloc.h>
#include "XIErrors.h"
#include "XIDefines.h"

#include "nsINIParser.h"

/** 
 * nsXInstallerDlg
 *
 * The interface for all installer dialogs. Helps maintain
 * uniform navigation mechanism, startup init, and UI widgets.
 */
class nsXInstallerDlg
{
public:
    nsXInstallerDlg();
    virtual ~nsXInstallerDlg();

/*-------------------------------------------------------------------*
 *   Navigation
 *-------------------------------------------------------------------*/
    // NOTE: Please define these static methods too 
    //       (static methods can't be virtual)
    // static void     Back(GtkWidget *aWidget, gpointer aData);
    // static void     Next(GtkWidget *aWidget, gpointer aData);

    virtual int     Parse(nsINIParser *aParser) = 0;

    virtual int     Show(int aDirection) = 0;
    virtual int     Hide(int aDirection) = 0;

/*-------------------------------------------------------------------*
 *   INI Properties
 *-------------------------------------------------------------------*/
    int             SetShowDlg(int aShowDlg);
    int             GetShowDlg();
    int             SetTitle(char *aTitle);
    char            *GetTitle();

    void            SetPageNum(int aPageNum);
    int             GetPageNum();

    enum
    {
        SKIP_DIALOG     = 0,
        SHOW_DIALOG     = 1,

        FORWARD_MOVE    = 555,
        BACKWARD_MOVE   = -555
    };

protected:
    int         mShowDlg;
    char        *mTitle;
    char        *mSubTitle;
    int         mPageNum;      // GtkNotebook page number for this dlg
    int         mWidgetsInit;  // FALSE until widgets are created (first Show())
    GtkWidget   *mTable;
};

#endif /* _NS_XINSTALLERDLG_H_ */
