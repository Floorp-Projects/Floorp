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

#include "nsComponentsDlg.h"

nsComponentsDlg::nsComponentsDlg() :
    mMsg0(NULL),
    mCompList(NULL)
{
}

nsComponentsDlg::~nsComponentsDlg()
{
    if (mMsg0)
        free (mMsg0);

    if (mCompList)
        delete mCompList;
}

int
nsComponentsDlg::Back()
{
    return OK;
}

int
nsComponentsDlg::Next()
{
    return OK;
}

int
nsComponentsDlg::SetMsg0(char *aMsg)
{
    if (!aMsg)
        return E_PARAM;

    mMsg0 = aMsg;
    
    return OK;
}

char *
nsComponentsDlg::GetMsg0()
{
    if (mMsg0)
        return mMsg0;

    return NULL;
}

int
nsComponentsDlg::SetCompList(nsComponentList *aCompList)
{
    if (!aCompList)
        return E_PARAM;

    mCompList = aCompList;

    return OK;
}

nsComponentList *
nsComponentsDlg::GetCompList()
{
    if (mCompList)
        return mCompList;

    return NULL;
}
