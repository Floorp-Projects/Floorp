/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsIProfile.h"
#include "nsIProfileStartupListener.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsIRegistry.h"
#include "nsFileSpec.h"
#include "nsString.h"
#include "nsICmdLineService.h"
#include "nsProfileAccess.h"

// Interfaces Needed
#include "nsIXULWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIURIContentListener.h"
#include "nsIDirectoryService.h"

#define _MAX_LENGTH   256

#define REGISTRY_YES_STRING  "yes"
#define REGISTRY_NO_STRING   "no"

// strings for items in the registry we'll be getting or setting
#define REGISTRY_PROFILE_SUBTREE_STRING     "Profiles"
#define REGISTRY_CURRENT_PROFILE_STRING     "CurrentProfile"
#define REGISTRY_NC_SERVICE_DENIAL_STRING   "NCServiceDenial"
#define REGISTRY_NC_PROFILE_NAME_STRING     "NCProfileName"
#define REGISTRY_NC_USER_EMAIL_STRING       "NCEmailAddress"
#define REGISTRY_NC_HAVE_PREG_INFO_STRING   "NCHavePregInfo"
#define REGISTRY_HAVE_PREG_INFO_STRING      "HavePregInfo"
#define REGISTRY_MIGRATED_STRING            "migrated"
#define REGISTRY_DIRECTORY_STRING           "directory"
#define REGISTRY_NEED_MIGRATION_STRING      "NeedMigration"
#define REGISTRY_MOZREG_DATA_MOVED_STRING   "OldRegDataMoved"

#define REGISTRY_VERSION_STRING	  "Version"
#define REGISTRY_VERSION_1_0      "1.0"		

class nsProfile: public nsIProfile,
                 public nsIDirectoryServiceProvider 
{
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROFILE
    NS_DECL_NSIDIRECTORYSERVICEPROVIDER

private:
    nsresult ProcessArgs(nsICmdLineService *service,
                            PRBool *profileDirSet,
                            nsCString & profileURLStr);
    nsresult LoadDefaultProfileDir(nsCString & profileURLStr);
	nsresult CopyDefaultFile(nsIFile *profDefaultsDir,
	                         nsIFile *newProfDir,
								const char *fileName);
	nsresult EnsureProfileFileExists(nsIFile *aFile);
								
	nsresult CloneProfileDirectorySpec(nsILocalFile **aLocalFile);
	
    PRBool mAutomigrate;
    PRBool mOutofDiskSpace;
    PRBool mDiskSpaceErrorQuitCalled;

public:
    nsProfile();
    virtual ~nsProfile();

    nsresult RenameProfileDir(const PRUnichar *newProfileName);

    // Creates associated user directories on the creation of a new profile
    nsresult CreateUserDirectories(nsILocalFile *profileDir);

    // Deletes associated user directories
    nsresult DeleteUserDirectories(nsILocalFile *profileDir);

    // Copies all the registry keys from old profile to new profile
    nsresult CopyRegKey(const PRUnichar *oldProfile, const PRUnichar *newProfile);

    nsresult AutoMigrate();

    nsresult CreateDefaultProfile(void);
    nsresult ShowProfileWizard(void);
};

