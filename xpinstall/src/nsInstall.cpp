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
#include "nsIDOMInstall.h"
#include "nsIDOMInstallFolder.h"
#include "nsIDOMInstallVersion.h"

#include "nsInstallFile.h"

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"

#include "nsVector.h"

#include "prmem.h"
#include "pratom.h"
#include "prefapi.h"
#include "VerReg.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

static NS_DEFINE_IID(kIInstall_IID, NS_IDOMINSTALL_IID);



nsInstall::nsInstall()
{
    mScriptObject   = nsnull;
    mVersionInfo    = nsnull;
    
    mPackageName    = "";
    mUserPackageName= "";

    mUninstallPackage = PR_FALSE;
    mRegisterPackage  = PR_FALSE;

    NS_INIT_REFCNT();
}

nsInstall::~nsInstall()
{
    if (mVersionInfo != nsnull)
        delete mVersionInfo;
}

NS_IMETHODIMP 
nsInstall::QueryInterface(REFNSIID aIID,void** aInstancePtr)
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
    else if ( aIID.Equals(kIInstall_IID) )
    {
        *aInstancePtr = (void*) ((nsIDOMInstall*)this);
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

NS_IMPL_ADDREF(nsInstall)
NS_IMPL_RELEASE(nsInstall)



NS_IMETHODIMP 
nsInstall::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
    NS_PRECONDITION(nsnull != aScriptObject, "null arg");
    nsresult res = NS_OK;
    
    if (nsnull == mScriptObject) 
    {
        res = NS_NewScriptInstall(  aContext, 
                                    (nsISupports *)(nsIDOMInstall*)this, 
                                    nsnull, 
                                    &mScriptObject);
    }
  

    *aScriptObject = mScriptObject;
    return res;
}

NS_IMETHODIMP 
nsInstall::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

NS_IMETHODIMP    
nsInstall::GetUserPackageName(nsString& aUserPackageName)
{
    aUserPackageName = mUserPackageName;
    return NS_OK;
}

NS_IMETHODIMP    
nsInstall::GetRegPackageName(nsString& aRegPackageName)
{
    aRegPackageName = mPackageName;
    return NS_OK;
}

