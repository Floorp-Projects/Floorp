

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

#include "nsProfileCore.h"

#include "nsIProfile.h"
#include "nsIURL.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsAppCores.h"
#include "nsAppCoresCIDs.h"
#include "nsAppShellCIDs.h"
#include "nsAppCoresManager.h"
#include "nsIAppShellService.h"
#include "nsIServiceManager.h"

#include "nsIScriptGlobalObject.h"

#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIWebShellWindow.h"
#include "nsIDOMHTMLInputElement.h"

#include "plstr.h"
#include "prmem.h"

#include <ctype.h>

class nsIDOMnsStringArray;
// Globals - how many K are we wasting by putting these in every file?
static NS_DEFINE_IID(kISupportsIID,             NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIProfileCoreIID,            NS_IDOMPROFILECORE_IID);

static NS_DEFINE_IID(kIDOMDocumentIID,          nsIDOMDocument::GetIID());
static NS_DEFINE_IID(kIDocumentIID,             nsIDocument::GetIID());
static NS_DEFINE_IID(kIAppShellServiceIID,      NS_IAPPSHELL_SERVICE_IID);

static NS_DEFINE_IID(kProfileCoreCID,             NS_PROFILECORE_CID);
static NS_DEFINE_IID(kBrowserWindowCID,         NS_BROWSER_WINDOW_CID);
static NS_DEFINE_IID(kAppShellServiceCID,       NS_APPSHELL_SERVICE_CID);

static NS_DEFINE_IID(kIProfileIID, NS_IPROFILE_IID);
static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);

static NS_DEFINE_IID(kIFileLocatorIID, NS_IFILELOCATOR_IID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);

//----------------------------------------------------------------------------------------
nsProfileCore::nsProfileCore()
//----------------------------------------------------------------------------------------
:	mProfile(nsnull)
{
    printf("Created nsProfileCore\n");
	mScriptObject   = nsnull;

	IncInstanceCount();
	NS_INIT_REFCNT();
}


//----------------------------------------------------------------------------------------
nsProfileCore::~nsProfileCore()
//----------------------------------------------------------------------------------------
{
  //NS_IF_RELEASE(mWindow);
  DecInstanceCount();
}

//NS_IMPL_ADDREF(nsProfileCore)
//NS_IMPL_RELEASE(nsProfileCore)

// Is this needed ?
NS_IMPL_ISUPPORTS_INHERITED(nsProfileCore, nsBaseAppCore, nsIDOMProfileCore)

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsProfileCore::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
//----------------------------------------------------------------------------------------
{
    NS_PRECONDITION(nsnull != aScriptObject, "null arg");
    nsresult res = NS_OK;
    if (nsnull == mScriptObject) 
    {
            res = NS_NewScriptProfileCore(aContext, 
                                (nsISupports *)(nsIDOMProfileCore*)this, 
                                nsnull, 
                                &mScriptObject);
    }
    *aScriptObject = mScriptObject;

    return res;
}

//----------------------------------------------------------------------------------------
nsresult nsProfileCore::InitializeProfileManager()
//----------------------------------------------------------------------------------------
{ 
    nsIProfile* Profile;
    nsresult rv = nsServiceManager::GetService(kProfileCID, kIProfileIID, (nsISupports**)&Profile);
    if (NS_FAILED(rv))
        return rv;

    if (!Profile)
        return NS_ERROR_FAILURE;

    if (Profile)
        nsServiceManager::ReleaseService(kProfileCID, Profile);
   
    if (NS_FAILED(rv))
        return rv;

    mProfile = Profile;
    return NS_OK;
} // nsProfileCore::InitializeProfileManager

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsProfileCore::Init(const nsString& aId)
//----------------------------------------------------------------------------------------
{ 
    nsresult rv = nsBaseAppCore::Init(aId);
    if (NS_FAILED(rv))
        return rv;

    rv = InitializeProfileManager();
    if (NS_FAILED(rv))
        return rv;

    return NS_OK;
} // nsProfileCore::Init

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsProfileCore::CreateNewProfile(const nsString& aData)
// Start loading of a new Profile panel.
//----------------------------------------------------------------------------------------
{
	printf ("Entered Create Profile Routine");

	mProfile->CreateNewProfile(aData.ToNewCString());
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsProfileCore::RenameProfile(const nsString& oldName, const nsString& newName)
// Start loading of a new Profile panel.
//----------------------------------------------------------------------------------------
{
	printf ("Entered Rename Profile Routine\n");

	printf ("Change profileName from -> %s <- to -> %s <-\n", oldName.ToNewCString(), newName.ToNewCString());

	mProfile->RenameProfile(oldName.ToNewCString(), newName.ToNewCString());
    return NS_OK;
}


//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsProfileCore::DeleteProfile(const nsString& profileName, const nsString& deleteFlag)
// Start loading of a new Profile panel.
//----------------------------------------------------------------------------------------
{
	printf ("Entered Delete Profile Routine\n");

	printf ("Requesting deletion of profile -> %s <-\n", profileName.ToNewCString());

	mProfile->DeleteProfile(profileName.ToNewCString(), deleteFlag.ToNewCString());
    return NS_OK;
}


//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsProfileCore::GetProfileList(nsString& profileList)
// Start loading of a new Profile panel.
//----------------------------------------------------------------------------------------
{
	printf ("Entered GetProfileList Routine\n");

	mProfile->GetProfileList(profileList);
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsProfileCore::StartCommunicator(const nsString& profileName)
// Start loading of a new Profile panel.
//----------------------------------------------------------------------------------------
{
	printf ("Entered Start AppRunner Routine\n");

	printf ("Run AppRunner with profile -> %s <-\n", profileName.ToNewCString());

	mProfile->StartCommunicator(profileName.ToNewCString());
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsProfileCore::GetCurrentProfile(nsString& currProfile)
// Start loading of a new Profile panel.
//----------------------------------------------------------------------------------------
{
	printf ("Entered GetCurrentProfile Routine\n");

	mProfile->GetCurrProfile(currProfile);
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsProfileCore::MigrateProfile(const nsString& profileName)
// Start loading of a new Profile panel.
//----------------------------------------------------------------------------------------
{
#ifdef XP_PC
	printf ("Entered Migrate Profile Routine\n");

	printf ("Requesting migration of profile -> %s <-\n", profileName.ToNewCString());

	mProfile->MigrateProfile(profileName.ToNewCString());
#endif
    return NS_OK;
}
