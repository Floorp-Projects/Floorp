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

#ifndef nsVersionRegistry_h__
#define nsVersionRegistry_h__

#include "prtypes.h"
#include "nsVersionInfo.h"


PR_BEGIN_EXTERN_C

struct nsVersionRegistry {

public:

  /* Public Fields */

  /* Public Methods */

  /**
   * Return the physical disk path for the specified component.
   * @param   component  Registry path of the item to look up in the Registry
   * @return  The disk path for the specified component; NULL indicates error
   * @see     VersionRegistry#checkComponent
   */
  static char* componentPath( char* component );
  
  /**
   * Return the version information for the specified component.
   * @param   component  Registry path of the item to look up in the Registry
   * @return  A VersionInfo object for the specified component; NULL indicates error
   * @see     VersionRegistry#checkComponent
   */
  static nsVersionInfo* componentVersion( char* component );
  
  static char* getDefaultDirectory( char* component );
  
  static PRInt32 setDefaultDirectory( char* component, char* directory );
  
  static PRInt32 installComponent( char* name, char* path, nsVersionInfo* version );
  
  static PRInt32 installComponent( char* name, char* path, nsVersionInfo* version, PRInt32 refCount );
  
  /**
   * Delete component and all sub-components.
   * @param   component  Registry path of the item to delete
   * @return  Error code
   */
  static PRInt32 deleteComponent( char* component );
  
  /**
   * Check the status of a named components.
   * @param   component  Registry path of the item to check
   * @return  Error code.  REGERR_OK means the named component was found in
   * the registry, the filepath referred to an existing file, and the
   * checksum matched the physical file. Other error codes can be used to
   * distinguish which of the above was not true.
   */
  static PRInt32 validateComponent( char* component );
  
  /**
   * verify that the named component is in the registry (light-weight
   * version of validateComponent since it does no disk access).
   * @param  component   Registry path of the item to verify
   * @return Error code. REGERR_OK means it is in the registry. 
   * REGERR_NOFIND will usually be the result otherwise.
   */
  static PRInt32 inRegistry( char* component );
  
  /**
   * Closes the registry file.
   * @return  Error code
   */
  static PRInt32 close();
  
  /**
   * Returns an enumeration of the Version Registry contents.  Use the
   * enumeration methods of the returned object to fetch individual
   * elements sequentially.
   */
  static void* elements();
  
  /**
   * Set the refcount of a named component.
   * @param   component  Registry path of the item to check
   * @param   refcount  value to be set
   * @return  Error code
   */
  static PRInt32 setRefCount( char* component, PRInt32 refcount );
  
  /**
   * Return the refcount of a named component.
   * @param   component  Registry path of the item to check
   * @return  the value of refCount
   */
  static PRInt32 getRefCount( char* component );
  
  /**
   * Creates a node for the item in the Uninstall list.
   * @param   regPackageName  Registry name of the package we are installing
   * @return  userPackagename User-readable package name
   * @return  Error code
   */
  static PRInt32 uninstallCreate( char* regPackageName, char* userPackageName );
  
  static PRInt32 uninstallCreateNode( char* regPackageName, char* userPackageName );
  
  /**
   * Adds the file as a property of the Shared Files node under the appropriate 
   * packageName node in the Uninstall list.
   * @param   regPackageName  Registry name of the package installed
   * @param   vrName  registry name of the shared file
   * @return  Error code
   */
  static PRInt32 uninstallAddFile( char* regPackageName, char* vrName );
  
  static PRInt32 uninstallAddFileToList( char* regPackageName, char* vrName );
  
  /**
   * Checks if the shared file exists in the uninstall list of the package
   * @param   regPackageName  Registry name of the package installed
   * @param   vrName  registry name of the shared file
   * @return true or false 
   */
  static PRInt32 uninstallFileExists( char* regPackageName, char* vrName );
  
  static PRInt32 uninstallFileExistsInList( char* regPackageName, char* vrName );
  
  static char* getUninstallUserName( char* regPackageName );
  
  
private:
  
  /* Private Fields */
  
  /* Private Methods */
  
  /**
   * This class is simply static function wrappers; don't "new" one
   */
  nsVersionRegistry();
  
  /**
   * Replaces all '/' with '_',in the given char*.If an '_' already exists in the
   * given char*, it is escaped by adding another '_' to it.
   * @param   regPackageName  Registry name of the package we are installing
   * @return  modified char*
   */
  static char* convertPackageName( char* regPackageName );
  
};

/* XXX: We should see whether we need the following class or not. 
 * Because there is no Enumerator class in C++ 
 */
struct VerRegEnumerator {
  
public:
  PRBool hasMoreElements();

  void* nextElement();
  
private:
  char*   path;
  PRInt32 state;

  char* regNext();

};

PR_END_EXTERN_C

#endif /* nsVersionRegistry_h__ */
