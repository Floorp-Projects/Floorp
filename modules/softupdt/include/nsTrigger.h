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

#ifndef nsTrigger_h__
#define nsTrigger_h__

#include "prtypes.h"
#include "nsVersionInfo.h"

/**
 * DEFAULT_MODE has UI, triggers conditionally
 * @see StartSoftwareUpdate flags argument
 *
 */
static PRInt32 DEFAULT_MODE = 0;

/**
 * FORCE_MODE will install the package regardless of what was installed previously
 * @see StartSoftwareUpdate flags argument
 */
static PRInt32 FORCE_MODE = 1;

/**
 * SILENT_MODE will not display the UI
 */
static PRInt32 SILENT_MODE = 2;

PR_BEGIN_EXTERN_C

struct nsTrigger {

public:

  /* Public Fields */

  /* Public Methods */
  /**
   * @return true if SmartUpdate is enabled
   */
  static  PRBool UpdateEnabled(void);
  
  /**
   * @param componentName     version registry name of the component
   * @return  version of the package. null if not installed, or SmartUpdate disabled
   */
  static nsVersionInfo* GetVersionInfo( char* componentName );
  
  /**
   * Unconditionally starts the software update
   * @param url   URL of the JAR file
   * @param flags SmartUpdate modifiers (SILENT_INSTALL, FORCE_INSTALL)
   * @return false if SmartUpdate did not trigger
   */
  static PRBool StartSoftwareUpdate( char* url, PRInt32 flags );
  
  
  static PRBool StartSoftwareUpdate( char* url );
  
  
  /**
   * Conditionally starts the software update
   * @param url           URL of JAR file
   * @param componentName name of component in the registry to compare
   *                      the version against.  This doesn't have to be the
   *                      registry name of the installed package but could
   *                      instead be a sub-component -- useful because if a
   *                      file doesn't exist then it triggers automatically.
   * @param diffLevel     Specifies which component of the version must be
   *                      different.  If not supplied BLD_DIFF is assumed.
   *                      If the diffLevel is positive the install is triggered
   *                      if the specified version is NEWER (higher) than the
   *                      registered version.  If the diffLevel is negative then
   *                      the install is triggered if the specified version is
   *                      OLDER than the registered version.  Obviously this
   *                      will only have an effect if FORCE_MODE is also used.
   * @param version       The version that must be newer (can be a VersionInfo
   *                      object or a char* version
   * @param flags         Same flags as StartSoftwareUpdate (force, silent)
   */
  static PRBool
  ConditionalSoftwareUpdate( char* url,
                             char* componentName,
                             PRInt32 diffLevel,
                             nsVersionInfo* version,
                             PRInt32 flags);
  

  static PRBool ConditionalSoftwareUpdate(char* url,
                                          char* componentName,
                                          char* version);
  
  /**
   * Validates existence and compares versions
   *
   * @param regName       name of component in the registry to compare
   *                      the version against.  This doesn't have to be the
   *                      registry name of an installed package but could
   *                      instead be a sub-component.  If the named item
   *                      has a Path property the file must exist or a 
   *                      null version is used in the comparison.
   * @param version       The version to compare against
   */
  static PRInt32 CompareVersion( char* regName, nsVersionInfo* version );
  
  static PRInt32 CompareVersion( char* regName, char* version );
  
  static PRInt32 CompareVersion( char* regName, PRInt32 maj, PRInt32 min, PRInt32 rel, PRInt32 bld );
  
private:
  
  /* Private Fields */
  
  /* Private Methods */
  
};

PR_END_EXTERN_C

#endif /* nsTrigger_h__ */
