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

#ifndef _NS_XIOPTIONS_H_
#define _NS_XIOPTIONS_H_

#include "XIDefines.h"

class nsXIOptions
{
public:
    nsXIOptions();
    ~nsXIOptions();
    
    char    *mTitle;
    char    *mDestination;
    int     mSetupType;
    int     mMode;
    int     mShouldRunApps;

    enum
    {
        MODE_DEFAULT = 0,
        MODE_AUTO,
        MODE_SILENT
    };

    char    *mProxyHost;
    char    *mProxyPort;
    char    *mProxyUser;
    char    *mProxyPswd;

    int     mSaveModules;
};
    
#endif /* _NS_XIOPTIONS_H_ */
