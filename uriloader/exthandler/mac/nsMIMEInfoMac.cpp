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

#include <LaunchServices.h>

#include "nsMIMEInfoMac.h"
#include "nsILocalFileMac.h"

NS_IMETHODIMP
nsMIMEInfoMac::LaunchWithFile(nsIFile* aFile)
{
  nsIFile* application;

  if (mPreferredAction == useHelperApp)
    application = mPreferredApplication;
  else if (mPreferredAction == useSystemDefault)
    application = mDefaultApplication;
  else
    return NS_ERROR_INVALID_ARG;

  if (application) {
    nsresult rv;
    nsCOMPtr<nsILocalFileMac> app = do_QueryInterface(application, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsILocalFile> docToLoad = do_QueryInterface(aFile, &rv);
    if (NS_FAILED(rv)) return rv;

    return app->LaunchWithDoc(docToLoad, PR_FALSE); 
  }
#ifdef XP_MACOSX
  // We didn't get an application to handle the file from aMIMEInfo, ask LaunchServices directly
  nsresult rv;
  nsCOMPtr <nsILocalFileMac> tempFile = do_QueryInterface(aFile, &rv);
  if (NS_FAILED(rv)) return rv;
  
  FSRef tempFileRef;
  tempFile->GetFSRef(&tempFileRef);

  FSRef appFSRef;
  if (::LSGetApplicationForItem(&tempFileRef, kLSRolesAll, &appFSRef, nsnull) == noErr)
  {
    nsCOMPtr<nsILocalFileMac> app(do_CreateInstance("@mozilla.org/file/local;1"));
    if (!app) return NS_ERROR_FAILURE;
    app->InitWithFSRef(&appFSRef);
    
    nsCOMPtr <nsILocalFile> docToLoad = do_QueryInterface(aFile, &rv);
    if (NS_FAILED(rv)) return rv;
    
    rv = app->LaunchWithDoc(docToLoad, PR_FALSE); 
  }
  return rv;
#endif
}
