/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsDirectoryService.h"
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


MOZ_DECL_CTOR_COUNTER(nsInstallFolder);

nsInstallFolder::nsInstallFolder()
{
    MOZ_COUNT_CTOR(nsInstallFolder);
}

nsresult
nsInstallFolder::Init(const nsString& aFolderID, const nsString& aRelativePath)
{

    PRBool flagIsDir, flagExists;
  
    mFileSpec = nsnull;

    /*
        aFolderID can be either a Folder enum in which case we merely pass it
        to SetDirectoryPath, or it can be a Directory.  If it is the later, it
        must already exist and of course be a directory not a file.  
    */

    SetDirectoryPath( aFolderID, aRelativePath );

    if (mFileSpec)
        return NS_OK;

    // it didn't, so aFolderID is not one of the magic strings.
    // maybe it's already a pathname? If so it had better be a directory
    // if it already exists...
    nsAutoCString tempFolderID(aFolderID);
    nsCOMPtr<nsILocalFile> dirCheck;
    NS_NewLocalFile(tempFolderID, getter_AddRefs(dirCheck));
    
    if (!dirCheck)
        return NS_ERROR_FAILURE;

    dirCheck->IsDirectory(&flagIsDir);
    dirCheck->Exists(&flagExists);

    if ( flagIsDir && flagExists )
    {
        mFileSpec = dirCheck;

        if (aRelativePath.Length() > 0 )
        {
            // we've got a subdirectory to tack on
            nsString morePath(aFolderID);
            morePath.Append(aRelativePath);
            nsAutoCString tempMorePath(morePath);
            NS_NewLocalFile(tempMorePath, getter_AddRefs(dirCheck));

            mFileSpec = dirCheck;
        }
    }
    if (mFileSpec)
        return NS_OK;

    return NS_ERROR_FAILURE;
}

nsresult
nsInstallFolder::Init(nsInstallFolder& inFolder, const nsString& subString)
{
    if (!inFolder.mFileSpec)
        return NS_ERROR_NULL_POINTER;

    inFolder.mFileSpec->Clone(getter_AddRefs(mFileSpec));
    
    if (!mFileSpec)
        return NS_ERROR_FAILURE;

    if(!subString.IsEmpty())
    {
      nsAutoCString tempSubString(subString);
      mFileSpec->Append(tempSubString);
    }

    return NS_OK;
}


nsInstallFolder::~nsInstallFolder()
{
   MOZ_COUNT_DTOR(nsInstallFolder);
}

void 
nsInstallFolder::GetDirectoryPath(nsString& aDirectoryPath)
{
	PRBool flagIsDir;
  char*  thePath;
  
  aDirectoryPath.SetLength(0);
    
    if (mFileSpec != nsnull)
    {
      // We want the a NATIVE path.
      mFileSpec->GetPath(&thePath);  
      aDirectoryPath.AssignWithConversion(thePath);

      mFileSpec->IsDirectory(&flagIsDir);
      if (flagIsDir)
      {
        if (aDirectoryPath.Last() != FILESEP)
            aDirectoryPath.AppendWithConversion(FILESEP);
      }
    }
}

