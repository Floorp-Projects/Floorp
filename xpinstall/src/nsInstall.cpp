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



#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"

#include "nsVector.h"
#include "nsHashtable.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"

#include "nsIPref.h"

#include "prmem.h"
#include "plstr.h"

#include "VerReg.h"
//#include "zipfile.h" // replaced by nsIJAR.h

#include "nsInstall.h"
#include "nsInstallFolder.h"
#include "nsInstallVersion.h"
#include "nsInstallFile.h"
#include "nsInstallDelete.h"
#include "nsInstallExecute.h"
#include "nsInstallPatch.h"
#include "nsInstallUninstall.h"
#ifdef NECKO
#include "nsNeckoUtil.h"
#endif

#ifdef _WINDOWS
#include "nsWinReg.h"
#include "nsWinProfile.h"
#endif

#include "nsInstallFileOpEnums.h"
#include "nsInstallFileOpItem.h"

#ifdef XP_MAC
#include "Gestalt.h"
#endif 

#ifdef XP_PC
#define FILESEP "\\"
#elif defined(XP_MAC)
#define FILESEP ":"
#else
#define FILESEP "/"
#endif

static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
#ifdef NECKO
#else
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
#endif
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);
	

nsInstallInfo::nsInstallInfo(nsIFileSpec* aFile, 
                             const PRUnichar* aArgs, 
                             long flags, 
                             nsIXPINotifier* aNotifier)
: mError(0), 
  mFlags(flags), 
  mArgs(aArgs), 
  mFile(aFile), 
  mNotifier(aNotifier)
{}



nsInstallInfo::~nsInstallInfo()
{
    VR_Close();
}

void 
nsInstallInfo::GetLocalFile(char **aPath)
{
    if (mFile)
        mFile->GetNSPRPath(aPath);
    else
        *aPath = 0;
}




static NS_DEFINE_IID(kISoftwareUpdateIID, NS_ISOFTWAREUPDATE_IID);
static NS_DEFINE_IID(kSoftwareUpdateCID,  NS_SoftwareUpdate_CID);

static NS_DEFINE_IID(kIJARIID, NS_IJAR_IID);
static NS_DEFINE_IID(kJARCID,  NS_JAR_CID);

nsInstall::nsInstall()
{
    mScriptObject           = nsnull;           // this is the jsobject for our context
    mVersionInfo            = nsnull;           // this is the version information passed to us in StartInstall()             
    mRegistryPackageName    = "";               // this is the name that we will add into the registry for the component we are installing 
    mUIName                 = "";               // this is the name that will be displayed in UI.

    mUninstallPackage = PR_FALSE;
    mRegisterPackage  = PR_FALSE;

    mJarFileLocation    = "";
    mInstallArguments   = "";

    // mJarFileData is an opaque handle to the jarfile.
    nsresult rv = nsComponentManager::CreateInstance(kJARCID, nsnull, kIJARIID, 
                                                     (void**) &mJarFileData);

    nsISoftwareUpdate *su;
    rv = nsServiceManager::GetService(kSoftwareUpdateCID, 
                                               kISoftwareUpdateIID,
                                               (nsISupports**) &su);
    
    if (NS_SUCCEEDED(rv))
    {
        su->GetMasterNotifier( &mNotifier );
    }

    su->Release();
}

nsInstall::~nsInstall()
{
    if (mVersionInfo != nsnull)
        delete mVersionInfo;
}


nsInstall::SetScriptObject(void *aScriptObject)
{
  mScriptObject = (JSObject*) aScriptObject;
  return NS_OK;
}
#ifdef _WINDOWS
nsInstall::SaveWinRegPrototype(void *aScriptObject)
{
  mWinRegObject = (JSObject*) aScriptObject;
  return NS_OK;
}

nsInstall::SaveWinProfilePrototype(void *aScriptObject)
{
  mWinProfileObject = (JSObject*) aScriptObject;
  return NS_OK;
}

JSObject*
nsInstall::RetrieveWinRegPrototype()
{
  return mWinRegObject;
}

JSObject*
nsInstall::RetrieveWinProfilePrototype()
{
  return mWinProfileObject;
}
#endif

PRInt32    
nsInstall::GetUserPackageName(nsString& aUserPackageName)
{
    aUserPackageName = mUIName;
    return NS_OK;
}

PRInt32    
nsInstall::GetRegPackageName(nsString& aRegPackageName)
{
    aRegPackageName = mRegistryPackageName;
    return NS_OK;
}

PRInt32    
nsInstall::AbortInstall()
{
    if (mNotifier)
        mNotifier->InstallAborted();

    nsInstallObject* ie;
    if (mInstalledFiles != nsnull) 
    {
        PRUint32 i=0;
        for (i=0; i < mInstalledFiles->GetSize(); i++) 
        {
            ie = (nsInstallObject *)mInstalledFiles->Get(i);
            if (ie == nsnull)
                continue;
            ie->Abort();
        }
    }
    
    CleanUp();
    
    return NS_OK;
}

