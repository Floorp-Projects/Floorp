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

#include "nsSoftwareUpdate.h"

#include "nsInstallFolder.h"
#include "nsIDOMInstallFolder.h"

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIScriptGlobalObject.h"

#include "prefapi.h"
#include "pratom.h"
#include "prprf.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

static NS_DEFINE_IID(kIInstallFolder_IID, NS_IDOMINSTALLFOLDER_IID);



#ifndef MAX_PATH
	#if defined(XP_WIN) || defined(XP_OS2)
		#define MAX_PATH _MAX_PATH
	#endif
	#ifdef XP_UNIX
		#if defined(HPUX) || defined(SCO)
			/*
			** HPUX: PATH_MAX is defined in <limits.h> to be 1023, but they
			**       recommend that it not be used, and that pathconf() be
			**       used to determine the maximum at runtime.
			** SCO:  This is what MAXPATHLEN is set to in <arpa/ftp.h> and
			**       NL_MAXPATHLEN in <nl_types.h>.  PATH_MAX is defined in
			**       <limits.h> to be 256, but the comments in that file
			**       claim the setting is wrong.
			*/
			#define MAX_PATH 1024
		#else
			#define MAX_PATH PATH_MAX
		#endif
	#endif
#endif

typedef enum su_SecurityLevel {
	eOneFolderAccess,
	eAllFolderAccess
} su_SecurityLevel;

struct su_DirectoryTable
{
	char *  directoryName;			/* The formal directory name */
	PRInt32 folderEnum;		        /* Directory ID */
	PRBool  bJavaDir;               /* TRUE is a Java-capable directory */
};


/* 
 * Directory manipulation 
 *
 * DirectoryTable holds the info about built-in directories:
 * Text name, security level, enum
 */
struct su_DirectoryTable DirectoryTable[] = 
{
    {"Plugins",         nsIDOMInstallFolder::PluginFolder,              PR_TRUE},
	{"Program",         nsIDOMInstallFolder::ProgramFolder,             PR_FALSE},
	{"Communicator",    nsIDOMInstallFolder::CommunicatorFolder,        PR_FALSE},
	{"User Pick",       nsIDOMInstallFolder::PackageFolder,             PR_FALSE},
	{"Temporary",       nsIDOMInstallFolder::TemporaryFolder,           PR_FALSE},
	{"Installed",       nsIDOMInstallFolder::InstalledFolder,           PR_FALSE},
	{"Current User",    nsIDOMInstallFolder::CurrentUserFolder,         PR_FALSE},

	{"NetHelp",         nsIDOMInstallFolder::NetHelpFolder,             PR_FALSE},
	{"OS Drive",        nsIDOMInstallFolder::OSDriveFolder,             PR_FALSE},
	{"File URL",        nsIDOMInstallFolder::FileURLFolder,             PR_FALSE},

	{"Netscape Java Bin",     nsIDOMInstallFolder::JavaBinFolder,       PR_FALSE},
	{"Netscape Java Classes", nsIDOMInstallFolder::JavaClassesFolder,   PR_TRUE},
	{"Java Download",         nsIDOMInstallFolder::JavaDownloadFolder,  PR_TRUE},

	{"Win System",      nsIDOMInstallFolder::Win_SystemFolder,          PR_FALSE},
	{"Win System16",    nsIDOMInstallFolder::Win_System16Folder,        PR_FALSE},
	{"Windows",         nsIDOMInstallFolder::Win_WindowsFolder,         PR_FALSE},

	{"Mac System",      nsIDOMInstallFolder::Mac_SystemFolder,          PR_FALSE},
	{"Mac Desktop",     nsIDOMInstallFolder::Mac_DesktopFolder,         PR_FALSE},
	{"Mac Trash",       nsIDOMInstallFolder::Mac_TrashFolder,           PR_FALSE},
	{"Mac Startup",     nsIDOMInstallFolder::Mac_StartupFolder,         PR_FALSE},
	{"Mac Shutdown",    nsIDOMInstallFolder::Mac_ShutdownFolder,        PR_FALSE},
	{"Mac Apple Menu",  nsIDOMInstallFolder::Mac_AppleMenuFolder,       PR_FALSE},
	{"Mac Control Panel", nsIDOMInstallFolder::Mac_ControlPanelFolder,  PR_FALSE},
	{"Mac Extension",   nsIDOMInstallFolder::Mac_ExtensionFolder,       PR_FALSE},
	{"Mac Fonts",       nsIDOMInstallFolder::Mac_FontsFolder,           PR_FALSE},
	{"Mac Preferences", nsIDOMInstallFolder::Mac_PreferencesFolder,     PR_FALSE},

