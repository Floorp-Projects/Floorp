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

#ifndef nsUninstallObject_h__
#define nsUninstallObject_h__

#include "prtypes.h"
#include "nsSoftwareUpdate.h"


PR_BEGIN_EXTERN_C

struct nsUninstallObject : public nsInstallObject {

public:

  /* Public Fields */

  /* Public Methods */

  nsUninstallObject(nsSoftwareUpdate* inSoftUpdate, char* inRegName, char* *errorMsg);

  virtual ~nsUninstallObject();
  
  /* Complete
   * Uninstalls the package
   */
  char* Complete();
  
  char* Prepare();
  
  void Abort();
  
  char* toString();

  /* should these be protected? */
  PRBool CanUninstall();
  PRBool RegisterPackageNode();
  
private:
  
  /* Private Fields */
  char* regName;     // Registry name of package
  char* userName;    // User name of package
  
  /* Private Methods */
  char* NativeComplete(char* regname);
  
};

PR_END_EXTERN_C

#endif /* nsUninstallObject_h__ */
