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
