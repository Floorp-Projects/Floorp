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

#ifndef nsWinProfileItem_h__
#define nsWinProfileItem_h__

#include "prtypes.h"
#include "nsSoftwareUpdate.h"
#include "nsInstallObject.h"
#include "nsWinProfile.h"


PR_BEGIN_EXTERN_C

class nsWinProfileItem : public nsInstallObject {

public:

  /* Public Fields */

  /* Public Methods */

  nsWinProfileItem(nsWinProfile* profileObj,
                   nsString sectionName,
                   nsString keyName,
                   nsString val);

  ~nsWinProfileItem();

  /**
   * Completes the install:
   * - writes the data into the .INI file
   */
  PRInt32 Complete();
  PRUnichar* toString();
  
  // no need for special clean-up
  void Abort();
  
  // no need for set-up
  PRInt32 Prepare();
  
  /* should these be protected? */
  PRBool CanUninstall();
  PRBool RegisterPackageNode();
  
private:
  
  /* Private Fields */
  nsWinProfile* mProfile;     // initiating profile object
  nsString*     mSection;     // Name of section
  nsString*     mKey;         // Name of key
  nsString*     mValue;       // data to write
  
  /* Private Methods */
 
};

PR_END_EXTERN_C

#endif /* nsWinProfileItem_h__ */
