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

#ifndef nsFolderSpec_h__
#define nsFolderSpec_h__

#include "prtypes.h"


PR_BEGIN_EXTERN_C

struct nsFolderSpec {

public:

  /* Public Methods */

  /* Constructor
   */
  nsFolderSpec(char* inFolderID , char* inVRPath, char* inPackageName);
  ~nsFolderSpec();
 
   /*
   * GetDirectoryPath
   * returns full path to the directory in the standard URL form
   */
  char* GetDirectoryPath(void);

  /**
   * Returns full path to a file. Makes sure that the full path is bellow
   * this directory (security measure
   * @param relativePath      relative path
   * @return                  full path to a file
   */
  char* MakeFullPath(char* relativePath, char* *errorMsg);

  /**
   * IsJavaCapable
   * returns true if folder is on the Java classpath
   */
  PRBool IsJavaCapable() { return NativeIsJavaDir(); }

  char* toString();

private:

  /* Private Fields */
  char* urlPath;  		// Full path to the directory. Used to cache results from GetDirectoryPath
  char* folderID; 		// Unique string specifying a folder
  char* versionRegistryPath;    // Version registry path of the package
  char* userPackageName;  	// Name of the package presented to the user


  /* Private Methods */

  /*
   * SetDirectoryPath
   * sets full path to the directory in the standard URL form
   */
  
  char* SetDirectoryPath(char* *errorMsg);
  
  /* PickDefaultDirectory
   * asks the user for the default directory for the package
   * stores the choice
   */
  char* PickDefaultDirectory(char* *errorMsg);


  /* Private Native Methods */

  /* NativeGetDirectoryPath
   * gets a platform-specific directory path
   * stores it in urlPath
   */
  int NativeGetDirectoryPath();

  /* GetNativePath
   * returns a native equivalent of a XP directory path
   */
  char* GetNativePath(char* Path);

  /*
   * NativePickDefaultDirectory
   * Platform-specific implementation of GetDirectoryPath
   */
  char* NativePickDefaultDirectory(void);

  PRBool NativeIsJavaDir();

};

PR_END_EXTERN_C

#endif /* nsFolderSpec_h__ */