	{"Unix Local",      nsIDOMInstallFolder::Unix_LocalFolder,          PR_FALSE},
	{"Unix Lib",        nsIDOMInstallFolder::Unix_LibFolder,            PR_FALSE},

	{"",                nsIDOMInstallFolder::BadFolder,                 PR_FALSE} /* Termination */
};


nsInstallFolder::nsInstallFolder()
{
    mScriptObject   = nsnull;

	char *errorMsg = NULL;
	mUrlPath = mFolderID = mVersionRegistryPath = mUserPackageName = nsnull;

    NS_INIT_REFCNT();
}

nsInstallFolder::~nsInstallFolder()
{
}

NS_IMETHODIMP 
nsInstallFolder::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    // Always NULL result, in case of failure
    *aInstancePtr = NULL;

    if ( aIID.Equals(kIScriptObjectOwnerIID))
    {
        *aInstancePtr = (void*) ((nsIScriptObjectOwner*)this);
        AddRef();
        return NS_OK;
    }
    else if ( aIID.Equals(kIInstallFolder_IID) )
    {
        *aInstancePtr = (void*) ((nsIDOMInstallFolder*)this);
        AddRef();
        return NS_OK;
    }
    else if ( aIID.Equals(kISupportsIID) )
    {
        *aInstancePtr = (void*)(nsISupports*)(nsIScriptObjectOwner*)this;
        AddRef();
        return NS_OK;
    }

     return NS_NOINTERFACE;
}


NS_IMPL_ADDREF(nsInstallFolder)
NS_IMPL_RELEASE(nsInstallFolder)



NS_IMETHODIMP 
nsInstallFolder::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
    NS_PRECONDITION(nsnull != aScriptObject, "null arg");
    nsresult res = NS_OK;
    
    if (nsnull == mScriptObject) 
    {
        res = NS_NewScriptInstallFolder( aContext, 
                                         (nsISupports *)(nsIDOMInstallFolder*)this, 
                                         nsnull, 
                                         &mScriptObject);
    }
  

    *aScriptObject = mScriptObject;
    return res;
}

NS_IMETHODIMP 
nsInstallFolder::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

//  this will go away when our constructors can have parameters.

NS_IMETHODIMP 
nsInstallFolder::Init(const nsString& aFolderID, const nsString& aVrPatch, const nsString& aPackageName)
{
    /* Since urlPath is set to NULL, this FolderSpec is essentially the error message */
	
    if ( aFolderID == "null" || aVrPatch == "null" || aPackageName == "null") 
    {
        return NS_OK; // should we stop the script?
    }
    
    mFolderID            = new nsString(aFolderID);
	mVersionRegistryPath = new nsString(aVrPatch);
	mUserPackageName     = new nsString(aPackageName);
    
    /* Setting the urlPath to a real file patch. */
	SetDirectoryPath( &mUrlPath );

    return NS_OK;
}
        
NS_IMETHODIMP 
nsInstallFolder::GetDirectoryPath(nsString& aDirectoryPath)
{
    aDirectoryPath.SetLength(0);
    aDirectoryPath.Append(*mUrlPath);
    return NS_OK;
}

NS_IMETHODIMP 
nsInstallFolder::MakeFullPath(const nsString& aRelativePath, nsString& aFullPath)
{
    nsString *tempString = GetNativePath(aRelativePath);
    aFullPath.SetLength(0);
    aFullPath.Append( *mUrlPath );
    aFullPath.Append( *tempString );

    if (tempString) 
        delete tempString;

    return NS_OK;
}

NS_IMETHODIMP 
nsInstallFolder::IsJavaCapable(PRBool* aReturn)
{
    *aReturn =  PR_FALSE; // FIX: what are we going to do here.
    return -1;
}

NS_IMETHODIMP 
nsInstallFolder::ToString(nsString& aFolderString)
{
    return GetDirectoryPath(aFolderString);
}

void
nsInstallFolder::SetDirectoryPath(nsString** aFolderString)
{
    if ( mFolderID->EqualsIgnoreCase("User Pick") )
    {
        PickDefaultDirectory(&mUrlPath);
    }
    else if ( mFolderID->EqualsIgnoreCase("Installed") )
    {
        mUrlPath = mVersionRegistryPath->ToNewString();
    }
    else
    {
        PRInt32 folderDirSpecID;
		char*	folderPath = NULL;
		
		folderDirSpecID = MapNameToEnum(mFolderID);
        
        switch (folderDirSpecID) 
		{
            case nsIDOMInstallFolder::BadFolder:
                folderPath = NULL;
                break;
            
            case nsIDOMInstallFolder::CurrentUserFolder:
				{
					char dir[MAX_PATH];
					int len = MAX_PATH;
					if ( PREF_GetCharPref("profile.directory", dir, &len) == PREF_NOERROR)      
					{
//						char * platformDir = WH_FileName(dir, xpURL);
//						if (platformDir)
//							folderPath = AppendSlashToDirPath(platformDir);
//						PR_FREEIF(platformDir);
					}
				}
				break;

				default:
					/* Get the FE path */
					//      folderPath = FE_GetDirectoryPath(folderDirSpecID);
					break;
		}
    }
}

