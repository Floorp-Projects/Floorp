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
#include "nsString.h"

#include "nsInstallObject.h"

#include "nsIDOMInstall.h"
#include "nsIDOMInstallFolder.h"
#include "nsIDOMInstallVersion.h"


class nsInstallFile : public nsInstallObject 
{
    public:

      /*************************************************************
       * Public Methods 
       *
       *	Constructor
       *    inSoftUpdate    - softUpdate object we belong to
       *    inComponentName	- full path of the registry component
       *    inVInfo	        - full version info
       *    inJarLocation   - location inside the JAR file
       *    inFinalFileSpec	- final	location on	disk
       *************************************************************/
        nsInstallFile(  nsIDOMInstall* inInstall,
                        const nsString& inVRName,
                        nsIDOMInstallVersion* inVInfo,
                        const nsString& inJarLocation,
                        nsIDOMInstallFolder* folderSpec,
                        const nsString& inPartialPath,
                        PRBool forceInstall,
                        char**errorMsg);

        virtual ~nsInstallFile();

        char* Prepare();
        char* Complete();
        void  Abort();
        char* toString();

        PRBool CanUninstall();
        PRBool RegisterPackageNode();
	  
  
    private:

        /* Private Fields */
        nsIDOMInstallVersion* mVersionInfo;	        /* Version info for this file*/
        
        nsString*   mJarLocation;	      /* Location in the JAR */
        nsString*   mTempFile;	          /* temporary file location */
        nsString*   mFinalFile;	          /* final file destination */
        nsString*   mVersionRegistryName; /* full version path */
        
        PRBool      mForceInstall;   /* whether install is forced */
        PRBool      mJavaInstall;    /* whether file is installed to a Java directory */
        PRBool      mReplaceFile;    /* whether file exists */
        PRBool      mChildFile;      /* whether file is a child */
        PRBool      mUpgradeFile;     /* whether file is an upgrade */

        int         NativeComplete();
        PRBool      DoesFileExist();
        void        AddToClasspath(nsString* file);
};

#endif /* nsInstallFile_h__ */
