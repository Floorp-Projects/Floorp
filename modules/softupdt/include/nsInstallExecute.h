/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsInstallExecute_h__
#define nsInstallExecute_h__

#include "prtypes.h"
#include "nsSoftwareUpdate.h"

PR_BEGIN_EXTERN_C

struct nsInstallExecute  : public nsInstallObject {

public:

  /* Public Fields */

  /* Public Methods */

  /*	Constructor
   *	inJarLocation	- location inside the JAR file
   *	inZigPtr        - pointer to the ZIG *
   */
  nsInstallExecute(nsSoftwareUpdate* inSoftUpdate, char* inJarLocation, char* *errorMsg, char* inArgs);
  
  virtual ~nsInstallExecute();
  
  /* Prepare
   * Extracts	file out of	the	JAR	archive	into the temp directory
   */
  char* Prepare(void);
  
  /* Complete
   * Completes the install by executing the file
   * Security hazard: make sure we request the right permissions
   */
  char* Complete(void);
  
  void Abort(void);
  
  char* toString();
  
  /* should these be protected? */
  PRBool CanUninstall();
  PRBool RegisterPackageNode();
  
private:
  
  /* Private Fields */
  char* jarLocation; // Location in the JAR
  char* tempFile;    // temporary file location
  char* args;        // command line arguments
  char* cmdline;     // constructed command-line
  
  /* Private Methods */
  char* NativeComplete(void);
  void NativeAbort(void);
  
};

PR_END_EXTERN_C

#endif /* nsInstallExecute_h__ */
