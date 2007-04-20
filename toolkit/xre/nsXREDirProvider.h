/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsXREDirProvider_h__
#define _nsXREDirProvider_h__

#include "nsIDirectoryService.h"
#include "nsIProfileMigrator.h"
#include "nsILocalFile.h"

class nsXREDirProvider : public nsIDirectoryServiceProvider2,
                         public nsIProfileStartup
{
public:
  // we use a custom isupports implementation (no refcount)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  NS_DECL_NSIDIRECTORYSERVICEPROVIDER
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER2
  NS_DECL_NSIPROFILESTARTUP

  nsXREDirProvider();

  // if aXULAppDir is null, use gArgv[0]
  nsresult Initialize(nsIFile *aXULAppDir,
                      nsILocalFile *aGREDir,
                      nsIDirectoryServiceProvider* aAppProvider = nsnull);
  ~nsXREDirProvider();

  // We only set the profile dir, we don't ensure that it exists;
  // that is the responsibility of the toolkit profile service.
  // We also don't fire profile-changed notifications... that is
  // the responsibility of the apprunner.
  nsresult SetProfile(nsIFile* aProfileDir, nsIFile* aProfileLocalDir);

  void DoShutdown();

  nsresult GetProfileDefaultsDir(nsIFile* *aResult);

  static nsresult GetUserAppDataDirectory(nsILocalFile* *aFile) {
    return GetUserDataDirectory(aFile, PR_FALSE);
  }
  static nsresult GetUserLocalDataDirectory(nsILocalFile* *aFile) {
    return GetUserDataDirectory(aFile, PR_TRUE);
  }

  /* make sure you clone it, if you need to do stuff to it */
  nsIFile* GetGREDir() { return mGREDir; }
  nsIFile* GetAppDir() { 
    if (mXULAppDir)
      return mXULAppDir;
    return mGREDir;
  }

  /**
   * Get the directory under which update directory is created.
   * This method may be called before XPCOM is started. aResult
   * is a clone, it may be modified.
   */
  nsresult GetUpdateRootDir(nsIFile* *aResult);

  /**
   * Get the profile startup directory as determined by this class or by
   * mAppProvider. This method may be called before XPCOM is started. aResult
   * is a clone, it may be modified.
   */
  nsresult GetProfileStartupDir(nsIFile* *aResult);

  /**
   * Get the profile directory as determined by this class or by an
   * embedder-provided XPCOM directory provider. Only call this method
   * when XPCOM is initialized! aResult is a clone, it may be modified.
   */
  nsresult GetProfileDir(nsIFile* *aResult);

protected:
  nsresult GetFilesInternal(const char* aProperty, nsISimpleEnumerator** aResult);
  static nsresult GetUserDataDirectory(nsILocalFile* *aFile, PRBool aLocal);
  static nsresult EnsureDirectoryExists(nsIFile* aDirectory);
  void EnsureProfileFileExists(nsIFile* aFile);

  nsCOMPtr<nsIDirectoryServiceProvider> mAppProvider;
  nsCOMPtr<nsILocalFile> mGREDir;
  nsCOMPtr<nsIFile>      mXULAppDir;
  nsCOMPtr<nsIFile>      mProfileDir;
  nsCOMPtr<nsIFile>      mProfileLocalDir;
  PRBool                 mProfileNotified;
};

#endif
