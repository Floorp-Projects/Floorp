/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Doug Turner <dougt@netscape.com>
 */

#ifndef _NSSPECIALSYSTEMDIRECTORY_H_
#define _NSSPECIALSYSTEMDIRECTORY_H_

#include "nscore.h"
#include "nsFileSpec.h"

#ifdef XP_MAC
#include "Types.h"
#endif

// SEE ALSO:
//      mozilla/xpfe/appshell/public/nsFileLocations.h

class NS_BASE nsSpecialSystemDirectory : public nsFileSpec
{

    public:
        enum SystemDirectories
        {
            OS_DriveDirectory         =   1
        ,   OS_TemporaryDirectory     =   2
        ,   OS_CurrentProcessDirectory=   3
        ,   OS_CurrentWorkingDirectory=   4

        ,   Mac_SystemDirectory       =   101
        ,   Mac_DesktopDirectory      =   102
        ,   Mac_TrashDirectory        =   103
        ,   Mac_StartupDirectory      =   104
        ,   Mac_ShutdownDirectory     =   105
        ,   Mac_AppleMenuDirectory    =   106
        ,   Mac_ControlPanelDirectory =   107
        ,   Mac_ExtensionDirectory    =   108
        ,   Mac_FontsDirectory        =   109
        ,   Mac_PreferencesDirectory  =   110
        ,   Mac_DocumentsDirectory    =   111
        
        ,   Win_SystemDirectory       =   201
        ,   Win_WindowsDirectory      =   202
        
        ,   Unix_LocalDirectory       =   301
        ,   Unix_LibDirectory         =   302    
        };

                    //nsSpecialSystemDirectory();
                    nsSpecialSystemDirectory(SystemDirectories aSystemSystemDirectory);
        
    virtual         ~nsSpecialSystemDirectory();

    void            operator = (SystemDirectories aSystemSystemDirectory);
 
#ifdef XP_MAC
    void            operator = (OSType folderType);
                    nsSpecialSystemDirectory(OSType folderType);
#endif

private:
    void            operator = (const char* inPath) { *(nsFileSpec*)this = inPath; }

}; // class NS_BASE nsSpecialSystemDirectory

#endif
