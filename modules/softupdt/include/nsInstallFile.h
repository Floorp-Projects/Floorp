/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef nsInstallFile_h__
#define nsInstallFile_h__

#include "prtypes.h"

#include "nsSoftwareUpdate.h"
#include "nsVersionInfo.h"
#include "nsFolderSpec.h"
#include "nsString.h"
#include "nsVersionInfo.h"

#include "nsTarget.h"


PR_BEGIN_EXTERN_C

struct nsInstallFile : public nsInstallObject {

public:

  /* Public Methods */

  /*	Constructor
        inSoftUpdate    - softUpdate object we belong to
        inComponentName	- full path of the registry component
        inVInfo	        - full version info
        inJarLocation   - location inside the JAR file
        inFinalFileSpec	- final	location on	disk
  */
  nsInstallFile(nsSoftwareUpdate* inSoftUpdate,
                char* inVRName,
                nsVersionInfo* inVInfo,
                char* inJarLocation,
                nsFolderSpec* folderSpec,
                char* inPartialPath,
                PRBool forceInstall,
                char* *errorMsg);

  virtual ~nsInstallFile();

  /* Prepare
   * Extracts file out of the JAR archive into the temp directory
   */
  char* Prepare();

  /* Complete
   * Completes the install:
   * - move the downloaded file to the final location
   * - updates the registry
   */
  char* Complete();

  void Abort();

  char* toString();

/* should these be protected? */
  PRBool CanUninstall();
  PRBool RegisterPackageNode();
	  
  
private:

  /* Private Fields */
  nsString* vrName;	        /* Name of the component */
  nsVersionInfo* versionInfo;	/* Version */
  nsString* jarLocation;	/* Location in the JAR */
  nsString* tempFile;		/* temporary file location */
  nsString* finalFile;		/* final file destination */
  nsString* regPackageName;     /* Name of the package we are installing */
  nsString* userPackageName;    /* User-readable package name */
  PRBool force;                 /* whether install is forced */
  PRBool bJavaDir;              /* whether file is installed to a Java directory */
  PRBool replace;               /* whether file exists */
  PRBool bChild;     /* whether file is a child */
  PRBool bUpgrade;   /* whether file is an upgrade */
  
  /* Private Field Accessors */

  /* Private Methods */

  /* Private Native Methods */
  void NativeAbort();
  int NativeComplete();
  PRBool NativeDoesFileExist();
  void AddToClasspath(nsString* file);
};

PR_END_EXTERN_C

#endif /* nsInstallFile_h__ */