void
nsInstallFolder::SetDirectoryPath(const nsString& aFolderID, const nsString& aRelativePath)
{
    if ( aFolderID.EqualsIgnoreCase("Installed") )
    {   
        // XXX block from users or remove "Installed"
        // XXX the filespec creation will fail due to unix slashes on Mac
        nsAutoCString tempRelPath(aRelativePath);
        nsCOMPtr<nsILocalFile> temp;
        NS_NewLocalFile(tempRelPath, getter_AddRefs(temp));
        mFileSpec = temp;  // creates the directories to the relative path.
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
                    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                    if (!directoryService) return;

                    directoryService->Get("system.OS_CurrentProcessDirectory", NS_GET_IID(nsIFile), getter_AddRefs(mFileSpec));
#ifdef XP_MAC
                    mFileSpec->Append("Plugins");
#else
                    mFileSpec->Append("plugins");
#endif
                }
                else
                {
                    rv = nsSoftwareUpdate::GetProgramDirectory()->Clone(getter_AddRefs(mFileSpec));

                    if (NS_SUCCEEDED(rv))
                    {
#ifdef XP_MAC
                        mFileSpec->Append("Plugins");
#else
                        mFileSpec->Append("plugins");
#endif
                    }
                    else
                        mFileSpec = nsnull;
                }
                break; 

            case 101: ///////////////////////////////////////////////////////////  Program
            case 102: ///////////////////////////////////////////////////////////  Communicator
                if (!nsSoftwareUpdate::GetProgramDirectory())  //Not in stub installer
                {
                    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                    if (!directoryService) return;
                    directoryService->Get("system.OS_CurrentProcessDirectory", NS_GET_IID(nsIFile), getter_AddRefs(mFileSpec));
                }
                else //In stub installer.  mProgram has been set so 
                {
                    rv = nsSoftwareUpdate::GetProgramDirectory()->Clone(getter_AddRefs(mFileSpec));
                }
                break;
            
            case 103: ///////////////////////////////////////////////////////////  User Pick
                // we should never be here.
                mFileSpec = nsnull;
                break;

            case 104: ///////////////////////////////////////////////////////////  Temporary
              {
                NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                if (!directoryService) return;
                directoryService->Get("system.OS_TemporaryDirectory", NS_GET_IID(nsIFile), getter_AddRefs(mFileSpec));
              }
              break;

            case 105: ///////////////////////////////////////////////////////////  Installed
                // we should never be here.
                mFileSpec = nsnull;
                break;

            case 106: ///////////////////////////////////////////////////////////  Current User
                //nsIFileXXX: User profile dir not implemented in nsDirectoryService
                //SetAppShellDirectory(nsSpecialFileSpec::App_UserProfileDirectory50 );
                break;

            case 107: ///////////////////////////////////////////////////////////  Preferences
                //nsIFileXXX: User profile dir not implemented in nsDirectoryService
                //SetAppShellDirectory(nsSpecialFileSpec::App_PrefsDirectory50 );
                break;

            case 108: ///////////////////////////////////////////////////////////  OS Drive
              {
                  NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                  if (!directoryService) return;
                  directoryService->Get("system.OS_DriveDirectory", NS_GET_IID(nsIFile), getter_AddRefs(mFileSpec));
              }
                break;

            case 109: ///////////////////////////////////////////////////////////  File URL
                {
                    nsAutoCString tempRelPath(aRelativePath);
                    nsCOMPtr<nsILocalFile> temp;
                    NS_NewLocalFile(tempRelPath, getter_AddRefs(temp));
                    mFileSpec = temp;

                    // file:// is a special case where it returns and does not
                    // go to the standard relative path code below.  This is
                    // so that nsFile(Spec|Path) will work properly.  (ie. Passing
                    // just "file://" to the nsFileSpec && nsFileURL is wrong).

                    return;

                }
                break;

            case 110: ///////////////////////////////////////////////////////////  Components
                if (!nsSoftwareUpdate::GetProgramDirectory())
                {
                    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                    if (!directoryService) return;
                    directoryService->Get("system.OS_CurrentProcessDirectory", 
                                           NS_GET_IID(nsIFile), 
                                           getter_AddRefs(mFileSpec));

                    if (NS_SUCCEEDED(rv))
                    {
#ifdef XP_MAC
                        mFileSpec->Append("Components");
#else
                        mFileSpec->Append("components");
#endif
                    }
                }
                else
                {
                    rv = nsSoftwareUpdate::GetProgramDirectory()->Clone(getter_AddRefs(mFileSpec));

                    if (NS_SUCCEEDED(rv))
                    {
#ifdef XP_MAC
                        mFileSpec->Append("Components");
#else
                        mFileSpec->Append("components");
#endif
                    }
                    else
                      mFileSpec = nsnull;
                }
                break;
            
            case 111: ///////////////////////////////////////////////////////////  Chrome
                if (!nsSoftwareUpdate::GetProgramDirectory())
                { 
                    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                    if (!directoryService) return;
                    directoryService->Get("system.OS_CurrentProcessDirectory", 
                                           NS_GET_IID(nsIFile), 
                                           getter_AddRefs(mFileSpec));


                    if (NS_SUCCEEDED(rv))
                    {
#ifdef XP_MAC
                        mFileSpec->Append("Chrome");
#else
                        mFileSpec->Append("chrome");
#endif
                    }
                }
                else
                {
                    rv = nsSoftwareUpdate::GetProgramDirectory()->Clone(getter_AddRefs(mFileSpec));

                    if (NS_SUCCEEDED(rv))
                    {
#ifdef XP_MAC
                        mFileSpec->Append("Chrome");
#else
                        mFileSpec->Append("chrome");
#endif
                    }
                }
                break;

            case 200: ///////////////////////////////////////////////////////////  Win System
              {  
                  NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                  if (!directoryService) return;
                  directoryService->Get("system.SystemDirectory", 
                                         NS_GET_IID(nsIFile), 
                                         getter_AddRefs(mFileSpec));

              }
                  break;

            case 201: ///////////////////////////////////////////////////////////  Windows
              {
                   NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                   if (!directoryService) return;
                   directoryService->Get("system.WindowsDirectory", 
                                          NS_GET_IID(nsIFile), 
                                          getter_AddRefs(mFileSpec));
              }
                break;

            case 300: ///////////////////////////////////////////////////////////  Mac System
              {   
                    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                    if (!directoryService) return;
                    directoryService->Get("system.Directory", 
                                           NS_GET_IID(nsIFile), 
                                           getter_AddRefs(mFileSpec));
              }
                break;

            case 301: ///////////////////////////////////////////////////////////  Mac Desktop
              {  
                    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                    if (!directoryService) return;
                    directoryService->Get("system.DesktopDirectory", 
                                           NS_GET_IID(nsIFile), 
                                           getter_AddRefs(mFileSpec));
              }
                break;

            case 302: ///////////////////////////////////////////////////////////  Mac Trash
              {  
                    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                    if (!directoryService) return;
                    directoryService->Get("system.TrashDirectory", 
                                           NS_GET_IID(nsIFile), 
                                           getter_AddRefs(mFileSpec));
              }
                break;

            case 303: ///////////////////////////////////////////////////////////  Mac Startup
              {  
                    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                    if (!directoryService) return;
                    directoryService->Get("system.StartupDirectory", 
                                           NS_GET_IID(nsIFile), 
                                           getter_AddRefs(mFileSpec));
              }
                break;

            case 304: ///////////////////////////////////////////////////////////  Mac Shutdown
              {  
                    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                    if (!directoryService) return;
                    directoryService->Get("system.ShutdownDirectory", 
                                           NS_GET_IID(nsIFile), 
                                           getter_AddRefs(mFileSpec));
              }
                break;

            case 305: ///////////////////////////////////////////////////////////  Mac Apple Menu
              {  
                    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                    if (!directoryService) return;
                    directoryService->Get("system.AppleMenuDirectory", 
                                           NS_GET_IID(nsIFile), 
                                           getter_AddRefs(mFileSpec));
              }
                break;

            case 306: ///////////////////////////////////////////////////////////  Mac Control Panel
              {  
                    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                    if (!directoryService) return;
                    directoryService->Get("system.ControlPanelDirectory", 
                                           NS_GET_IID(nsIFile), 
                                           getter_AddRefs(mFileSpec));
              }
                break;

            case 307: ///////////////////////////////////////////////////////////  Mac Extension
              {  
                    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                    if (!directoryService) return;
                    directoryService->Get("system.ExtensionDirectory", 
                                           NS_GET_IID(nsIFile), 
                                           getter_AddRefs(mFileSpec));
              }
                break;

            case 308: ///////////////////////////////////////////////////////////  Mac Fonts
              {  
                    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                    if (!directoryService) return;
                    directoryService->Get("system.FontsDirectory", 
                                           NS_GET_IID(nsIFile), 
                                           getter_AddRefs(mFileSpec));
              }
                break;

            case 309: ///////////////////////////////////////////////////////////  Mac Preferences
              {  
                    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                    if (!directoryService) return;
                    directoryService->Get("system.PreferencesDirectory", 
                                           NS_GET_IID(nsIFile), 
                                           getter_AddRefs(mFileSpec));
              }
                break;
                    
            case 310: ///////////////////////////////////////////////////////////  Mac Documents
              {  
                    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                    if (!directoryService) return;
                    directoryService->Get("system.DocumentsDirectory", 
                                           NS_GET_IID(nsIFile), 
                                           getter_AddRefs(mFileSpec));
              }
                break;
                    
            case 400: ///////////////////////////////////////////////////////////  Unix Local
              {  
                    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                    if (!directoryService) return;
                    directoryService->Get("system.LocalDirectory", 
                                           NS_GET_IID(nsIFile), 
                                           getter_AddRefs(mFileSpec));
              }
                break;

            case 401: ///////////////////////////////////////////////////////////  Unix Lib
              {  
                    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
                    if (!directoryService) return;
                    directoryService->Get("system.LibDirectory", 
                                           NS_GET_IID(nsIFile), 
                                           getter_AddRefs(mFileSpec));
              }
                break;


            case -1:
		    default:
                mFileSpec = nsnull;
			   return;
		}

        if (aRelativePath.Length() > 0 && mFileSpec)
        {
            //nsString tempPath(aRelativePath);

            //if (aRelativePath.Last() != '/' || aRelativePath.Last() != '\\')
            //    tempPath.AppendWithConversion('/');
            nsAutoCString tempRelPath(aRelativePath);
            mFileSpec->Append(tempRelPath);
        }
    }
}


