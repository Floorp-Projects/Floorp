/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
