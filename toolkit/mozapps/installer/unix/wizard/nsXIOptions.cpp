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

#include "nsXIOptions.h"

nsXIOptions::nsXIOptions() :
    mTitle(NULL),
    mDestination(NULL),
    mProductName(NULL),
    mHeaderPixmap(NULL),
    mProgramName(NULL),
    mSetupType(0),
    mMode(MODE_DEFAULT),
    mShouldRunApps(TRUE),
    mProxyHost(NULL),
    mProxyPort(NULL),
    mProxyUser(NULL),
    mProxyPswd(NULL),
    mSaveModules(FALSE)
{
}

nsXIOptions::~nsXIOptions()
{
     XI_IF_FREE(mTitle);
     XI_IF_FREE(mDestination);
     XI_IF_FREE(mProxyHost);
     XI_IF_FREE(mProxyPort);
     XI_IF_FREE(mProxyUser);
     XI_IF_FREE(mProxyPswd);
}
