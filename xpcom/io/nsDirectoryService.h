/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     IBM Corp.
 */

#ifndef nsDirectoryService_h___
#define nsDirectoryService_h___

#include "nsIDirectoryService.h"
#include "nsHashtable.h"
#include "nsILocalFile.h"
#include "nsISupportsArray.h"
#include "nsXPIDLString.h"


class nsDirectoryService : public nsIDirectoryService, public nsIProperties, public nsIDirectoryServiceProvider
{
  public:

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_DIRECTORY_SERVICE_CID);
  
  nsresult Init();

  // nsISupports interface
  NS_DECL_ISUPPORTS

  NS_DECL_NSIPROPERTIES  

  NS_DECL_NSIDIRECTORYSERVICE

  NS_DECL_NSIDIRECTORYSERVICEPROVIDER

  nsDirectoryService();
  virtual ~nsDirectoryService();

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

private:
    nsresult GetCurrentProcessDirectory(nsILocalFile** aFile);
    
    static nsDirectoryService* mService;
    static PRBool PR_CALLBACK ReleaseValues(nsHashKey* key, void* data, void* closure);
    nsSupportsHashtable* mHashtable;
    nsCOMPtr<nsISupportsArray> mProviders;

    nsCString mProductName;

    static nsIAtom *sCurrentProcess;
    static nsIAtom *sAppRegistryDirectory;
    static nsIAtom *sAppRegistry;
    static nsIAtom *sComponentRegistry;
    static nsIAtom *sComponentDirectory;
    static nsIAtom *sOS_DriveDirectory;
    static nsIAtom *sOS_TemporaryDirectory;
    static nsIAtom *sOS_CurrentProcessDirectory;
    static nsIAtom *sOS_CurrentWorkingDirectory;
#ifdef XP_MAC
    static nsIAtom *sDirectory;
    static nsIAtom *sDesktopDirectory;
    static nsIAtom *sTrashDirectory;
    static nsIAtom *sStartupDirectory;
    static nsIAtom *sShutdownDirectory;
    static nsIAtom *sAppleMenuDirectory;
    static nsIAtom *sControlPanelDirectory;
    static nsIAtom *sExtensionDirectory;
    static nsIAtom *sFontsDirectory;
    static nsIAtom *sPreferencesDirectory;
    static nsIAtom *sDocumentsDirectory;
    static nsIAtom *sInternetSearchDirectory;
    static nsIAtom *sHomeDirectory;
    static nsIAtom *sDefaultDownloadDirectory;
#elif defined (XP_OS2)
    static nsIAtom *sSystemDirectory;
    static nsIAtom *sOS2Directory;
    static nsIAtom *sHomeDirectory;
    static nsIAtom *sDesktopDirectory;
#elif defined (XP_PC) 
    static nsIAtom *sSystemDirectory;
    static nsIAtom *sWindowsDirectory;
    static nsIAtom *sHomeDirectory;
    static nsIAtom *sDesktop;
    static nsIAtom *sPrograms;
    static nsIAtom *sControls;
    static nsIAtom *sPrinters;
    static nsIAtom *sPersonal;
    static nsIAtom *sFavorites;
    static nsIAtom *sStartup;
    static nsIAtom *sRecent;
    static nsIAtom *sSendto;
    static nsIAtom *sBitbucket;
    static nsIAtom *sStartmenu;
    static nsIAtom *sDesktopdirectory;
    static nsIAtom *sDrives;
    static nsIAtom *sNetwork;
    static nsIAtom *sNethood;
    static nsIAtom *sFonts;
    static nsIAtom *sTemplates;
    static nsIAtom *sCommon_Startmenu;
    static nsIAtom *sCommon_Programs;
    static nsIAtom *sCommon_Startup;
    static nsIAtom *sCommon_Desktopdirectory;
    static nsIAtom *sAppdata;
    static nsIAtom *sPrinthood;
#elif defined (XP_UNIX) 
    static nsIAtom *sLocalDirectory;
    static nsIAtom *sLibDirectory;
    static nsIAtom *sHomeDirectory;
#elif defined (XP_BEOS)
    static nsIAtom *sSettingsDirectory;
    static nsIAtom *sHomeDirectory;
    static nsIAtom *sDesktopDirectory;
    static nsIAtom *sSystemDirectory;
#endif


};


#endif