PRInt32    
nsInstall::AddDirectory(const nsString& aRegName, 
                        const nsString& aVersion, 
                        const nsString& aJarSource, 
                        const nsString& aFolder, 
                        const nsString& aSubdir, 
                        PRBool aForceMode, 
                        PRInt32* aReturn)
{
    nsInstallFile* ie = nsnull;
    PRInt32 result;
    
    if ( aJarSource == "null" || aFolder == "null") 
    {
        *aReturn = SaveError(nsInstall::INVALID_ARGUMENTS);
        return NS_OK;
    }

    result = SanityCheck();
    
    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
    
    nsString qualifiedRegName;
    
    if ( aRegName == "" || aRegName == "null") 
    {
        // Default subName = location in jar file
        *aReturn = GetQualifiedRegName( aJarSource, qualifiedRegName);
    } 
    else 
    {
        *aReturn = GetQualifiedRegName( aRegName, qualifiedRegName );
    }

    if (*aReturn != SUCCESS)
    {
        return NS_OK;
    }
    
    nsString subdirectory(aSubdir);

    if (subdirectory != "")
    {
        subdirectory.Append("/");
    }

    
    nsVector *paths = new nsVector();
    
    result = ExtractDirEntries(aJarSource, paths);
    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
    
    for (PRUint32 i=0; i < paths->GetSize(); i++)
    {
        nsString *thisPath = (nsString *)paths->Get(i);

        nsString newJarSource = aJarSource;
        newJarSource += "/";
        newJarSource += *thisPath;
        
        nsString fullRegName = qualifiedRegName;
        fullRegName += "/";
        fullRegName += *thisPath;
        

        nsString newSubDir;

        if (subdirectory != "")
        {
            newSubDir = subdirectory;
        }
        
        newSubDir += *thisPath;

        ie = new nsInstallFile( this,
                                fullRegName,
                                aVersion,
                                newJarSource,
                                aFolder,
                                newSubDir,
                                aForceMode,
                                &result);
        
        if (result == nsInstall::SUCCESS)
        {
            result = ScheduleForInstall( ie );
        }
        else
        {
            delete ie;
        }
    
    }
    
    DeleteVector(paths);

    *aReturn = SaveError( result );
    return NS_OK;
}

PRInt32    
nsInstall::AddDirectory(const nsString& aRegName, 
                        const nsString& aVersion, 
                        const nsString& aJarSource, 
                        const nsString& aFolder, 
                        const nsString& aSubdir, 
                        PRInt32* aReturn)
{
    return AddDirectory(aRegName, 
                        aVersion, 
                        aJarSource, 
                        aFolder, 
                        aSubdir, 
                        PR_FALSE,
                        aReturn);
}

PRInt32    
nsInstall::AddDirectory(const nsString& aRegName, 
                        const nsString& aJarSource, 
                        const nsString& aFolder, 
                        const nsString& aSubdir, 
                        PRInt32* aReturn)
{
    return AddDirectory(aRegName, 
                        "", 
                        aJarSource, 
                        aFolder, 
                        aSubdir, 
                        PR_FALSE,
                        aReturn);
}

PRInt32    
nsInstall::AddDirectory(const nsString& aJarSource,
                        PRInt32* aReturn)
{
    return AddDirectory("", 
                        "", 
                        aJarSource, 
                        "", 
                        "", 
                        PR_FALSE,
                        aReturn);

   
}

PRInt32    
nsInstall::AddSubcomponent(const nsString& aRegName, 
                           const nsString& aVersion, 
                           const nsString& aJarSource, 
                           const nsString& aFolder, 
                           const nsString& aTargetName, 
                           PRBool aForceMode, 
                           PRInt32* aReturn)
{
    nsInstallFile*  ie;
    nsString        qualifiedRegName;
    nsString        tempTargetName;
    
    PRInt32         errcode = nsInstall::SUCCESS;

    if((aTargetName == "") || (aTargetName == "null"))
    {
      tempTargetName = aJarSource;
    }
    else
    {
      tempTargetName = aTargetName;
    }
    
    if(aJarSource == "null" || aFolder == "null") 
    {
        *aReturn = SaveError( nsInstall::INVALID_ARGUMENTS );
        return NS_OK;
    }
    
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }


    if ( aRegName == "" || aRegName == "null") 
    {
        // Default subName = location in jar file
        *aReturn = GetQualifiedRegName( aJarSource, qualifiedRegName);
    } 
    else 
    {
        *aReturn = GetQualifiedRegName( aRegName, qualifiedRegName );
    }

    if (*aReturn != SUCCESS)
    {
        return NS_OK;
    }
    
   
    ie = new nsInstallFile( this, 
                            qualifiedRegName, 
                            aVersion, 
                            aJarSource,
                            aFolder,
                            tempTargetName, 
                            aForceMode, 
                            &errcode );

    if (errcode == nsInstall::SUCCESS) 
    {
        errcode = ScheduleForInstall( ie );
    }
    else
    {
        delete ie;
    }    

    *aReturn = SaveError( errcode );
    return NS_OK;
}

PRInt32    
nsInstall::AddSubcomponent(const nsString& aRegName, 
                           const nsString& aVersion, 
                           const nsString& aJarSource, 
                           const nsString& aFolder, 
                           const nsString& aTargetName, 
                           PRInt32* aReturn)
{
    return AddSubcomponent(aRegName, 
                           aVersion, 
                           aJarSource, 
                           aFolder, 
                           aTargetName, 
                           PR_FALSE, 
                           aReturn);
}

PRInt32    
nsInstall::AddSubcomponent(const nsString& aRegName, 
                           const nsString& aJarSource, 
                           const nsString& aFolder, 
                           const nsString& aTargetName, 
                           PRInt32* aReturn)
{
    return AddSubcomponent(aRegName, 
                           "", 
                           aJarSource, 
                           aFolder, 
                           aTargetName, 
                           PR_FALSE, 
                           aReturn);
}

