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
