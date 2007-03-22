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

    virtual int     Show() = 0;
    virtual int     Hide() = 0;

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
        SHOW_DIALOG     = 1
    };

protected:
    int         mShowDlg;
    char        *mTitle;
    int         mPageNum;      // GtkNotebook page number for this dlg
    int         mWidgetsInit;  // FALSE until widgets are created (first Show())
    GtkWidget   *mTable;
};

#endif /* _NS_XINSTALLERDLG_H_ */