PRInt32    
nsInstall::AddSubcomponent(const nsString& aJarSource,
                           PRInt32* aReturn)
{
    if(mPackageFolder == "null")
    {
        *aReturn = SaveError( nsInstall::PACKAGE_FOLDER_NOT_SET );
        return NS_OK;
    }

    return AddSubcomponent("", 
                           "", 
                           aJarSource, 
                           mPackageFolder, 
                           "",
                           PR_FALSE, 
                           aReturn);
      
    
}

PRInt32    
nsInstall::DeleteComponent(const nsString& aRegistryName, PRInt32* aReturn)
{
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }

    nsString qualifiedRegName;

    *aReturn = GetQualifiedRegName( aRegistryName, qualifiedRegName);
    
    if (*aReturn != SUCCESS)
    {
        return NS_OK;
    }
    
    nsInstallDelete* id = new nsInstallDelete(this, "", qualifiedRegName, &result);
    if (result == nsInstall::SUCCESS) 
    {
        result = ScheduleForInstall( id );
    }
    
    *aReturn = SaveError(result);

    return NS_OK;
}

PRInt32    
nsInstall::DeleteFile(const nsString& aFolder, const nsString& aRelativeFileName, PRInt32* aReturn)
{
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
   
    nsInstallDelete* id = new nsInstallDelete(this, aFolder, aRelativeFileName, &result);

    if (result == nsInstall::SUCCESS) 
    {
        result = ScheduleForInstall( id );
    }
        
    if (result == nsInstall::FILE_DOES_NOT_EXIST) 
    {
        result = nsInstall::SUCCESS;
    }

    *aReturn = SaveError(result);

    return NS_OK;
}

PRInt32    
nsInstall::DiskSpaceAvailable(const nsString& aFolder, PRInt32* aReturn)
{
    nsFileSpec fsFolder(aFolder);

    *aReturn = fsFolder.GetDiskSpaceAvailable();
    return NS_OK;
}

PRInt32    
nsInstall::Execute(const nsString& aJarSource, const nsString& aArgs, PRInt32* aReturn)
{
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
   
    nsInstallExecute* ie = new nsInstallExecute(this, aJarSource, aArgs, &result);

    if (result == nsInstall::SUCCESS) 
    {
        result = ScheduleForInstall( ie );
    }
        
    *aReturn = SaveError(result);
    return NS_OK;
}

PRInt32    
nsInstall::Execute(const nsString& aJarSource, PRInt32* aReturn)
{
    return Execute(aJarSource, "", aReturn);
}

PRInt32    
nsInstall::FinalizeInstall(PRInt32* aReturn)
{
    PRBool  rebootNeeded = PR_FALSE;

    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
    
    if ( mInstalledFiles == NULL || mInstalledFiles->GetSize() == 0 ) 
    {
        // no actions queued: don't register the package version
        // and no need for user confirmation
    
        CleanUp();
        return NS_OK; 
    }

    nsInstallObject* ie = nsnull;

    if ( mUninstallPackage )
    {
        VR_UninstallCreateNode( (char*)(const char*) nsAutoCString(mRegistryPackageName), 
                                (char*)(const char*) nsAutoCString(mUIName));
    }
      
    PRUint32 i=0;
    for (i=0; i < mInstalledFiles->GetSize(); i++) 
    {
        ie = (nsInstallObject*)mInstalledFiles->Get(i);
        if (ie == NULL)
            continue;
    
        char *objString = ie->toString();
        
        if (mNotifier)
            mNotifier->InstallFinalization(objString, i , mInstalledFiles->GetSize());

        delete [] objString;
    
        ie->Complete();

        if (result != nsInstall::SUCCESS) 
        {
            ie->Abort();
            *aReturn = SaveError( result );
            return NS_OK;
        }
    }

    *aReturn = NS_OK;
    return NS_OK;
}

PRInt32    
nsInstall::Gestalt(const nsString& aSelector, PRInt32* aReturn)
{
    *aReturn = nsnull;

#ifdef XP_MAC
	
    long response = 0;
    
    if (aSelector == "")
    {
        return NS_OK;
    }

	char selector[4];
	
    aSelector.ToCString((char*)&selector,4);

    Gestalt(selector, (int*)&response);

    *aReturn = response;
	
#endif    
    return NS_OK;    
}

PRInt32
nsInstall::GetComponentFolder(const nsString& aComponentName, const nsString& aSubdirectory, nsString** aFolder)
{
    long        err;
    char*       componentCString;
    char        dir[MAXREGPATHLEN];
    nsFileSpec  nsfsDir;

// FIX: aSubdirectory is not processed at all in this function.

    *aFolder = nsnull;

    nsString tempString;

    if ( GetQualifiedPackageName(aComponentName, tempString) != SUCCESS )
    {
        return NS_OK;
    }

    componentCString = tempString.ToNewCString();

    if((err = VR_GetDefaultDirectory( componentCString, MAXREGPATHLEN, dir )) != REGERR_OK)
    {
        if((err = VR_GetPath( componentCString, MAXREGPATHLEN, dir )) == REGERR_OK)
        {
            int i;

            nsString dirStr(dir);
            if (  (i = dirStr.RFind(FILESEP)) > 0 ) 
            {
                // i is the index in the string, not the total number of
                // characters in the string.  ToCString() requires the
                // total number of characters in the string to copy,
                // therefore add 1 to it.
                dirStr.ToCString(dir, i + 1);
            }
        }
        else
        {
            *dir = '\0';
        }
    }
    else
    {
        *dir = '\0';
    }

    if(*dir != '\0') 
    {
        nsfsDir = dir;
        if(aSubdirectory != "")
        {
          nsfsDir += aSubdirectory;
        }
        *aFolder  = new nsString(nsfsDir.GetNativePathCString());
    }

    delete [] componentCString;
    return NS_OK;
}