NS_IMETHODIMP    
nsInstall::AbortInstall()
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstall::AddDirectory(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsIDOMInstallFolder* aFolder, const nsString& aSubdir, PRBool aForceMode, PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstall::AddSubcomponent(const nsString& aRegName, 
                           nsIDOMInstallVersion* aVersion, 
                           const nsString& aJarSource, 
                           nsIDOMInstallFolder* aFolder, 
                           const nsString& aTargetName, 
                           PRBool aForceMode, 
                           PRInt32* aReturn)
{
    nsInstallFile*  ie;
    nsString*       qualifiedRegName;
    
    PRInt32         errcode = SU_SUCCESS;
    
    if ( aJarSource == "null" || aFolder == nsnull) 
    {
        return nsIDOMInstall::SUERR_INVALID_ARGUMENTS;
    }
    
    PRInt32 result = SanityCheck();

    if (result != nsIDOMInstall::SU_SUCCESS)
    {
        SaveError( result );
        return result;
    }


    if ( aRegName == "" ) 
    {
        // Default subName = location in jar file
        qualifiedRegName = GetQualifiedRegName( aJarSource );
    } 
    else 
    {
        qualifiedRegName = GetQualifiedRegName( aRegName );
    }

    if (qualifiedRegName == nsnull)
    {
        return SUERR_BAD_PACKAGE_NAME;  // this will stop the install script
    }
    
    /* Check for existence of the newer	version	*/
      
    PRBool versionNewer = PR_FALSE;
    char* qualifiedRegNameString = qualifiedRegName->ToNewCString();

    if ( (aForceMode == PR_FALSE ) && (aVersion !=  nsnull) && ( VR_ValidateComponent( qualifiedRegNameString ) == 0 ) ) 
    {
        VERSION versionStruct;
        
        VR_GetVersion( qualifiedRegNameString, &versionStruct );
    
        nsInstallVersion* oldVersion = new nsInstallVersion();
// FIX.  Once we move to XPConnect, we can have parameterized constructors.
        oldVersion->Init(versionStruct.major,
                         versionStruct.minor,
                         versionStruct.release,
                         versionStruct.build);

        PRInt32 areTheyEqual;
        aVersion->CompareTo((nsInstallVersion*)oldVersion, &areTheyEqual);
        
        if ( areTheyEqual != nsIDOMInstallVersion::SU_EQUAL )
            versionNewer = PR_TRUE;
      
	    if( oldVersion )
		    delete oldVersion;
    }
    else 
    {
        versionNewer = PR_TRUE;
    }
    
    
    if (qualifiedRegNameString != nsnull)
        delete qualifiedRegNameString;

    if (versionNewer) 
    {
        char* errorMsg = NULL;
    
        ie = new nsInstallFile( this, 
                                *qualifiedRegName, 
                                aVersion, 
                                aJarSource,
                                aFolder,
                                aTargetName, 
                                aForceMode, 
                                &errorMsg );

        if (errorMsg == NULL) 
        {
            errorMsg = ScheduleForInstall( ie );
        }
    
        if (errorMsg != NULL) 
        {
            SaveError( SUERR_UNEXPECTED_ERROR);
            return SUERR_UNEXPECTED_ERROR;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP    
nsInstall::DeleteComponent(const nsString& aRegistryName, PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstall::DeleteFile(nsIDOMInstallFolder* aFolder, const nsString& aRelativeFileName, PRInt32* aReturn)
{
    return NS_OK;    
}

NS_IMETHODIMP    
nsInstall::DiskSpaceAvailable(nsIDOMInstallFolder* aFolder, PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstall::Execute(const nsString& aJarSource, const nsString& aArgs, PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstall::FinalizeInstall(PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstall::Gestalt(const nsString& aSelector, PRInt32* aReturn)
{
    return NS_OK;    
}

NS_IMETHODIMP    
nsInstall::GetComponentFolder(const nsString& aRegName, const nsString& aSubdirectory, nsIDOMInstallFolder** aFolder)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstall::GetFolder(nsIDOMInstallFolder* targetFolder, const nsString& aSubdirectory, nsIDOMInstallFolder** aFolder)
{
    return NS_OK;    
}

NS_IMETHODIMP    
nsInstall::GetLastError(PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstall::GetWinProfile(nsIDOMInstallFolder* aFolder, const nsString& aFile, PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstall::GetWinRegistry(PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstall::Patch(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsIDOMInstallFolder* aFolder, const nsString& aTargetName, PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstall::ResetError()
{
    mLastError = SU_SUCCESS;
    return NS_OK;
}


NS_IMETHODIMP    
nsInstall::SetPackageFolder(nsIDOMInstallFolder* aFolder)
{
    return NS_OK;
}

/**
 * Call this to initialize the update
 * Opens the jar file and gets the certificate of the installer
 * Opens up the gui, and asks for proper security privileges
 *
 * @param aUserPackageName   
 *
 * @param aPackageName      Full qualified  version registry name of the package
 *                          (ex: "/Plugins/Adobe/Acrobat")
 *                          NULL or empty package names are errors
 *
 * @param inVInfo           version of the package installed.
 *                          Can be NULL, in which case package is installed
 *                          without a version. Having a NULL version, this 
 *                          package is automatically updated in the future 
 *                          (ie. no version check is performed).
 *
 * @param flags             Once was securityLevel(LIMITED_INSTALL or FULL_INSTALL).  Now
 *                          can be either NO_STATUS_DLG or NO_FINALIZE_DLG
 */
NS_IMETHODIMP    
nsInstall::StartInstall(const nsString& aUserPackageName, const nsString& aPackageName, const nsString& aVersion, PRInt32 aFlags, PRInt32* aReturn)
{
    *aReturn     = SU_SUCCESS;
    
    ResetError();
    ParseFlags(aFlags);
    
    mUserCancelled = PR_FALSE; 
    
    mUserPackageName = aUserPackageName;

    if ( aPackageName.Equals("") ) 
    {
        *aReturn = SUERR_INVALID_ARGUMENTS;
        return NS_OK;
    }
    
    nsString *tempString = GetQualifiedPackageName( aPackageName );
    
    mPackageName.SetLength(0);
    mPackageName.Append( *tempString );  
    
    delete tempString;

    /* Check to see if the PackageName ends in a '/'.  If it does nuke it. */

    if (mPackageName.Last() == '/')
    {
        PRInt32 index = mPackageName.Length();
        mPackageName.Truncate(--index);
    }
    
    if (mVersionInfo != nsnull)
        delete mVersionInfo;

    mVersionInfo    = new nsInstallVersion();
    mVersionInfo->Init(aVersion); /* this will go away when we have params in constructs in XPCOM */

    mInstalledFiles = new nsVector();

    /* this function should also check security!!! */
    *aReturn = OpenJARFile();

    if (*aReturn != SU_SUCCESS)
    {
        /* if we can not continue with the javascript return a JAR error*/
        return -1;  /* FIX: need real error code */
    }
 
    if (mShowProgress)
    {
        /* Show our window here */
    }   

     // set up default package folder, if any
    int   err;
    char* path = (char*) PR_Malloc(MAXREGPATHLEN);
    char* packageNameCString = mPackageName.ToNewCString();

    err = VR_GetDefaultDirectory( packageNameCString , MAXREGPATHLEN, path );
    
    delete [] packageNameCString;

    if (err != REGERR_OK)
    {
        PR_FREEIF(path);
        path = NULL;
    }
    
    if ( path !=  NULL ) 
    {
        mPackageFolder = new nsInstallFolder();
        mPackageFolder->Init("Installed",  nsString(path), mPackageName);

        PR_FREEIF(path); 
    }

    SaveError(*aReturn);
    
    if (*aReturn != SU_SUCCESS)
    {
        mPackageName = ""; // Reset!
    }

    return NS_OK;
}

NS_IMETHODIMP    
nsInstall::Uninstall(const nsString& aPackageName, PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstall::ExtractFileFromJar(const nsString& aJarfile, const nsString& aFinalFile, nsString& aTempFile, nsString& aErrorMsg)
{
    return NS_OK;
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
char* 
nsInstall::ScheduleForInstall(nsInstallObject* ob)
{
    char *errorMsg = NULL;
    char *objString = ob->toString();

    // flash current item
    //SetProgressDialogItem( objString );

    PR_FREEIF(objString);
    
    // do any unpacking or other set-up
    errorMsg = ob->Prepare();
    if (errorMsg != NULL) 
    {
        return errorMsg;
    }
    
    // Add to installation list if we haven't thrown out
    
    mInstalledFiles->Add( ob );

    // turn on flags for creating the uninstall node and
    // the package node for each InstallObject
    
    if (ob->CanUninstall())
        mUninstallPackage = PR_TRUE;
	
    if (ob->RegisterPackageNode())
        mRegisterPackage = PR_TRUE;
  
  return NULL;
}



void
nsInstall::ParseFlags(int flags)
{
    mShowProgress = mShowFinalize = PR_TRUE;
    if ((flags & SU_NO_STATUS_DLG) == SU_NO_STATUS_DLG)
    {
        mShowProgress = PR_FALSE;
    }
    if ((flags & SU_NO_FINALIZE_DLG) == SU_NO_FINALIZE_DLG)
    {
        mShowFinalize = PR_FALSE;
    }
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
    if ( mPackageName == "" || mUserPackageName == "") 
    {
        return SUERR_INSTALL_NOT_STARTED;	
    }

    if (mUserCancelled) 
    {
        AbortInstall();
        SaveError(SUERR_USER_CANCELLED);
        return SUERR_USER_CANCELLED;
    }
	
	return 0;
}

/**
 * GetQualifiedPackageName
 *
 * This routine converts a package-relative component registry name
 * into a full name that can be used in calls to the version registry.
 */

nsString *
nsInstall::GetQualifiedPackageName( const nsString& name )
{
    nsString* qualifedName = nsnull;
    
    /* this functions is really messed up.  The original checkin is messed as well */

    if ( name.Equals( "=USER=/") )
    {
        qualifedName = CurrentUserNode();
        qualifedName->Insert( name, 7 );
    }
    
    if (BadRegName(qualifedName)) 
    {
        if (qualifedName != nsnull)
        {
            delete qualifedName;
            qualifedName = nsnull;
        }
    }
    return qualifedName;
}


/**
 * GetQualifiedRegName
 *
 * Allocates a new string and returns it. Caller is supposed to free it
 *
 * This routine converts a package-relative component registry name
 * into a full name that can be used in calls to the version registry.
 */
nsString *
nsInstall::GetQualifiedRegName(const nsString& name )
{
    nsString *qualifiedRegName;

    nsString comm("=COMM=/");
    nsString usr ("=USER=/");

    if ( name.Compare(comm, PR_TRUE, comm.Length()) == 0 ) 
    {
        qualifiedRegName = new nsString( name );
        qualifiedRegName->Cut( 0, comm.Length() );
    }
    else if ( name.Compare(usr, PR_TRUE, usr.Length()) == 0 ) 
    {
        qualifiedRegName = new nsString( name );
        qualifiedRegName->Cut( 0, usr.Length() );
    }
    else if ( name[0] != '/' )
    {
        if (mUserPackageName != "")
        {
            qualifiedRegName = new nsString(mUserPackageName);
            qualifiedRegName += '/';
            qualifiedRegName->Append(name);
        }
        else
        {
            qualifiedRegName = new nsString(name);
        }
    }
    else
    {
        qualifiedRegName = new nsString(name);
    }

    if (BadRegName(qualifiedRegName)) 
    {
        delete qualifiedRegName;
        qualifiedRegName = NULL;
    }
  
    return qualifiedRegName;
}



nsString*
nsInstall::CurrentUserNode()
{
    nsString *qualifedName;
    nsString *profileName;
    
    char *profname;
    int len = MAXREGNAMELEN;
    int err;

    profname = (char*) PR_Malloc(len);

    err = PREF_GetCharPref( "profile.name", profname, &len );

    if ( err != PREF_OK )
    {
        PR_FREEIF(profname);
        profname = NULL;
    }
    
    profileName  = new nsString(profname);
    qualifedName = new nsString("/Netscape/Users/");
    
    qualifedName->Append(*profileName);
    qualifedName->Append("/");

    if (profileName != nsnull)
        delete profileName;

    return qualifedName;
}

// catch obvious registry name errors proactively
// rather than returning some cryptic libreg error
PRBool 
nsInstall::BadRegName(nsString* regName)
{
    if (regName == nsnull)
        return PR_TRUE;
    
    if ((regName->First() == ' ' ) || (regName->Last() == ' ' ))
        return PR_TRUE;
        
    if ( regName->Find("//") != -1 )
        return PR_TRUE;
     
    if ( regName->Find(" /") != -1 )
        return PR_TRUE;

    if ( regName->Find("/ ") != -1  )
        return PR_TRUE;        
    
    if ( regName->Find("=") != -1 )
        return PR_TRUE;           

    return PR_FALSE;
}

PRInt32    
nsInstall::SaveError(PRInt32 errcode)
{
  if ( errcode != SU_SUCCESS ) 
    mLastError = errcode;
  
  return errcode;
}


PRInt32 
nsInstall::OpenJARFile(void)
{
    return SU_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////
static PRInt32 gInstallInstanceCnt = 0;
static PRInt32 gInstallLock        = 0;

nsInstallFactory::nsInstallFactory(void)
{
    mRefCnt=0;
    PR_AtomicIncrement(&gInstallInstanceCnt);
}

nsInstallFactory::~nsInstallFactory(void)
{
    PR_AtomicDecrement(&gInstallInstanceCnt);
}

NS_IMETHODIMP 
nsInstallFactory::QueryInterface(REFNSIID aIID,void** aInstancePtr)
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
nsInstallFactory::AddRef(void)
{
    return ++mRefCnt;
}


NS_IMETHODIMP
nsInstallFactory::Release(void)
{
    if (--mRefCnt ==0)
    {
        delete this;
        return 0; // Don't access mRefCnt after deleting!
    }

    return mRefCnt;
}

NS_IMETHODIMP
nsInstallFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aResult == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aResult = NULL;

    /* do I have to use iSupports? */
    nsInstall *inst = new nsInstall();

    if (inst == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult result =  inst->QueryInterface(aIID, aResult);

    if (result != NS_OK)
        delete inst;

    return result;
}

NS_IMETHODIMP
nsInstallFactory::LockFactory(PRBool aLock)
{
    if (aLock)
        PR_AtomicIncrement(&gInstallLock);
    else
        PR_AtomicDecrement(&gInstallLock);

    return NS_OK;
}