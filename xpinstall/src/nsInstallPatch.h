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

#ifndef nsInstallPatch_h__
#define nsInstallPatch_h__

#include "prtypes.h"
#include "nsString.h"

#include "nsInstallObject.h"

#include "nsInstall.h"
#include "nsInstallFolder.h"
#include "nsIDOMInstallVersion.h"


class nsInstallPatch : public nsInstallObject 
{
    public:

        nsInstallPatch( nsInstall* inInstall,
                        const nsString& inVRName,
                        nsIDOMInstallVersion* inVInfo,
                        const nsString& inJarLocation,
                        const nsString& folderSpec,
                        const nsString& inPartialPath,
                        PRInt32 *error);

        nsInstallPatch( nsInstall* inInstall,
                        const nsString& inVRName,
                        nsIDOMInstallVersion* inVInfo,
                        const nsString& inJarLocation,
                        PRInt32 *error);

        virtual ~nsInstallPatch();

        PRInt32 Prepare();
        PRInt32 Complete();
        void  Abort();
        char* toString();

        PRBool CanUninstall();
        PRBool RegisterPackageNode();
	  
  
    private:
        
        
        nsInstallVersion        *mVersionInfo;
        
        nsFileSpec              *mTargetFile;
        nsFileSpec              *mPatchFile;
        nsFileSpec              *mPatchedFile;

        nsString                *mJarLocation;
        nsString                *mRegistryName;
        
       

        PRInt32  NativePatch(const nsFileSpec &sourceFile, const nsFileSpec &patchfile, nsFileSpec **newFile);
        PRInt32  NativeReplace (const nsFileSpec& target, nsFileSpec& tempFile);
        PRInt32  NativeDeleteFile(nsFileSpec* doomedFile);
        void*    HashFilePath(const nsFilePath& aPath);
};

#endif /* nsInstallPatch_h__ */
