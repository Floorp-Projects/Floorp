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

#ifndef _NS_LICENSEDLG_H_
#define _NS_LICENSEDLG_H_

#include "nsXInstallerDlg.h"
#include "XIErrors.h"

class nsLicenseDlg : public nsXInstallerDlg
{
public:
    nsLicenseDlg();
    ~nsLicenseDlg();

/*-------------------------------------------------------------------*
 *   Navigation
 *-------------------------------------------------------------------*/
    static void     Back(GtkWidget *aWidget, gpointer aData);
    static void     Next(GtkWidget *aWidget, gpointer aData);
    
    int     Parse(nsINIParser *aParser);

    int     Show(int aDirection);
    int     Hide(int aDirection);

/*-------------------------------------------------------------------*
 *   INI Properties
 *-------------------------------------------------------------------*/
    int     SetLicenseFile(char *aLicenseFile);
    char    *GetLicenseFile();
    char    *GetLicenseContents();
    
private:
    char    *mLicenseFile;
};

#endif /* _NS_LICENSEDLG_H_ */
