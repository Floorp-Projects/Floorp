/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsInstallPatch_h__
#define nsInstallPatch_h__

#include "prtypes.h"
#include "nsString.h"

#include "nsInstallObject.h"

#include "nsInstall.h"
#include "nsInstallFolder.h"
#include "nsInstallVersion.h"


class nsInstallPatch : public nsInstallObject 
{
    public:

        nsInstallPatch( nsInstall* inInstall,
                        const nsString& inVRName,
                        const nsString& inVInfo,
                        const nsString& inJarLocation,
                        nsInstallFolder* folderSpec,
                        const nsString& inPartialPath,
                        PRInt32 *error);

        nsInstallPatch( nsInstall* inInstall,
                        const nsString& inVRName,
                        const nsString& inVInfo,
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
        
        nsCOMPtr<nsIFile>       mTargetFile;
        nsCOMPtr<nsIFile>       mPatchFile;
        nsCOMPtr<nsIFile>       mPatchedFile;

        nsString                *mJarLocation;
        nsString                *mRegistryName;
        
       

        PRInt32  NativePatch(nsIFile *sourceFile, nsIFile *patchfile, nsIFile **newFile);
        void*    HashFilePath(nsIFile* aPath);
};

#endif /* nsInstallPatch_h__ */
