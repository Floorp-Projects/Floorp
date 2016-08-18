/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAppFileLocationProvider_h
#define nsAppFileLocationProvider_h

#include "nsIDirectoryService.h"
#include "nsIFile.h"
#include "mozilla/Attributes.h"

class nsIFile;

//*****************************************************************************
// class nsAppFileLocationProvider
//*****************************************************************************

class nsAppFileLocationProvider final : public nsIDirectoryServiceProvider2
{
public:
  nsAppFileLocationProvider();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER2

private:
  ~nsAppFileLocationProvider()
  {
  }

protected:
  nsresult CloneMozBinDirectory(nsIFile** aLocalFile);
  /**
  * Get the product directory. This is a user-specific directory for storing
  * application settings (e.g. the Application Data directory on windows
  * systems).
  * @param aLocal If true, should try to get a directory that is only stored
  *               locally (ie not transferred with roaming profiles)
  */
  nsresult GetProductDirectory(nsIFile** aLocalFile,
                               bool aLocal = false);
  nsresult GetDefaultUserProfileRoot(nsIFile** aLocalFile,
                                     bool aLocal = false);

#if defined(MOZ_WIDGET_COCOA)
  static bool IsOSXLeopard();
#endif

  nsCOMPtr<nsIFile> mMozBinDirectory;
};

#endif
