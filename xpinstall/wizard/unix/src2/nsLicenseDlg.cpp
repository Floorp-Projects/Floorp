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

#include "nsLicenseDlg.h"

nsLicenseDlg::nsLicenseDlg() :
    mLicenseFile(NULL),
    mMsg0(NULL),
    mMsg1(NULL)
{
}

nsLicenseDlg::~nsLicenseDlg()
{
    if (mLicenseFile)
        free (mLicenseFile);
    if (mMsg0)
        free (mMsg0);
    if (mMsg1)
        free (mMsg1);
}

int
nsLicenseDlg::Back()
{
    return OK;
}

int
nsLicenseDlg::Next()
{
    return OK;
}

int
nsLicenseDlg::SetLicenseFile(char *aLicenseFile)
{
    if (!aLicenseFile)
        return E_PARAM;

    aLicenseFile = mLicenseFile;

    return OK;
}

char *
nsLicenseDlg::GetLicenseFile()
{
    if (mLicenseFile)
        return mLicenseFile;

    return NULL;
}

int
nsLicenseDlg::SetMsg0(char *aMsg)
{
    if (!aMsg)
        return E_PARAM;

    mMsg0 = aMsg;

    return OK;
}

char *
nsLicenseDlg::GetMsg0()
{
    if (mMsg0)
        return mMsg0;

    return NULL;
}

int
nsLicenseDlg::SetMsg1(char *aMsg)
{
    if (!aMsg)
        return E_PARAM;

    mMsg1 = aMsg;

    return OK;
}

char *
nsLicenseDlg::GetMsg1()
{
    if (mMsg1)
        return mMsg1;

    return NULL;
}
