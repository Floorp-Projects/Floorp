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

#include "nsWelcomeDlg.h"

nsWelcomeDlg::nsWelcomeDlg() :
    mReadmeFile(NULL),
    mMsg0(NULL),
    mMsg1(NULL),
    mMsg2(NULL)
{
}

nsWelcomeDlg::~nsWelcomeDlg()
{
    if (mReadmeFile)
        free (mReadmeFile);
    if (mMsg0)
        free (mMsg0);
    if (mMsg1)
        free (mMsg1);
    if (mMsg2)
        free (mMsg2);
}

int 
nsWelcomeDlg::Back()
{
    return OK;
}

int 
nsWelcomeDlg::Next()
{
    return OK;
}

int
nsWelcomeDlg::SetReadmeFile(char *aReadmeFile)
{
    if (!aReadmeFile)
        return E_PARAM;

    mReadmeFile = aReadmeFile;

    return OK;
}

char *
nsWelcomeDlg::GetReadmeFile()
{
    if (mReadmeFile)
        return mReadmeFile;

    return NULL;
}

int
nsWelcomeDlg::SetMsg0(char *aMsg)
{
    if (!aMsg)
        return E_PARAM;

    mMsg0 = aMsg;

    return OK;
}

char *
nsWelcomeDlg::GetMsg0()
{
    if (mMsg0)
        return mMsg0;

    return NULL;
}

int
nsWelcomeDlg::SetMsg1(char *aMsg)
{
    if (!aMsg)
        return E_PARAM;

    mMsg1 = aMsg;

    return OK;
}

char *
nsWelcomeDlg::GetMsg1()
{
    if (mMsg1)
        return mMsg1;

    return NULL;
}

int
nsWelcomeDlg::SetMsg2(char *aMsg)
{
    if (!aMsg)
        return E_PARAM;

    mMsg2 = aMsg;

    return OK;
}

char *
nsWelcomeDlg::GetMsg2()
{
    if (mMsg2)
        return mMsg2;

    return NULL;
}