PRInt32    
nsInstall::GetComponentFolder(const nsString& aComponentName, nsString** aFolder)
{
    return GetComponentFolder(aComponentName, "", aFolder);
}

PRInt32    
nsInstall::GetFolder(const nsString& targetFolder, const nsString& aSubdirectory, nsString** aFolder)
{
    nsInstallFolder* spec = nsnull;
    *aFolder = nsnull;

    spec = new nsInstallFolder(targetFolder, aSubdirectory);   
       
    nsString dirString;
    spec->GetDirectoryPath(dirString);

    *aFolder = new nsString(dirString);
    return NS_OK;    
}

PRInt32    
nsInstall::GetFolder(const nsString& targetFolder, nsString** aFolder)
{
    return GetFolder(targetFolder, "", aFolder);
}

PRInt32    
nsInstall::GetLastError(PRInt32* aReturn)
{
    *aReturn = mLastError;
    return NS_OK;
}

PRInt32    
nsInstall::GetWinProfile(const nsString& aFolder, const nsString& aFile, JSContext* jscontext, JSClass* WinProfileClass, jsval* aReturn)
{
#ifdef _WINDOWS
    JSObject*     winProfileObject;
    nsWinProfile* nativeWinProfileObject = new nsWinProfile(this, aFolder, aFile);
    JSObject*     winProfilePrototype    = this->RetrieveWinProfilePrototype();

    winProfileObject = JS_NewObject(jscontext, WinProfileClass, winProfilePrototype, NULL);
    if(winProfileObject == NULL)
    {
      return PR_FALSE;
    }

    JS_SetPrivate(jscontext, winProfileObject, nativeWinProfileObject);

    *aReturn = OBJECT_TO_JSVAL(winProfileObject);
#else
    *aReturn = JSVAL_NULL;
#endif /* _WINDOWS */

    return NS_OK;
}

PRInt32    
nsInstall::GetWinRegistry(JSContext* jscontext, JSClass* WinRegClass, jsval* aReturn)
{
#ifdef _WINDOWS
    JSObject* winRegObject;
    nsWinReg* nativeWinRegObject = new nsWinReg(this);
    JSObject* winRegPrototype    = this->RetrieveWinRegPrototype();

    winRegObject = JS_NewObject(jscontext, WinRegClass, winRegPrototype, NULL);
    if(winRegObject == NULL)
    {
      return PR_FALSE;
    }

    JS_SetPrivate(jscontext, winRegObject, nativeWinRegObject);

    *aReturn = OBJECT_TO_JSVAL(winRegObject);
#else
    *aReturn = JSVAL_NULL;
#endif /* _WINDOWS */

    return NS_OK;
}

PRInt32
nsInstall::LoadResources(JSContext* cx, const nsString& aBaseName, jsval* aReturn)
{
    nsresult ret;
    nsFileSpec* resFile;
    nsFileURL* resFileURL = nsnull;
    nsIURI *url = nsnull;
    nsILocale* locale = nsnull;
    nsIStringBundleService* service = nsnull;
#ifndef NECKO
    nsINetService* pNetService = nsnull;
#endif
    nsIEventQueueService* pEventQueueService = nsnull;
    nsIStringBundle* bundle = nsnull;
    nsIBidirectionalEnumerator* propEnum = nsnull;
    *aReturn = JSVAL_NULL;
    jsval v = JSVAL_NULL;

    // set up JSObject to return
    JS_GetProperty( cx, JS_GetGlobalObject( cx ), "Object", &v );
    if (!v)
    {
        SaveError(nsInstall::UNEXPECTED_ERROR);
        return NS_ERROR_FAILURE;
    }
    JSClass *objclass = JS_GetClass( cx, JSVAL_TO_OBJECT(v) );
    JSObject *res = JS_NewObject( cx, objclass, JSVAL_TO_OBJECT(v), 0 );

    // extract properties file
    // XXX append locale info: lang code, country code, .properties suffix to aBaseName
    ExtractFileFromJar(aBaseName, nsnull, &resFile);
    if (!resFile)
    {
        SaveError( nsInstall::FILE_DOES_NOT_EXIST );
        return NS_OK;
    }
	
    // initialize string bundle and related services
    ret = nsServiceManager::GetService(kStringBundleServiceCID, 
                    kIStringBundleServiceIID, (nsISupports**) &service);
    if (NS_FAILED(ret)) 
        goto handle_err;
#ifndef NECKO
    ret = nsServiceManager::GetService(kNetServiceCID, kINetServiceIID, (nsISupports**) &pNetService);
    if (NS_FAILED(ret)) 
        goto handle_err;
#endif
    ret = nsServiceManager::GetService(kEventQueueServiceCID,
                    kIEventQueueServiceIID, (nsISupports**) &pEventQueueService);
    if (NS_FAILED(ret)) 
        goto handle_err;
    ret = pEventQueueService->CreateThreadEventQueue();
    if (NS_FAILED(ret)) 
        goto handle_err;

    // construct properties file URL as required by StringBundle interface
    resFileURL = new nsFileURL( *resFile );
#ifdef NECKO
    ret = NS_NewURI(&url, resFileURL->GetURLString());
#else
    ret = pNetService->CreateURL(&url, nsString( resFileURL->GetURLString()), nsnull, nsnull, nsnull);
#endif
    if (resFileURL)
        delete resFileURL;
    if (resFile)
	    delete resFile;
    if (NS_FAILED(ret)) 
        goto handle_err;

    // get the string bundle using the extracted properties file
    ret = service->CreateBundle(url, locale, &bundle);
    if (NS_FAILED(ret)) 
        goto handle_err;
    ret = bundle->GetEnumeration(&propEnum);
    if (NS_FAILED(ret))
        goto handle_err;

    // set the variables of the JSObject to return using the StringBundle's
    // enumeration service
    ret = propEnum->First();
    if (NS_FAILED(ret))
        goto handle_err;
    while (NS_SUCCEEDED(ret))
    {
        nsIPropertyElement* propElem = nsnull;
        ret = propEnum->CurrentItem((nsISupports**)&propElem);
        if (NS_FAILED(ret))
            goto handle_err;
        nsString* key = nsnull;
        nsString* val = nsnull;
        ret = propElem->GetKey(&key);
        if (NS_FAILED(ret)) 
            goto handle_err;
        ret = propElem->GetValue(&val);
        if (NS_FAILED(ret))
            goto handle_err;
        char* keyCStr = key->ToNewCString();
        PRUnichar* valCStr = val->ToNewUnicode();
        if (keyCStr && valCStr) 
        {
            JSString* propValJSStr = JS_NewUCStringCopyZ(cx, (jschar*) valCStr);
            jsval propValJSVal = STRING_TO_JSVAL(propValJSStr);
            JS_SetProperty(cx, res, keyCStr, &propValJSVal);
            delete[] keyCStr;
            delete[] valCStr;
        }
        if (key)
            delete key;
        if (val)
            delete val;
        ret = propEnum->Next();
    }
	 
    *aReturn = OBJECT_TO_JSVAL(res);
    return NS_OK;

handle_err:
    SaveError(ret);
    return NS_OK;
}

