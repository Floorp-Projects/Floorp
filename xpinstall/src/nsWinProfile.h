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

#ifndef nsWinProfile_h__
#define nsWinProfile_h__

#include "prtypes.h"

#include "nsInstall.h"
#include "nsFolderSpec.h"

struct nsWinProfile {

public:

  /* Public Fields */

  /* Public Methods */

  nsWinProfile( nsSoftwareUpdate* suObj, nsFolderSpec* folder, char* file );
  
  /**
   * Schedules a write into a windows "ini" file.  "Value" can be
   * null to delete the value, but we don't support deleting an entire
   * section via a null "key".  The actual write takes place during
   * SoftwareUpdate.FinalizeInstall();
   *
   * @return  false for failure, true for success
   */
  PRInt32 writeString( char* section, char* key, char* value, PRInt32* aReturn );
  
  /**
   * Reads a value from a windows "ini" file.  We don't support using
   * a null "key" to return a list of keys--you have to know what you want
   *
   * @return  String value from INI, "" if not found, null if error
   */
  PRInt32 getString( char* section, char* key, nsString* aReturn );
  
  char* getFilename();
  nsInstall* softUpdate();
  
  int finalWriteString( char* section, char* key, char* value );
  
  
private:
  
  /* Private Fields */
  char* filename;
  nsInstall* su;
  
  nsUserTarget* target;
  
  /* Private Methods */
  int nativeWriteString( char* section, char* key, char* value );
  char* nativeGetString( char* section, char* key );
};

#endif /* nsWinProfile_h__ */
