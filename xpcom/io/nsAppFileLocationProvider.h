/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDirectoryService.h"
#include "nsIFile.h"

class nsIFile;

//*****************************************************************************
// class nsAppFileLocationProvider
//*****************************************************************************   

class nsAppFileLocationProvider : public nsIDirectoryServiceProvider2
{
public:
                        nsAppFileLocationProvider();

   NS_DECL_ISUPPORTS
   NS_DECL_NSIDIRECTORYSERVICEPROVIDER
   NS_DECL_NSIDIRECTORYSERVICEPROVIDER2

private:
                        ~nsAppFileLocationProvider() {}

protected:
   NS_METHOD            CloneMozBinDirectory(nsIFile **aLocalFile);
   /**
    * Get the product directory. This is a user-specific directory for storing
    * application settings (e.g. the Application Data directory on windows
    * systems).
    * @param aLocal If true, should try to get a directory that is only stored
    *               locally (ie not transferred with roaming profiles)
    */
   NS_METHOD            GetProductDirectory(nsIFile **aLocalFile,
                                            bool aLocal = false);
   NS_METHOD            GetDefaultUserProfileRoot(nsIFile **aLocalFile,
                                                  bool aLocal = false);

#if defined(MOZ_WIDGET_COCOA)
   static bool          IsOSXLeopard();
#endif

   nsCOMPtr<nsIFile> mMozBinDirectory;
};
