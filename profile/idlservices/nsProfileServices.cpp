/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


/*

  A sample of XPConnect. This file contains an implementation of
  nsIProfileServices.

*/
#include "nscore.h"
#include "nsIProfileServices.h"
#include "nsIAllocator.h"
#include "plstr.h"
#include "stdio.h"
#include "nsIProfile.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"


static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);

class ProfileServicesImpl : public nsIProfileServices
{
public:
    ProfileServicesImpl();
    virtual ~ProfileServicesImpl();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIProfile interface
	NS_IMETHOD CreateNewProfile(const char* aValue);
	NS_IMETHOD RenameProfile(const char *oldName, const char *newName);
	NS_IMETHOD DeleteProfile(const char *profileName);
	NS_IMETHOD GetProfileList(char **_retval);
	NS_IMETHOD StartCommunicator(const char *profileName);
	NS_IMETHOD GetCurrentProfile(char **_retval);
	NS_IMETHOD MigrateProfile(const char *profileName);
	NS_IMETHOD ProcessPREGInfo(const char *data);

private:
	nsIProfile *mProfile;
};

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewProfileServices(nsIProfileServices** aProfile)
{
    NS_PRECONDITION(aProfile != nsnull, "null ptr");
    if (! aProfile)
        return NS_ERROR_NULL_POINTER;

    *aProfile = new ProfileServicesImpl();
    if (! *aProfile)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aProfile);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////


ProfileServicesImpl::ProfileServicesImpl()
:	mProfile(nsnull)
{
    NS_INIT_REFCNT();

	nsresult rv = nsServiceManager::GetService(kProfileCID, 
                                    nsIProfile::GetIID(), 
                                    (nsISupports **)&mProfile);
}


ProfileServicesImpl::~ProfileServicesImpl()
{
}



NS_IMPL_ISUPPORTS(ProfileServicesImpl, nsIProfileServices::GetIID());


NS_IMETHODIMP
ProfileServicesImpl::CreateNewProfile(const char* data)
{
    NS_PRECONDITION(data != nsnull, "null ptr");
    if (! data)
        return NS_ERROR_NULL_POINTER;

	mProfile->CreateNewProfile((char *)data);
    return NS_OK;
}

NS_IMETHODIMP
ProfileServicesImpl::RenameProfile(const char* oldName, const char* newName)
{

	mProfile->RenameProfile(oldName, newName);
    return NS_OK;
}

NS_IMETHODIMP
ProfileServicesImpl::DeleteProfile(const char* profileName)
{
    NS_PRECONDITION(profileName != nsnull, "null ptr");
    if (! profileName)
        return NS_ERROR_NULL_POINTER;

	mProfile->DeleteProfile(profileName);
    return NS_OK;
}

NS_IMETHODIMP
ProfileServicesImpl::GetProfileList(char** profileList)
{
	nsString localVar;
	mProfile->GetProfileList(localVar);
	*profileList = PL_strdup(localVar.ToNewCString());
    return NS_OK;
}

NS_IMETHODIMP
ProfileServicesImpl::StartCommunicator(const char* profileName)
{
    NS_PRECONDITION(profileName != nsnull, "null ptr");
    if (! profileName)
        return NS_ERROR_NULL_POINTER;

	mProfile->StartCommunicator(profileName);
    return NS_OK;
}

NS_IMETHODIMP
ProfileServicesImpl::GetCurrentProfile(char** profileName)
{
	mProfile->GetCurrentProfile(profileName);
    return NS_OK;
}

NS_IMETHODIMP
ProfileServicesImpl::MigrateProfile(const char* profileName)
{
    NS_PRECONDITION(profileName != nsnull, "null ptr");
    if (! profileName)
        return NS_ERROR_NULL_POINTER;

	mProfile->MigrateProfile(profileName);
    return NS_OK;
}

NS_IMETHODIMP
ProfileServicesImpl::ProcessPREGInfo(const char* data)
{
    NS_PRECONDITION(data != nsnull, "null ptr");
    if (! data)
        return NS_ERROR_NULL_POINTER;

 	mProfile->ProcessPREGInfo((char *)data);
    return NS_OK;
}
