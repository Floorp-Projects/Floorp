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

#ifndef nsInstallExecute_h__
#define nsInstallExecute_h__

#include "prtypes.h"
#include "nsString.h"

#include "nsInstallObject.h"

#include "nsInstall.h"
#include "nsIDOMInstallVersion.h"


class nsInstallExecute : public nsInstallObject 
{
    public:
          
        nsInstallExecute( nsInstall* inInstall,
                          const nsString& inJarLocation,
                          const nsString& inArgs,
                          PRInt32 *error);


        virtual ~nsInstallExecute();

        PRInt32 Prepare();
        PRInt32 Complete();
        void  Abort();
        char* toString();

        PRBool CanUninstall();
        PRBool RegisterPackageNode();
	  
  
    private:
          
        nsString mJarLocation; // Location in the JAR
        nsString mArgs;        // command line arguments
        
        nsFileSpec *mExecutableFile;    // temporary file location
        

        PRInt32 NativeComplete(void);
        void NativeAbort(void);

};

#endif /* nsInstallExecute_h__ */
