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

#ifndef nsIProfile_h__
#define nsIProfile_h__

#include "nsISupports.h"
#include "nsFileSpec.h"

/** new ids **/
#define NS_IPROFILE_IID                                \
  { /* {02b0625a-e7f3-11d2-9f5a-006008a6efe9} */       \
    0x02b0625a,                                        \
    0xe7f3,                                            \
    0x11d2,                                            \
    { 0x9f, 0x5a, 0x00, 0x60, 0x08, 0xa6, 0xef, 0xe9 } \
  }

#define NS_PROFILE_CID                                 \
  { /* {02b0625b-e7f3-11d2-9f5a-006008a6efe9} */       \
    0x02b0625b,                                        \
    0xe7f3,                                            \
    0x11d2,                                            \
    { 0x9f, 0x5a, 0x00, 0x60, 0x08, 0xa6, 0xef, 0xe9 } \
  }

#define NS_USING_PROFILES 1

#define PREG_PREF	"netcenter.register"

/*
 * Return values
 */

class nsIProfile: public nsISupports {
public:

  static const nsIID& GetIID(void) { static nsIID iid = NS_IPROFILE_IID; return iid; }

	// Initialize/shutdown
	NS_IMETHOD Startup(char *filename) = 0;
	NS_IMETHOD Shutdown() = 0;

	// Getters
	NS_IMETHOD GetProfileDir(const char *profileName, nsFileSpec* profileDir) = 0;
	NS_IMETHOD GetProfileCount(int *numProfiles) = 0;
	NS_IMETHOD GetSingleProfile(char **profileName) = 0;
	NS_IMETHOD GetCurrentProfile(char **profileName) = 0;
	NS_IMETHOD GetFirstProfile(char **profileName) = 0;
	NS_IMETHOD GetCurrentProfileDir(nsFileSpec* profileDir) = 0;

	// Setters
	NS_IMETHOD SetProfileDir(const char *profileName, const nsFileSpec& profileDir) = 0;

	// Migrate 4.x profile information
	NS_IMETHOD MigrateProfileInfo() = 0;

    // Creators
    NS_IMETHOD CreateNewProfile(char* data) = 0;
	NS_IMETHOD RenameProfile(const char* aOldName, const char* aNewName) = 0;  
	NS_IMETHOD DeleteProfile(const char* aProfileName) = 0; 
	NS_IMETHOD GetProfileList(nsString& aProfileList) = 0;
	NS_IMETHOD StartCommunicator(const char* aProfileName) = 0;
	NS_IMETHOD GetCurrProfile(nsString& currProfile) = 0;
	NS_IMETHOD MigrateProfile(const char* aProfileName) = 0;

	// Cookie processing
	NS_IMETHOD GetCookie(nsString& aCookie) = 0;
	NS_IMETHOD ProcessPRegCookie() = 0;
	NS_IMETHOD IsPregCookieSet(char **pregSet) = 0;
	NS_IMETHOD ProcessPREGInfo(char* data) = 0;

	NS_IMETHOD Get4xProfileCount(int *numProfiles) = 0;
};

#endif /* nsIProfile_h__ */
