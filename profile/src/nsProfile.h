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
#include "nsIProfileInternal.h"
#include "nsIProfileStartupListener.h"
#include "nsIProfileChangeStatus.h"
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

class nsProfile: public nsIProfileInternal,
                 public nsIDirectoryServiceProvider,
                 public nsIProfileChangeStatus 
{
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROFILE
    NS_DECL_NSIPROFILEINTERNAL
    NS_DECL_NSIDIRECTORYSERVICEPROVIDER
    NS_DECL_NSIPROFILECHANGESTATUS

private:
    nsresult ProcessArgs(nsICmdLineService *service,
                            PRBool *profileDirSet,
                            nsCString & profileURLStr);
    nsresult LoadDefaultProfileDir(nsCString & profileURLStr);
	nsresult CopyDefaultFile(nsIFile *profDefaultsDir,
	                         nsIFile *newProfDir,
								const char *fileName);
	nsresult EnsureProfileFileExists(nsIFile *aFile);
	nsresult LoadNewProfilePrefs();
    nsresult SetProfileDir(const PRUnichar *profileName, nsIFile *profileDir);
								
	nsresult CloneProfileDirectorySpec(nsILocalFile **aLocalFile);
    nsresult AddLevelOfIndirection(nsIFile *aDir);

    PRBool mAutomigrate;
    PRBool mOutofDiskSpace;
    PRBool mDiskSpaceErrorQuitCalled;
    PRBool mProfileChangeVetoed;

public:
    nsProfile();
    virtual ~nsProfile();

    // Creates associated user directories on the creation of a new profile
    nsresult CreateUserDirectories(nsIFile *profileDir);

    // Deletes associated user directories
    nsresult DeleteUserDirectories(nsIFile *profileDir);

    // Copies all the registry keys from old profile to new profile
    nsresult CopyRegKey(const PRUnichar *oldProfile, const PRUnichar *newProfile);

    nsresult AutoMigrate();

    nsresult CreateDefaultProfile(void);
    nsresult ShowProfileWizard(void);
};

