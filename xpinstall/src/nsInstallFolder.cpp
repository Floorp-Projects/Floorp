/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsInstall.h"
#include "nsInstallFolder.h"

#include "nscore.h"
#include "prtypes.h"

#include "nsString.h"
#include "nsFileSpec.h"


struct DirectoryTable
{
	char *  directoryName;			/* The formal directory name */
	PRInt32 folderEnum;		        /* Directory ID */
};

struct DirectoryTable DirectoryTable[] = 
{
    {"Plugins",             100 },
	{"Program",             101 },
	{"Communicator",        102 },
	{"User Pick",           103 },
	{"Temporary",           104 },
	{"Installed",           105 },
	{"Current User",        106 },
	{"NetHelp",             107 },
	{"OS Drive",            108 },
	{"File URL",            109 },

	{"Win System",          200 },
	{"Windows",             201 },

	{"Mac System",          300 },
	{"Mac Desktop",         301 },
	{"Mac Trash",           302 },
	{"Mac Startup",         303 },
	{"Mac Shutdown",        304 },
	{"Mac Apple Menu",      305 },
	{"Mac Control Panel",   306 },
	{"Mac Extension",       307 },
	{"Mac Fonts",           308 },
	{"Mac Preferences",     309 },

	{"Unix Local",          400 },
	{"Unix Lib",            401 },

	{"",                    -1  }
};



nsInstallFolder::nsInstallFolder(const nsString& aFolderID)
{
    nsInstallFolder(aFolderID, "");
}

nsInstallFolder::nsInstallFolder(const nsString& aFolderID, const nsString& aRelativePath)
{
    mUrlPath = nsnull;

    if ( aFolderID == "null") 
    {
        return;
    }

    SetDirectoryPath( aFolderID, aRelativePath);
}


nsInstallFolder::~nsInstallFolder()
{
    if (mUrlPath != nsnull)
        delete mUrlPath;
}
        
void 
nsInstallFolder::GetDirectoryPath(nsString& aDirectoryPath)
{
	// We want the a NATIVE path.
	
    aDirectoryPath.SetLength(0);
    
    // FIX NSFILESPEC - we need to have a GetDisplayString.
    aDirectoryPath.Append(nsFilePath(*mUrlPath));
}


void
nsInstallFolder::SetDirectoryPath(const nsString& aFolderID, const nsString& aRelativePath)
{
    if ( aFolderID.EqualsIgnoreCase("User Pick") )
    {
        PickDefaultDirectory();
    }
    else if ( aFolderID.EqualsIgnoreCase("Installed") )
    {   
        mUrlPath = new nsFileSpec(aRelativePath, PR_TRUE);  // creates the directories to the relative path.
    }
    else
    {
        PRInt32 folderDirSpecID = MapNameToEnum(aFolderID);
        
        switch (folderDirSpecID) 
		{
            case 100:               //  Plugins
            
            case 101:               //  Program
            
            case 102:               //  Communicator
            
            case 103:               //  User Pick
                // we should never be here.
                break;
            case 104:               //  Temporary
            
            case 105:               //  Installed
                // we should never be here.
                break;
            case 106:               //  Current User
            
            case 107:               //  NetHelp
            
            case 108:               //  OS Drive
            
            case 109:               //  File URL


            case 200:               //  Win System
            
            case 201:               //  Windows

            case 300:               //  Mac System
            
            case 301:               //  Mac Desktop
            
            case 302:               //  Mac Trash
            
            case 303:               //  Mac Startup
            
            case 304:               //  Mac Shutdown
            
            case 305:               //  Mac Apple Menu
            
            case 306:               //  Mac Control Panel
            
            case 307:               //  Mac Extension
            
            case 308:               //  Mac Fonts
            
            case 309:               //  Mac Preferences

            case 400:               //  Unix Local
            case 401:               //  Unix Lib

            // Insert code here...

            case -1:
		    default:
			    break;
		}
    }
}

void nsInstallFolder::PickDefaultDirectory()
{
    //FIX:  Need to put up a dialog here and set mUrlPath
    return;   
}

/* MapNameToEnum
 * maps name from the directory table to its enum */
PRInt32 
nsInstallFolder::MapNameToEnum(const nsString& name)
{
	int i = 0;

	if ( name == "null")
        return -1;

	while ( DirectoryTable[i].directoryName[0] != 0 )
	{
		if ( name.EqualsIgnoreCase(DirectoryTable[i].directoryName) )
			return DirectoryTable[i].folderEnum;
		i++;
	}
	return -1;
}



