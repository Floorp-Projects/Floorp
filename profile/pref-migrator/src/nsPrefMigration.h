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

#ifndef nsPrefMigration_h___
#define nsPrefMigration_h___

#include "nsPrefMigrationIIDs.h"
#include "nsIPrefMigration.h"

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsFileSpec.h"
#include "nsIPref.h"
#include "nsIServiceManager.h" 
#include "nsCOMPtr.h"
#include "nsIFileSpec.h"
#include "nsPrefMigrationIIDs.h"

class nsPrefMigration: public nsIPrefMigration, public nsIShutdownListener
{
    public:
      NS_DEFINE_STATIC_CID_ACCESSOR(NS_PrefMigration_CID) 

      nsPrefMigration();
      virtual ~nsPrefMigration();

      NS_DECL_ISUPPORTS

      NS_DECL_NSIPREFMIGRATION

	  /* nsIShutdownListener methods */
	  NS_IMETHOD OnShutdown(const nsCID& aClass, nsISupports *service);
	  
    private:

      nsresult CreateNewUser5Tree(const char* oldProfilePath, 
                                  const char* newProfilePath);

      nsresult GetDirFromPref(const char* oldProfilePath,
                              const char* newProfilePath, 
			      const char* newDirName,
                              char* pref, 
                              char** newPath, 
                              char** oldPath);

      nsresult GetSizes(nsFileSpec inputPath,
                        PRBool readSubdirs,
                        PRUint32* sizeTotal);

      nsresult GetDriveName(nsFileSpec inputPath,
                            char** driveName);

      nsresult CheckForSpace(nsFileSpec newProfilePath, 
                             PRFloat64 requiredSpace);

      nsresult DoTheCopy(nsFileSpec oldPath, 
                         nsFileSpec newPath,
                         PRBool readSubdirs); 

      nsresult DoSpecialUpdates(nsFileSpec profilePath);

private:
      nsIPref* m_prefs;
      nsresult getPrefService();
      nsCOMPtr<nsIFileSpec> m_prefsFile; 
};

#endif

