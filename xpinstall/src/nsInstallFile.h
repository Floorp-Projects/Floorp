/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Daniel Veditz <dveditz@netscape.com>
 *     Douglas Turner <dougt@netscape.com>
 */


#ifndef nsInstallFile_h__
#define nsInstallFile_h__

#include "prtypes.h"
#include "nsString.h"

#include "nsInstallObject.h"

#include "nsInstall.h"
#include "nsInstallVersion.h"

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
        nsInstallFile(  nsInstall* inInstall,
                        const nsString& inVRName,
                        nsIDOMInstallVersion* inVInfo,
                        const nsString& inJarLocation,
                        const nsString& folderSpec,
                        const nsString& inPartialPath,
                        PRBool forceInstall,
                        PRInt32 *error);

        virtual ~nsInstallFile();

        PRInt32 Prepare();
        PRInt32 Complete();
        void  Abort();
        char* toString();

        PRBool CanUninstall();
        PRBool RegisterPackageNode();
	  
  
    private:

        /* Private Fields */
        nsInstallVersion* mVersionInfo;	  /* Version info for this file*/
        
        nsString*     mJarLocation;	      /* Location in the JAR */
        nsFileSpec*   mExtracedFile;	  /* temporary file location */
        nsFileSpec*   mFinalFile;	      /* final file destination */

        nsString*   mVersionRegistryName; /* full version path */
        
        PRBool      mForceInstall;   /* whether install is forced */
        PRBool      mJavaInstall;    /* whether file is installed to a Java directory */
        PRBool      mReplaceFile;    /* whether file exists */
        PRBool      mChildFile;      /* whether file is a child */
        PRBool      mUpgradeFile;    /* whether file is an upgrade */


        PRInt32     CompleteFileMove();
        PRInt32     RegisterInVersionRegistry();
};

#endif /* nsInstallFile_h__ */
