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

#include "nsXInstallerDlg.h"

nsXInstallerDlg::nsXInstallerDlg() :
    mShowDlg(nsXInstallerDlg::SHOW_DIALOG),
    mTitle(NULL),
    mWidgetsInit((int)FALSE),
    mTable(NULL)
{
}

nsXInstallerDlg::~nsXInstallerDlg()
{
    if (mTitle)
        free (mTitle);
}

int
nsXInstallerDlg::SetShowDlg(int aShowDlg)
{
    if ( aShowDlg != nsXInstallerDlg::SHOW_DIALOG &&
         aShowDlg != nsXInstallerDlg::SKIP_DIALOG )
        return E_PARAM;

    mShowDlg = aShowDlg;

    return OK;
}

int 
nsXInstallerDlg::GetShowDlg()
{
    return mShowDlg;
}

int
nsXInstallerDlg::SetTitle(char *aTitle)
{
    if (!aTitle)
        return E_PARAM;
    
    mTitle = aTitle;

    return OK;
}

char *
nsXInstallerDlg::GetTitle()
{
    if (mTitle)
        return mTitle;

    return NULL;
}