nsString* nsInstallFolder::GetNativePath(const nsString& path)
{
    char pathSeparator;
    char xp_pathSeparator =  '/';

#ifdef XP_WIN
  pathSeparator = '\\';
#elif defined(XP_MAC)
  pathSeparator = ':';
#else /* XP_UNIX */
  pathSeparator = '/';
#endif
    
    nsString *xpPath = new nsString(path);
    PRInt32 offset   = xpPath->FindCharInSet(&xp_pathSeparator);
    
    while (offset != -1)
    {
        xpPath[offset] =   pathSeparator;
        offset = xpPath->FindCharInSet(&xp_pathSeparator, offset); 
    }

  return xpPath;
}

void nsInstallFolder::PickDefaultDirectory(nsString** aFolderString)
{
    return;   //FIX:  Need to put up a dialog here!
}

PRBool nsInstallFolder::IsJavaDir(void)
{  
    for (int i=0; DirectoryTable[i].directoryName[0] != 0; i++ ) 
    {
        if ( mFolderID->EqualsIgnoreCase(DirectoryTable[i].directoryName) )
            return DirectoryTable[i].bJavaDir;
    }
    return PR_FALSE;
}



/* MapNameToEnum
 * maps name from the directory table to its enum */
PRInt32 
nsInstallFolder::MapNameToEnum(nsString* name)
{
	int i = 0;

	if ( name == nsnull )
        return nsIDOMInstallFolder::BadFolder;

	while ( DirectoryTable[i].directoryName[0] != 0 )
	{
		if ( name->EqualsIgnoreCase(DirectoryTable[i].directoryName) )
			return DirectoryTable[i].folderEnum;
		i++;
	}
	return nsIDOMInstallFolder::BadFolder;
}



/* 
     Makes sure that the path ends with a slash (or other platform end character)
 */

void
nsInstallFolder::AppendSlashToDirPath(nsString* dirPath)
{
    char pathSeparator;

#ifdef XP_WIN
    pathSeparator = '\\';
#elif defined(XP_MAC)
    pathSeparator = ':';
#else /* XP_UNIX */
    pathSeparator = '/';
#endif

    if ( dirPath->CharAt( dirPath->Last() ) != pathSeparator )
    {
        dirPath += pathSeparator;
    }
}









/////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////
static PRInt32 gInstallFolderInstanceCnt = 0;
static PRInt32 gInstallFolderLock        = 0;

nsInstallFolderFactory::nsInstallFolderFactory(void)
{
    mRefCnt=0;
    PR_AtomicIncrement(&gInstallFolderInstanceCnt);
}

nsInstallFolderFactory::~nsInstallFolderFactory(void)
{
    PR_AtomicDecrement(&gInstallFolderInstanceCnt);
}

NS_IMETHODIMP 
nsInstallFolderFactory::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    // Always NULL result, in case of failure
    *aInstancePtr = NULL;

    if ( aIID.Equals(kISupportsIID) )
    {
        *aInstancePtr = (void*) this;
    }
    else if ( aIID.Equals(kIFactoryIID) )
    {
        *aInstancePtr = (void*) this;
    }

    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NO_INTERFACE;
    }

    AddRef();
    return NS_OK;
}



NS_IMETHODIMP
nsInstallFolderFactory::AddRef(void)
{
    return ++mRefCnt;
}


NS_IMETHODIMP
nsInstallFolderFactory::Release(void)
{
    if (--mRefCnt ==0)
    {
        delete this;
        return 0; // Don't access mRefCnt after deleting!
    }

    return mRefCnt;
}

NS_IMETHODIMP
nsInstallFolderFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aResult == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aResult = NULL;

    /* do I have to use iSupports? */
    nsInstallFolder *inst = new nsInstallFolder();

    if (inst == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult result =  inst->QueryInterface(aIID, aResult);

    if (result != NS_OK)
        delete inst;

    return result;
}

NS_IMETHODIMP
nsInstallFolderFactory::LockFactory(PRBool aLock)
{
    if (aLock)
        PR_AtomicIncrement(&gInstallFolderLock);
    else
        PR_AtomicDecrement(&gInstallFolderLock);

    return NS_OK;
}