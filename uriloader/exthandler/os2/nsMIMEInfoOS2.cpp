/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>  (Added mailcap and mime.types support)
 *   Christian Biesinger <cbiesinger@web.de>
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


#define INCL_DOS
#include <os2.h>

#include "nsMIMEInfoOS2.h"
#include "nsExternalHelperAppService.h"
#include "nsCExternalHandlerService.h"
#include "nsReadableUtils.h"
#include "nsIProcess.h"
#include <stdlib.h>

#define SALT_SIZE 8
#define TABLE_SIZE 36
static const PRUnichar table[] = 
  { 'a','b','c','d','e','f','g','h','i','j',
    'k','l','m','n','o','p','q','r','s','t',
    'u','v','w','x','y','z','0','1','2','3',
    '4','5','6','7','8','9'};


nsMIMEInfoOS2::~nsMIMEInfoOS2()
{
}

NS_IMETHODIMP nsMIMEInfoOS2::LaunchWithFile(nsIFile* aFile)
{
  nsresult rv = NS_OK;

  nsCAutoString path;
  aFile->GetNativePath(path);

  nsIFile* application;
  if (mPreferredAction == useHelperApp) {
    application = mPreferredApplication;
  } else if (mPreferredAction == useSystemDefault) {
    application = mDefaultApplication;
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  // The nsIMIMEInfo should have either the default or preferred 
  // application handler attribute set to match the preferredAction!
  if (!application) {
    HOBJECT hobject = WinQueryObject(path.get());
    if (WinSetObjectData( hobject, "OPEN=DEFAULT" ))
      return NS_OK;
    else
      return NS_ERROR_FAILURE;
  }
  
  ULONG ulAppType;
  nsCAutoString apppath;
  application->GetNativePath(apppath);
  DosQueryAppType(apppath.get(), &ulAppType);
  if (ulAppType & (FAPPTYP_DOS |
                   FAPPTYP_WINDOWSPROT31 |
                   FAPPTYP_WINDOWSPROT |
                   FAPPTYP_WINDOWSREAL)) {
    // if the helper application is a DOS app, create an 8.3 filename
    // we do this even if the filename is valid because it's 8.3, who cares
    nsCOMPtr<nsPIExternalAppLauncher> helperAppService (do_GetService(NS_EXTERNALHELPERAPPSERVICE_CONTRACTID));
    if (helperAppService)
    {
      nsCAutoString leafName; 
      aFile->GetNativeLeafName(leafName);
      const char* lastDot = strrchr(leafName.get(), '.');
      char suffix[CCHMAXPATH + 1] = "";
      if (lastDot)
      {
          strcpy(suffix, lastDot);
      }
      suffix[4] = '\0';
      
      nsAutoString saltedTempLeafName;
      do {
          saltedTempLeafName.Truncate();
          // this salting code was ripped directly from the profile manager.
          // turn PR_Now() into milliseconds since epoch 1058 // and salt rand with that. 
          double fpTime;
          LL_L2D(fpTime, PR_Now());
          srand((uint)(fpTime * 1e-6 + 0.5));
          PRInt32 i;
          for (i=0;i<SALT_SIZE;i++) {
            saltedTempLeafName.Append(table[(rand()%TABLE_SIZE)]);
          }
          AppendASCIItoUTF16(suffix, saltedTempLeafName);
          nsresult rv = aFile->MoveTo(nsnull, saltedTempLeafName);
      } while (NS_FAILED(rv));
      helperAppService->DeleteTemporaryFileOnExit(aFile);
      aFile->GetNativePath(path);
    }
  } else {
    path.Insert('\"', 0);
    path.Append('\"');
  }
    
  const char * strPath = path.get();
  // if we were given an application to use then use it....otherwise
  // make the registry call to launch the app
  nsCOMPtr<nsIProcess> process = do_CreateInstance(NS_PROCESS_CONTRACTID);
  if (NS_FAILED(rv = process->Init(application)))
    return rv;
  PRUint32 pid;
  return process->Run(PR_FALSE, &strPath, 1, &pid);
}

