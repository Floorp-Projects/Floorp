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
#include "nsIDOMInstallFolder.h"
#include "nsIDOMInstallVersion.h"


class nsInstallPatch : public nsInstallObject 
{
    public:

        nsInstallPatch( nsInstall* inInstall,
                        const nsString& inVRName,
                        nsIDOMInstallVersion* inVInfo,
                        const nsString& inJarLocation,
                        nsIDOMInstallFolder* folderSpec,
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
        
        nsString                mRegistryName;
        nsIDOMInstallVersion*   mVersionInfo;

        nsString                mJarLocation;

        nsString                *mPatchFile;
        
        nsString                mTargetFile;
        nsString                mPatchedFile;

        PRInt32 NativePatch(const nsString &sourceFile, const nsString &patchfile, nsString &newFile);
        PRInt32 NativeReplace (const nsString& target, nsString& tempFile);
        PRInt32 NativeDeleteFile(const nsString& doomedFile);
};

#endif /* nsInstallPatch_h__ */
