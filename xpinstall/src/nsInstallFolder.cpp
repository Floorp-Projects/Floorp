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
 *     Daniel Veditz <dveditz@netscape.com>
 *     Douglas Turner <dougt@netscape.com>
 */

#include "nsInstall.h"
#include "nsInstallFolder.h"

#include "nscore.h"
#include "prtypes.h"

#include "nsString.h"
#include "nsFileSpec.h"
#include "nsSpecialSystemDirectory.h"

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
    {"Mac Documents",       310 },

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
    aDirectoryPath.Append(mUrlPath->GetCString());
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
            case 100: ///////////////////////////////////////////////////////////  Plugins
                // FIX
                break; 

            case 101: ///////////////////////////////////////////////////////////  Program
                *this = nsSpecialSystemDirectory::OS_CurrentProcessDirectory;
                break;
            
            case 102: ///////////////////////////////////////////////////////////  Communicator
                // FIX
                break;

            case 103: ///////////////////////////////////////////////////////////  User Pick
                // we should never be here.
                break;

            case 104: ///////////////////////////////////////////////////////////  Temporary
                *this = nsSpecialSystemDirectory::OS_TemporaryDirectory;
                break;

            case 105: ///////////////////////////////////////////////////////////  Installed
                // we should never be here.
                break;

            case 106: ///////////////////////////////////////////////////////////  Current User
                // FIX
                break;

            case 107: ///////////////////////////////////////////////////////////  NetHelp
                // FIX
                break;

            case 108: ///////////////////////////////////////////////////////////  OS Drive
                *this = nsSpecialSystemDirectory::OS_DriveDirectory;
                break;

            case 109: ///////////////////////////////////////////////////////////  File URL
                // we should never be here.
                break;

            case 200: ///////////////////////////////////////////////////////////  Win System
                *this = nsSpecialSystemDirectory::Win_SystemDirectory;
                break;

            case 201: ///////////////////////////////////////////////////////////  Windows
                *this = nsSpecialSystemDirectory::Win_WindowsDirectory;
                break;

            case 300: ///////////////////////////////////////////////////////////  Mac System
                *this = nsSpecialSystemDirectory::Mac_SystemDirectory;
                break;

            case 301: ///////////////////////////////////////////////////////////  Mac Desktop
                *this = nsSpecialSystemDirectory::Mac_DesktopDirectory;
                break;

            case 302: ///////////////////////////////////////////////////////////  Mac Trash
                *this = nsSpecialSystemDirectory::Mac_TrashDirectory;
                break;

            case 303: ///////////////////////////////////////////////////////////  Mac Startup
                *this = nsSpecialSystemDirectory::Mac_StartupDirectory;
                break;

            case 304: ///////////////////////////////////////////////////////////  Mac Shutdown
                *this = nsSpecialSystemDirectory::Mac_StartupDirectory;
                break;

            case 305: ///////////////////////////////////////////////////////////  Mac Apple Menu
                *this = nsSpecialSystemDirectory::Mac_AppleMenuDirectory;
                break;

            case 306: ///////////////////////////////////////////////////////////  Mac Control Panel
                *this = nsSpecialSystemDirectory::Mac_ControlPanelDirectory;
                break;

            case 307: ///////////////////////////////////////////////////////////  Mac Extension
                *this = nsSpecialSystemDirectory::Mac_ExtensionDirectory;
                break;

            case 308: ///////////////////////////////////////////////////////////  Mac Fonts
                *this = nsSpecialSystemDirectory::Mac_FontsDirectory;
                break;

            case 309: ///////////////////////////////////////////////////////////  Mac Preferences
                *this = nsSpecialSystemDirectory::Mac_PreferencesDirectory;
                break;
                    
            case 310: ///////////////////////////////////////////////////////////  Mac Documents
                *this = nsSpecialSystemDirectory::Mac_DocumentsDirectory;
                break;
                    
            case 400: ///////////////////////////////////////////////////////////  Unix Local
                *this = nsSpecialSystemDirectory::Unix_LocalDirectory;
                break;

            case 401: ///////////////////////////////////////////////////////////  Unix Lib
                *this = nsSpecialSystemDirectory::Unix_LibDirectory;
                break;


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



//----------------------------------------------------------------------------------------
void nsInstallFolder::operator = (enum nsSpecialSystemDirectory::SystemDirectories aSystemSystemDirectory)
//----------------------------------------------------------------------------------------
{
    nsSpecialSystemDirectory temp(aSystemSystemDirectory);
    mUrlPath = new nsFileSpec(temp);
}