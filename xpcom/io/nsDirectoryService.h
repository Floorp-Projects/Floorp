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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corp.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsDirectoryService_h___
#define nsDirectoryService_h___

#include "nsIDirectoryService.h"
#include "nsHashtable.h"
#include "nsILocalFile.h"
#include "nsISupportsArray.h"
#include "nsIAtom.h"

#define NS_XPCOM_INIT_CURRENT_PROCESS_DIR       "MozBinD"   // Can be used to set NS_XPCOM_CURRENT_PROCESS_DIR
                                                            // CANNOT be used to GET a location

class nsDirectoryService : public nsIDirectoryService,
                           public nsIProperties,
                           public nsIDirectoryServiceProvider2
{
  public:

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_DIRECTORY_SERVICE_CID);
  
  // nsISupports interface
  NS_DECL_ISUPPORTS

  NS_DECL_NSIPROPERTIES  

  NS_DECL_NSIDIRECTORYSERVICE

  NS_DECL_NSIDIRECTORYSERVICEPROVIDER
  
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER2

  nsDirectoryService();

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

private:
   ~nsDirectoryService();

    nsresult GetCurrentProcessDirectory(nsILocalFile** aFile);
    
    static nsDirectoryService* mService;
    static PRBool PR_CALLBACK ReleaseValues(nsHashKey* key, void* data, void* closure);
    nsSupportsHashtable mHashtable;
    nsCOMPtr<nsISupportsArray> mProviders;

public:
    static nsIAtom *sCurrentProcess;
    static nsIAtom *sComponentRegistry;
    static nsIAtom *sComponentDirectory;
    static nsIAtom *sXPTIRegistry;
    static nsIAtom *sGRE_Directory;
    static nsIAtom *sGRE_ComponentDirectory;
    static nsIAtom *sOS_DriveDirectory;
    static nsIAtom *sOS_TemporaryDirectory;
    static nsIAtom *sOS_CurrentProcessDirectory;
    static nsIAtom *sOS_CurrentWorkingDirectory;
#if defined (XP_MACOSX)
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
    static nsIAtom *sUserLibDirectory;
    static nsIAtom *sHomeDirectory;
    static nsIAtom *sDefaultDownloadDirectory;
    static nsIAtom *sUserDesktopDirectory;
    static nsIAtom *sLocalDesktopDirectory;
    static nsIAtom *sUserApplicationsDirectory;
    static nsIAtom *sLocalApplicationsDirectory;
    static nsIAtom *sUserDocumentsDirectory;
    static nsIAtom *sLocalDocumentsDirectory;
    static nsIAtom *sUserInternetPlugInDirectory;
    static nsIAtom *sLocalInternetPlugInDirectory;
    static nsIAtom *sUserFrameworksDirectory;
    static nsIAtom *sLocalFrameworksDirectory;
    static nsIAtom *sUserPreferencesDirectory;
    static nsIAtom *sLocalPreferencesDirectory;
    static nsIAtom *sPictureDocumentsDirectory;
    static nsIAtom *sMovieDocumentsDirectory;
    static nsIAtom *sMusicDocumentsDirectory;
    static nsIAtom *sInternetSitesDirectory;
#elif defined (XP_WIN) 
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
    static nsIAtom *sWinCookiesDirectory;
#elif defined (XP_UNIX)
    static nsIAtom *sLocalDirectory;
    static nsIAtom *sLibDirectory;
    static nsIAtom *sHomeDirectory;
#elif defined (XP_OS2)
    static nsIAtom *sSystemDirectory;
    static nsIAtom *sOS2Directory;
    static nsIAtom *sHomeDirectory;
    static nsIAtom *sDesktopDirectory;
#elif defined (XP_BEOS)
    static nsIAtom *sSettingsDirectory;
    static nsIAtom *sHomeDirectory;
    static nsIAtom *sDesktopDirectory;
    static nsIAtom *sSystemDirectory;
#endif


};


#endif

