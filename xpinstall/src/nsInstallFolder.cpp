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
#include "nsRepository.h"

#include "nsString.h"
#include "nsFileSpec.h"
#include "nsIFileSpec.h"
#include "nsSpecialSystemDirectory.h"

#include "nsFileLocations.h"
#include "nsIFileLocator.h"

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
	{"Preferences",         107 },
	{"OS Drive",            108 },
	{"file:///",            109 },

    {"Components",          110 },
    {"Chrome",              111 },

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
    mFileSpec = nsnull;
    SetDirectoryPath( aFolderID, "");
}

nsInstallFolder::nsInstallFolder(const nsString& aFolderID, const nsString& aRelativePath)
{
    mFileSpec = nsnull;

    /*
        aFolderID can be either a Folder enum in which case we merely pass it
        to SetDirectoryPath, or it can be a Directory.  If it is the later, it
        must already exist and of course be a directory not a file.  
    */

    SetDirectoryPath( aFolderID, aRelativePath );

    // check to see if that worked
    if ( !mFileSpec )
    {
        // it didn't, so aFolderID is not one of the magic strings.
        // maybe it's already a pathname? If so it had better be a directory
        // if it already exists...
        nsFileSpec dirCheck(aFolderID);
        if ( (dirCheck.Error() == NS_OK) && 
             ( dirCheck.IsDirectory() || !dirCheck.Exists() ) )
        {
            mFileSpec = new nsFileSpec( dirCheck );

            if (mFileSpec && aRelativePath.Length() > 0 )
            {
                // we've got a subdirectory to tack on
                nsString morePath(aRelativePath);

                if ( morePath.Last() != '/' || morePath.Last() != '\\' )
                    morePath += '/';

                *mFileSpec += morePath;
            }

            // make sure that the directory is created. 
            // XXX: **why** are we creating these? they might not be used!
            nsFileSpec(mFileSpec->GetCString(), PR_TRUE);
        }
    }
}


nsInstallFolder::~nsInstallFolder()
{
    if (mFileSpec != nsnull)
        delete mFileSpec;
}
        
void 
nsInstallFolder::GetDirectoryPath(nsString& aDirectoryPath)
{
	aDirectoryPath = "";
    
    if (mFileSpec != nsnull)
    {
        // We want the a NATIVE path.
       aDirectoryPath.SetString(mFileSpec->GetCString());
    }
}

