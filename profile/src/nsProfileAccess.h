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

#ifndef __nsProfileAccess_h___
#define __nsProfileAccess_h___

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsIRegistry.h"
#include "nsXPIDLString.h"

#define _MAX_NUM_PROFILES       100
#define _MAX_4X_PROFILES		50

//typedef struct _profile_struct ProfileStruct;
typedef struct _profile_struct {
	char*	profileName;
	char*	profileLocation;
	char*	isMigrated;
	char*   NCProfileName;
	char*	NCDeniedService;
	PRBool	updateProfileEntry;
}ProfileStruct;

class nsProfileAccess
{

private:
	nsCOMPtr <nsIRegistry> m_registry;

	ProfileStruct	*mProfiles[_MAX_NUM_PROFILES];
	PRInt32			mCount;
	char*			mCurrentProfile;
	char*			mVersion;
	PRBool			mFixRegEntries;
	char*			mHavePREGInfo;
	PRInt32			m4xCount;

	nsresult OpenRegistry();
	nsresult CloseRegistry();

public:
	PRBool			mProfileDataChanged;
	PRBool			mForgetProfileCalled;
	PRInt32			mNumProfiles;
	PRInt32			mNumOldProfiles;
	ProfileStruct	*m4xProfiles[_MAX_4X_PROFILES];

	nsProfileAccess();
	virtual ~nsProfileAccess();

	nsresult SetValue(ProfileStruct* aProfile);
	nsresult FillProfileInfo();

	void	 GetNumProfiles(PRInt32 *numProfiles);
	void	 GetNum4xProfiles(PRInt32 *numProfiles);
	void	 GetFirstProfile(char **firstProfile);

	void	 SetCurrentProfile(const char *profileName);
	void	 GetCurrentProfile(char **profileName);

	void	 RemoveSubTree(const char* profileName);
	void	 FixRegEntry(char** dirName);

	nsresult HavePregInfo(char **info);
	nsresult GetValue(const char* profileName, ProfileStruct** aProfile);
	PRInt32	 FindProfileIndex(const char* profileName);

	nsresult UpdateRegistry();
	void	 GetProfileList(char **profileListStr);
	PRBool	 ProfileExists(const char *profileName);
	nsresult Get4xProfileInfo(const char *registryName);
	nsresult UpdateProfileArray();
	void	 SetPREGInfo(const char* pregInfo);
	void	 GetPREGInfo(char** pregInfo);
	void	 FreeProfileMembers(ProfileStruct *aProfile[], PRInt32 numElems);
};

#endif // __nsProfileAccess_h___