PRInt32    
nsInstall::Patch(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, const nsString& aFolder, const nsString& aTargetName, PRInt32* aReturn)
{
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }

    nsString qualifiedRegName;
    
    *aReturn = GetQualifiedRegName( aRegName, qualifiedRegName);
    
    if (*aReturn != SUCCESS)
    {
        return NS_OK;
    }

    nsInstallPatch* ip = new nsInstallPatch( this,
                                             qualifiedRegName,
                                             aVersion,
                                             aJarSource,
                                             aFolder,
                                             aTargetName,
                                             &result);
    

    if (result == nsInstall::SUCCESS) 
    {
        result = ScheduleForInstall( ip );
    }
        
    *aReturn = SaveError(result);
    return NS_OK;
}

PRInt32    
nsInstall::Patch(const nsString& aRegName, const nsString& aJarSource, const nsString& aFolder, const nsString& aTargetName, PRInt32* aReturn)
{
    return Patch(aRegName, "", aJarSource, aFolder, aTargetName, aReturn);
}

PRInt32    
nsInstall::ResetError()
{
    mLastError = nsInstall::SUCCESS;
    return NS_OK;
}

PRInt32    
nsInstall::SetPackageFolder(const nsString& aFolder)
{
    mPackageFolder = aFolder;
    return NS_OK;
}


PRInt32    
nsInstall::StartInstall(const nsString& aUserPackageName, const nsString& aRegistryPackageName, const nsString& aVersion, PRInt32* aReturn)
{
    char szRegPackagePath[MAXREGPATHLEN];
    char* szRegPackageName = aRegistryPackageName.ToNewCString();

    *szRegPackagePath = '0';
    *aReturn   = nsInstall::SUCCESS;
    
    ResetError();
        
    mUserCancelled = PR_FALSE; 
    
    if ( aRegistryPackageName.Equals("") || aRegistryPackageName.EqualsIgnoreCase("null"))  
    {
        *aReturn = nsInstall::INVALID_ARGUMENTS;
        return NS_OK;
    }
    
    mUIName = aUserPackageName;
    
    *aReturn = GetQualifiedPackageName( aRegistryPackageName, mRegistryPackageName );
    
    if (*aReturn != nsInstall::SUCCESS)
    {
        return NS_OK;
    }

    if(REGERR_OK == VR_GetDefaultDirectory(szRegPackageName, MAXREGPATHLEN, szRegPackagePath))
    {
      mPackageFolder = szRegPackagePath;
    }
    else
    {
      mPackageFolder = "null";
    }

    if(szRegPackageName)
      delete [] szRegPackageName;

    if (mVersionInfo != nsnull)
        delete mVersionInfo;

    mVersionInfo    = new nsInstallVersion();
    mVersionInfo->Init(aVersion);

    mInstalledFiles = new nsVector();
    mPatchList      = new nsHashtable();

    /* this function should also check security!!! */
    *aReturn = OpenJARFile();

    if (*aReturn != nsInstall::SUCCESS)
    {
        /* if we can not continue with the javascript return a JAR error*/
        return -1;  /* FIX: need real error code */
    }
 
    SaveError(*aReturn);
    
    if (mNotifier)
            mNotifier->InstallStarted(nsAutoCString(mUIName));

    return NS_OK;
}


PRInt32    
nsInstall::Uninstall(const nsString& aRegistryPackageName, PRInt32* aReturn)
{
    PRInt32 result = SanityCheck();

    if (result != nsInstall::SUCCESS)
    {
        *aReturn = SaveError( result );
        return NS_OK;
    }
    
    nsString qualifiedPackageName;

    *aReturn = GetQualifiedPackageName( aRegistryPackageName, qualifiedPackageName );
    
    if (*aReturn != SUCCESS)
    {
        return NS_OK;
    }

    nsInstallUninstall *ie = new nsInstallUninstall( this,
                                                     qualifiedPackageName,
                                                     &result );

    if (result == nsInstall::SUCCESS) 
    {
        result = ScheduleForInstall( ie );
    }
    else
    {
        delete ie;
    }    

    *aReturn = SaveError(result);

    return NS_OK;
}

