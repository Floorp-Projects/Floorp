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

#include "nsIProfile.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsIRegistry.h"
#include "nsFileSpec.h"
#include "nsString.h"
#include "nsICmdLineService.h"


/* XXX TODO fix this */

#define _MAX_LENGTH             256
#define _MAX_NUM_PROFILES       50     

class nsProfile: public nsIProfile
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROFILE

private:
  nsCOMPtr <nsIRegistry> m_reg;

  nsresult ProcessArgs(nsICmdLineService *service,
                       PRBool *profileDirSet,
                       nsCString & profileURLStr);
  nsresult LoadDefaultProfileDir(nsCString & profileURLStr);
  nsresult OpenRegistry();
  nsresult CloseRegistry();

  char mNewProfileData[_MAX_NUM_PROFILES][_MAX_LENGTH];
  char mProfiles[_MAX_NUM_PROFILES][_MAX_LENGTH];
  PRInt32 mCount;
  PRInt32 mNumProfiles;
  PRInt32 mNumOldProfiles;
  PRBool mRenameCurrProfile;
  char mOldProfiles[_MAX_NUM_PROFILES][_MAX_LENGTH];
  char mOldProfLocations[_MAX_NUM_PROFILES][_MAX_LENGTH];
    
public:
    nsProfile();
    virtual ~nsProfile();

	// Creates associated user directories on the creation of a new profile
    void		CreateUserDirectories(const nsFileSpec& profileDir);

	// Deletes associated user directories
	void		DeleteUserDirectories(const nsFileSpec& profileDir);

	// Sets profile information recived from the Javascript (routed via core service) into an array
    void		SetDataArray(nsString data);

	// Gets a particular value from the DataArray
    char*		GetValue(char *name);

	// Copies all the registry keys from old profile to new profile
	nsresult	CopyRegKey(const char *oldProfile, const char *newProfile);

	// Fills the global array mProfiles by enumerating registry entries
	void		GetAllProfiles();

    NS_IMETHOD ProfileExists(const char *profileName);
	NS_IMETHOD UpdateMozProfileRegistry();
};