/* MapNameToEnum
 * maps name from the directory table to its enum */
PRInt32 
nsInstallFolder::MapNameToEnum(const nsString& name)
{
	int i = 0;

	if ( name.IsEmpty())
        return -1;

	while ( DirectoryTable[i].directoryName[0] != 0 )
	{
		if ( name.EqualsIgnoreCase(DirectoryTable[i].directoryName) )
			return DirectoryTable[i].folderEnum;
		i++;
	}
	return -1;
}



nsIFile*
nsInstallFolder::GetFileSpec()
{
  return mFileSpec;
}

PRInt32
nsInstallFolder::ToString(nsAutoString* outString)
{
  //XXX: May need to fix. Native charset paths will be converted into Unicode when the get to JS
  //     This will appear to work on Latin-1 charsets but won't work on Mac or other charsets.

  char* temp;
  PRBool flagIsFile;

  if (!mFileSpec)
      return NS_ERROR_NULL_POINTER;

  nsresult rv = mFileSpec->GetPath(&temp);
  mFileSpec->IsFile(&flagIsFile);
  if (!flagIsFile)
  {
      nsString tempString; 
      tempString.AssignWithConversion(temp);
      tempString.AppendWithConversion(FILESEP);
      nsAutoCString tempAutoString(tempString);
      outString->AssignWithConversion(tempAutoString);
  }
  else
      outString->AssignWithConversion(temp);
  return rv;
}