void
nsInstallFolder::SetDirectoryPath(const nsString& aFolderID, const nsString& aRelativePath)
{
    if ( aFolderID.EqualsIgnoreCase("User Pick") )
    {
        PickDefaultDirectory();
        return;
    }
    else if ( aFolderID.EqualsIgnoreCase("Installed") )
    {   
        mFileSpec = new nsFileSpec(aRelativePath, PR_TRUE);  // creates the directories to the relative path.
        return;
    }
    else
    {
        nsresult rv = NS_OK;
        PRInt32 folderDirSpecID = MapNameToEnum(aFolderID);
        
        switch (folderDirSpecID) 
		{
            case 100: ///////////////////////////////////////////////////////////  Plugins
                if (!nsSoftwareUpdate::GetProgramDirectory())
                {
                    SetAppShellDirectory(nsSpecialFileSpec::App_PluginsDirectory );
                }
                else
                {
                    mFileSpec = new nsFileSpec();
                    if ( !mFileSpec )
                        rv = NS_ERROR_OUT_OF_MEMORY;
                    else
                        rv = nsSoftwareUpdate::GetProgramDirectory()->GetFileSpec(mFileSpec);

                    if (NS_SUCCEEDED(rv))
                    {
#ifdef XP_MAC
                        *mFileSpec += "Plugins";
#else
                        *mFileSpec += "plugins";
#endif
                    }
                    else
                        mFileSpec = nsnull;
                }
                break; 

            case 101: ///////////////////////////////////////////////////////////  Program
            case 102: ///////////////////////////////////////////////////////////  Communicator
                if (!nsSoftwareUpdate::GetProgramDirectory())
                    mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::OS_CurrentProcessDirectory ));
                else
                {
                    mFileSpec = new nsFileSpec();
                    if ( !mFileSpec )
                        rv = NS_ERROR_OUT_OF_MEMORY;
                    else
                        rv = nsSoftwareUpdate::GetProgramDirectory()->GetFileSpec(mFileSpec);

                    if (!NS_SUCCEEDED(rv))
                        mFileSpec = nsnull;
                }
                break;
            
            case 103: ///////////////////////////////////////////////////////////  User Pick
                // we should never be here.
                mFileSpec = nsnull;
                break;

            case 104: ///////////////////////////////////////////////////////////  Temporary
                mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::OS_TemporaryDirectory ));
                break;

            case 105: ///////////////////////////////////////////////////////////  Installed
                // we should never be here.
                mFileSpec = nsnull;
                break;

            case 106: ///////////////////////////////////////////////////////////  Current User
                SetAppShellDirectory(nsSpecialFileSpec::App_UserProfileDirectory50 );
                break;

            case 107: ///////////////////////////////////////////////////////////  Preferences
                SetAppShellDirectory(nsSpecialFileSpec::App_PrefsDirectory50 );
                break;

            case 108: ///////////////////////////////////////////////////////////  OS Drive
                mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::OS_DriveDirectory ));
                break;

            case 109: ///////////////////////////////////////////////////////////  File URL
                {
                    if (aRelativePath.IsEmpty())
                    {
                        mFileSpec = nsnull;
                        return;
                    }

                    nsString tempFileURLString = aFolderID;
                    tempFileURLString += aRelativePath;
                    mFileSpec = new nsFileSpec( nsFileURL(tempFileURLString) );

                    // file:// is a special case where it returns and does not
                    // go to the standard relative path code below.  This is
                    // so that nsFile(Spec|Path) will work properly.  (ie. Passing
                    // just "file://" to the nsFileSpec && nsFileURL is wrong).

                    return;

                }
                break;

            case 110: ///////////////////////////////////////////////////////////  Components
                if (!nsSoftwareUpdate::GetProgramDirectory())
                    SetAppShellDirectory(nsSpecialFileSpec::App_ComponentsDirectory );
                else
                {
                    mFileSpec = new nsFileSpec();
                    if ( !mFileSpec )
                        rv = NS_ERROR_OUT_OF_MEMORY;
                    else
                        rv = nsSoftwareUpdate::GetProgramDirectory()->GetFileSpec(mFileSpec);

                    if (NS_SUCCEEDED(rv))
                    {
#ifdef XP_MAC
                        *mFileSpec += "Components";
#else
                        *mFileSpec += "components";
#endif
                    }
                }
                break;
            
            case 111: ///////////////////////////////////////////////////////////  Chrome
                if (!nsSoftwareUpdate::GetProgramDirectory())
                    SetAppShellDirectory(nsSpecialFileSpec::App_ChromeDirectory );
                else
                {
                    mFileSpec = new nsFileSpec();
                    if ( !mFileSpec )
                        rv = NS_ERROR_OUT_OF_MEMORY;
                    else
                        rv = nsSoftwareUpdate::GetProgramDirectory()->GetFileSpec(mFileSpec);

                    if (NS_SUCCEEDED(rv))
                    {
#ifdef XP_MAC
                        *mFileSpec += "Chrome";
#else
                        *mFileSpec += "chrome";
#endif
                    }
                }
                break;

            case 200: ///////////////////////////////////////////////////////////  Win System
                mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::Win_SystemDirectory ));
                break;

            case 201: ///////////////////////////////////////////////////////////  Windows
                mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::Win_WindowsDirectory ));
                break;

            case 300: ///////////////////////////////////////////////////////////  Mac System
                mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::Mac_SystemDirectory ));
                break;

            case 301: ///////////////////////////////////////////////////////////  Mac Desktop
                mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::Mac_DesktopDirectory ));
                break;

            case 302: ///////////////////////////////////////////////////////////  Mac Trash
                mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::Mac_TrashDirectory ));
                break;

            case 303: ///////////////////////////////////////////////////////////  Mac Startup
                mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::Mac_StartupDirectory ));
                break;

            case 304: ///////////////////////////////////////////////////////////  Mac Shutdown
                mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::Mac_ShutdownDirectory ));
                break;

            case 305: ///////////////////////////////////////////////////////////  Mac Apple Menu
                mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::Mac_AppleMenuDirectory ));
                break;

            case 306: ///////////////////////////////////////////////////////////  Mac Control Panel
                mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::Mac_ControlPanelDirectory ));
                break;

            case 307: ///////////////////////////////////////////////////////////  Mac Extension
                mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::Mac_ExtensionDirectory ));
                break;

            case 308: ///////////////////////////////////////////////////////////  Mac Fonts
                mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::Mac_FontsDirectory ));
                break;

            case 309: ///////////////////////////////////////////////////////////  Mac Preferences
                mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::Mac_PreferencesDirectory ));
                break;
                    
            case 310: ///////////////////////////////////////////////////////////  Mac Documents
                mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::Mac_DocumentsDirectory ));
                break;
                    
            case 400: ///////////////////////////////////////////////////////////  Unix Local
                mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::Unix_LocalDirectory ));
                break;

            case 401: ///////////////////////////////////////////////////////////  Unix Lib
                mFileSpec = new nsFileSpec( nsSpecialSystemDirectory( nsSpecialSystemDirectory::Unix_LibDirectory ));
                break;


            case -1:
		    default:
                mFileSpec = nsnull;
			   return;
		}

        if (aRelativePath.Length() > 0 && mFileSpec)
        {
            nsString tempPath(aRelativePath);

            if (aRelativePath.Last() != '/' || aRelativePath.Last() != '\\')
                tempPath += '/';

            *mFileSpec += tempPath;
        }
    }
}

void nsInstallFolder::PickDefaultDirectory()
{
    //FIX:  Need to put up a dialog here and set mFileSpec
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

void
nsInstallFolder::SetAppShellDirectory(PRUint32 value)
{
    nsIFileSpec* fs = NS_LocateFileOrDirectory(value);
    if ( fs )
    {
        mFileSpec = new nsFileSpec();
        fs->GetFileSpec(mFileSpec);
	    NS_RELEASE(fs);
    }
}