////////////////////////////////////////


void       
nsInstall::AddPatch(nsHashKey *aKey, nsFileSpec* fileName)
{
    if (mPatchList != nsnull)
    {
        mPatchList->Put(aKey, fileName);
    }
}

void       
nsInstall::GetPatch(nsHashKey *aKey, nsFileSpec** fileName)
{
    if (mPatchList != nsnull)
    {
        *fileName = (nsFileSpec*) mPatchList->Get(aKey);
    }
}

PRInt32
nsInstall::FileOpDirCreate(nsFileSpec& aTarget, PRInt32* aReturn)
{
  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_DIR_CREATE, aTarget, aReturn);

  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }
      
  if (*aReturn == nsInstall::FILE_DOES_NOT_EXIST) 
  {
      *aReturn = nsInstall::SUCCESS;
  }

  SaveError(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpDirGetParent(nsFileSpec& aTarget, nsFileSpec* aReturn)
{
//  nsInstallFileOpItem ifop(this, aTarget, aReturn);

  aTarget.GetParent(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpDirRemove(nsFileSpec& aTarget, PRInt32 aFlags, PRInt32* aReturn)
{
  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_DIR_REMOVE, aTarget, aFlags, aReturn);

  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }
      
  if (*aReturn == nsInstall::FILE_DOES_NOT_EXIST) 
  {
      *aReturn = nsInstall::SUCCESS;
  }

  SaveError(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpDirRename(nsFileSpec& aSrc, nsString& aTarget, PRInt32* aReturn)
{
  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_DIR_RENAME, aSrc, aTarget, aReturn);

  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }
      
  if (*aReturn == nsInstall::FILE_DOES_NOT_EXIST) 
  {
      *aReturn = nsInstall::SUCCESS;
  }

  SaveError(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpFileCopy(nsFileSpec& aSrc, nsFileSpec& aTarget, PRInt32* aReturn)
{
  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_FILE_COPY, aSrc, aTarget, aReturn);

  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }
      
  if (*aReturn == nsInstall::FILE_DOES_NOT_EXIST) 
  {
      *aReturn = nsInstall::SUCCESS;
  }

  SaveError(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpFileDelete(nsFileSpec& aTarget, PRInt32 aFlags, PRInt32* aReturn)
{
  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_FILE_DELETE, aTarget, aFlags, aReturn);

  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }
      
  if (*aReturn == nsInstall::FILE_DOES_NOT_EXIST) 
  {
      *aReturn = nsInstall::SUCCESS;
  }

  SaveError(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpFileExecute(nsFileSpec& aTarget, nsString& aParams, PRInt32* aReturn)
{
  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_FILE_EXECUTE, aTarget, aParams, aReturn);

  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }
      
  if (*aReturn == nsInstall::FILE_DOES_NOT_EXIST) 
  {
      *aReturn = nsInstall::SUCCESS;
  }

  SaveError(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpFileExists(nsFileSpec& aTarget, PRBool* aReturn)
{
  *aReturn = aTarget.Exists();
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileGetNativeVersion(nsFileSpec& aTarget, nsString* aReturn)
{
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileGetDiskSpaceAvailable(nsFileSpec& aTarget, PRUint32* aReturn)
{
  *aReturn = aTarget.GetDiskSpaceAvailable();
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileGetModDate(nsFileSpec& aTarget, nsFileSpec::TimeStamp* aReturn)
{
  aTarget.GetModDate(*aReturn);
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileGetSize(nsFileSpec& aTarget, PRUint32* aReturn)
{
  *aReturn = aTarget.GetFileSize();
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileIsDirectory(nsFileSpec& aTarget, PRBool* aReturn)
{
  *aReturn = aTarget.IsDirectory();
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileIsFile(nsFileSpec& aTarget, PRBool* aReturn)
{
  *aReturn = aTarget.IsFile();
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileModDateChanged(nsFileSpec& aTarget, nsFileSpec::TimeStamp& aOldStamp, PRBool* aReturn)
{
  *aReturn = aTarget.ModDateChanged(aOldStamp);
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileMove(nsFileSpec& aSrc, nsFileSpec& aTarget, PRInt32* aReturn)
{
  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_FILE_MOVE, aSrc, aTarget, aReturn);

  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }
      
  if (*aReturn == nsInstall::FILE_DOES_NOT_EXIST) 
  {
      *aReturn = nsInstall::SUCCESS;
  }

  SaveError(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpFileRename(nsFileSpec& aSrc, nsString& aTarget, PRInt32* aReturn)
{
  nsInstallFileOpItem* ifop = new nsInstallFileOpItem(this, NS_FOP_FILE_RENAME, aSrc, aTarget, aReturn);

  if (*aReturn == nsInstall::SUCCESS) 
  {
      *aReturn = ScheduleForInstall( ifop );
  }
      
  if (*aReturn == nsInstall::FILE_DOES_NOT_EXIST) 
  {
      *aReturn = nsInstall::SUCCESS;
  }

  SaveError(*aReturn);

  return NS_OK;
}

PRInt32
nsInstall::FileOpFileWinShortcutCreate(nsFileSpec& aTarget, PRInt32 aFlags, PRInt32* aReturn)
{
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileMacAliasCreate(nsFileSpec& aTarget, PRInt32 aFlags, PRInt32* aReturn)
{
  return NS_OK;
}

PRInt32
nsInstall::FileOpFileUnixLinkCreate(nsFileSpec& aTarget, PRInt32 aFlags, PRInt32* aReturn)
{
  return NS_OK;
}

void
nsInstall::LogComment(nsString& aComment)
{
  if(mNotifier)
    mNotifier->LogComment(nsAutoCString(aComment));
}

/////////////////////////////////////////////////////////////////////////
// Private Methods
/////////////////////////////////////////////////////////////////////////

/**
 * ScheduleForInstall
 * call this to put an InstallObject on the install queue
 * Do not call installedFiles.addElement directly, because this routine also 
 * handles progress messages
 */
PRInt32 
nsInstall::ScheduleForInstall(nsInstallObject* ob)
{
    PRInt32 error = nsInstall::SUCCESS;

    char *objString = ob->toString();

    // flash current item

    if (mNotifier)
        if ( !NS_SUCCEEDED(mNotifier->ItemScheduled(objString)) )
            mUserCancelled = PR_TRUE;

    delete [] objString;
    
    // do any unpacking or other set-up
    error = ob->Prepare();
    
    if (error != nsInstall::SUCCESS) 
        return error;
    
    
    // Add to installation list if we haven't thrown out
    
    mInstalledFiles->Add( ob );

    // turn on flags for creating the uninstall node and
    // the package node for each InstallObject
    
    if (ob->CanUninstall())
        mUninstallPackage = PR_TRUE;
	
    if (ob->RegisterPackageNode())
        mRegisterPackage = PR_TRUE;
  
  return nsInstall::SUCCESS;
}


/**
 * SanityCheck
 *
 * This routine checks if the packageName is null. It also checks the flag if the user cancels
 * the install progress dialog is set and acccordingly aborts the install.
 */
PRInt32
nsInstall::SanityCheck(void)
{
    if ( mRegistryPackageName == "" || mUIName == "") 
    {
        return INSTALL_NOT_STARTED;	
    }

    if (mUserCancelled) 
    {
        AbortInstall();
        SaveError(USER_CANCELLED);
        return USER_CANCELLED;
    }
	
	return 0;
}

/**
 * GetQualifiedPackageName
 *
 * This routine converts a package-relative component registry name
 * into a full name that can be used in calls to the version registry.
 */

PRInt32
nsInstall::GetQualifiedPackageName( const nsString& name, nsString& qualifiedName )
{
    nsString startOfName;
    name.Left(startOfName, 7);

    if ( startOfName.Equals( "=USER=/") )
    {
        CurrentUserNode(qualifiedName);
        qualifiedName += name;
    }
    else
    {
        qualifiedName = name;
    }
    
    if (BadRegName(qualifiedName)) 
    {
        return BAD_PACKAGE_NAME;
    }


    /* Check to see if the PackageName ends in a '/'.  If it does nuke it. */

    if (qualifiedName.Last() == '/')
    {
        PRInt32 index = qualifiedName.Length();
        qualifiedName.Truncate(--index);
    }

    return SUCCESS;
}


/**
 * GetQualifiedRegName
 *
 * This routine converts a package-relative component registry name
 * into a full name that can be used in calls to the version registry.
 */
PRInt32
nsInstall::GetQualifiedRegName(const nsString& name, nsString& qualifiedRegName )
{
    nsString startOfName;
    name.Left(startOfName, 7);

    nsString usr ();

    if ( startOfName.Equals("=COMM=/") || startOfName.Equals("=USER=/")) 
    {
        qualifiedRegName = name;
        qualifiedRegName.Cut( 0, 7 );
    }
    else if ( name.CharAt(0) != '/' )
    {
        if (mRegistryPackageName != "")
        {
            qualifiedRegName = mRegistryPackageName;
            qualifiedRegName += "/";
            qualifiedRegName += name;
        }
        else
        {
            qualifiedRegName = name;
        }
    }
    else
    {
       qualifiedRegName = name;
    }

    if (BadRegName(qualifiedRegName)) 
    {
        return BAD_PACKAGE_NAME;
    }
  
    return SUCCESS;
}


static NS_DEFINE_IID(kPrefsIID, NS_IPREF_IID);
static NS_DEFINE_IID(kPrefsCID,  NS_PREF_CID);

void
nsInstall::CurrentUserNode(nsString& userRegNode)
{    
    char *profname;
    int len = MAXREGNAMELEN;
    nsIPref * prefs;
    
    nsresult rv = nsServiceManager::GetService(kPrefsCID, 
                                               kPrefsIID,
                                               (nsISupports**) &prefs);


    if ( NS_SUCCEEDED(rv) )
    {
        rv = prefs->CopyCharPref("profile.name", &profname);

        if ( NS_FAILED(rv) )
        {
            PR_FREEIF(profname); // Allocated by PREF_CopyCharPref
            profname = NULL;
        }

        NS_RELEASE(prefs);
    }
    else
    {
        profname = NULL;
    }
    
    userRegNode = "/Netscape/Users/";
    if (profname != nsnull)
    {
        userRegNode += nsString(profname);
        userRegNode += "/";
        PR_FREEIF(profname);
    }
}

// catch obvious registry name errors proactively
// rather than returning some cryptic libreg error
PRBool 
nsInstall::BadRegName(const nsString& regName)
{
    if ((regName.First() == ' ' ) || (regName.Last() == ' ' ))
        return PR_TRUE;
        
    if ( regName.Find("//") != -1 )
        return PR_TRUE;
     
    if ( regName.Find(" /") != -1 )
        return PR_TRUE;

    if ( regName.Find("/ ") != -1  )
        return PR_TRUE;        
    
    if ( regName.Find("=") != -1 )
        return PR_TRUE;           

    return PR_FALSE;
}

PRInt32    
nsInstall::SaveError(PRInt32 errcode)
{
  if ( errcode != nsInstall::SUCCESS ) 
    mLastError = errcode;
  
  return errcode;
}

/*
 * CleanUp
 * call	it when	done with the install
 *
 */
void 
nsInstall::CleanUp(void)
{
    nsInstallObject* ie;
    CloseJARFile();
    
    if ( mInstalledFiles != NULL ) 
    {
        PRUint32 i=0;
        for (; i < mInstalledFiles->GetSize(); i++) 
        {
            ie = (nsInstallObject*)mInstalledFiles->Get(i);
            delete (ie);
        }

        mInstalledFiles->RemoveAll();
        delete (mInstalledFiles);
        mInstalledFiles = nsnull;
    }

    if (mPatchList)
    {
        // do I need to delete every entry?
        delete mPatchList;
    }
    
    mRegistryPackageName = ""; // used to see if StartInstall() has been called
}


void       
nsInstall::GetJarFileLocation(nsString& aFile)
{
    aFile = mJarFileLocation;
}

void       
nsInstall::SetJarFileLocation(const nsString& aFile)
{
    mJarFileLocation = aFile;
}

void       
nsInstall::GetInstallArguments(nsString& args)
{
    args = mInstallArguments;
}

void       
nsInstall::SetInstallArguments(const nsString& args)
{
    mInstallArguments = args;
}



PRInt32 
nsInstall::OpenJARFile(void)
{    
    PRInt32 result;

    nsresult rv = mJarFileData->Open( nsAutoCString(mJarFileLocation), &result );
    if (NS_FAILED(rv))
        return UNEXPECTED_ERROR;

    return result;
}

void
nsInstall::CloseJARFile(void)
{
    NS_IF_RELEASE(mJarFileData);
}


// aJarFile         - This is the filepath within the jar file.
// aSuggestedName   - This is the name that we should try to extract to.  If we can, we will create a new temporary file.
// aRealName        - This is the name that we did extract to.  This will be allocated by use and should be disposed by the caller.

PRInt32    
nsInstall::ExtractFileFromJar(const nsString& aJarfile, nsFileSpec* aSuggestedName, nsFileSpec** aRealName)
{
    PRInt32 result;
    nsFileSpec *extractHereSpec;

    NS_ASSERTION(aSuggestedName != nsnull, "Can't extract to no name!");
    if(aSuggestedName == nsnull)
    {
      return nsInstall::INVALID_ARGUMENTS;
    }
    else
    {
        // extract to the final destination.
        extractHereSpec = new nsFileSpec(*aSuggestedName);
        extractHereSpec->MakeUnique();
    }

    // We will overwrite what is in the way.  is this something that we want to do?  
    extractHereSpec->Delete(PR_FALSE);

    nsresult rv = mJarFileData->Extract( nsAutoCString(aJarfile), nsNSPRPath( *extractHereSpec ), &result );
    if (NS_FAILED(rv))
    {
        if (extractHereSpec != nsnull)
            delete extractHereSpec;
        return EXTRACTION_FAILED;
    }
    
    if (result == 0)
    {
        *aRealName = extractHereSpec;
    }
    else
    {
        if (extractHereSpec != nsnull)
            delete extractHereSpec;
    }
    return result;
}


PRInt32 
nsInstall::ExtractDirEntries(const nsString& directory, nsVector *paths)
{
    char                *buf;
    nsISimpleEnumerator *jarEnum = nsnull;
    nsIJARItem          *currJARItem = nsnull;

    if ( paths )
    {
        nsString pattern(directory);
        pattern += "/*";
        PRInt32 prefix_length = directory.Length()+1; // account for slash

        nsresult rv = mJarFileData->Find( nsAutoCString(pattern), &jarEnum );
        if (NS_FAILED(rv) || !jarEnum)
            goto handle_err;

        PRBool bMore;
        rv = jarEnum->HasMoreElements(&bMore);
        while (bMore && NS_SUCCEEDED(rv))
        {
            rv = jarEnum->GetNext( (nsISupports**) &currJARItem );
            if (currJARItem)
            {
                // expensive 'buf' callee malloc per iteration!
                rv = currJARItem->GetName(&buf);
                if (NS_FAILED(rv)) 
                    goto handle_err;
                if (buf)
                {
                    PRInt32 namelen = PL_strlen(buf);
                    NS_ASSERTION( prefix_length <= namelen, "Match must be longer than pattern!" );

                    if ( buf[namelen-1] != '/' ) 
                    {
                        paths->Add( new nsString(buf+prefix_length) ); // XXX manipulation should be in caller
                    }

                    PR_FREEIF( buf );
                }
                NS_IF_RELEASE(currJARItem);
            }
            rv = jarEnum->HasMoreElements(&bMore);
        }
    }

    NS_IF_RELEASE(jarEnum);
    return SUCCESS;

handle_err:    
    NS_IF_RELEASE(jarEnum);                         
    NS_IF_RELEASE(currJARItem); 
    return EXTRACTION_FAILED;
}

void
nsInstall::DeleteVector(nsVector* vector)
{
    if (vector != nsnull)
    {
        PRUint32 i=0;
        for (; i < vector->GetSize(); i++) 
        {
            nsString* element = (nsString*)vector->Get(i);
            if (element != nsnull)
                delete element;
        }

        vector->RemoveAll();
        delete (vector);
        vector = nsnull;
    }
}
