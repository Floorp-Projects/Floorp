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


#include "prmem.h"

#include "nsFileSpec.h"

#include "VerReg.h"
#include "nsInstallExecute.h"

#include "nsInstall.h"
#include "nsIDOMInstallFolder.h"
#include "nsIDOMInstallVersion.h"

#include "nsInstallErrorMessages.h"


nsInstallExecute:: nsInstallExecute(  nsInstall* inInstall,
                                      const nsString& inJarLocation,
                                      const nsString& inArgs,
                                      PRInt32 *error)

: nsInstallObject(inInstall)
{
    if ((inInstall == nsnull) || (inJarLocation == "null"))
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }

    mJarLocation        = inJarLocation;
    mArgs               = inArgs;
    mExecutableFile     = nsnull;

}


nsInstallExecute::~nsInstallExecute()
{
    delete mExecutableFile;
}



PRInt32 nsInstallExecute::Prepare()
{
    if (mInstall == NULL || mJarLocation == "null") 
        return nsInstall::INVALID_ARGUMENTS;

    return mInstall->ExtractFileFromJar(mJarLocation, "", &mExecutableFile);
}

PRInt32 nsInstallExecute::Complete()
{
    if (mExecutableFile == nsnull)
        return nsInstall::INVALID_ARGUMENTS;
    
    char* tempCString = mExecutableFile->ToNewCString();

    nsFileSpec appPath(tempCString , false);
    
    delete [] tempCString;

    if (!appPath.Exists())
	{
		return nsInstall::INVALID_ARGUMENTS;
	}

    tempCString = mArgs.ToNewCString();
	
    PRInt32 result = appPath.Execute(tempCString);
        
    delete [] tempCString;
    
    return result;
}

void nsInstallExecute::Abort()
{
    char* currentName;
    int result;

    /* Get the names */
    if (mExecutableFile == nsnull) 
        return;

    currentName = mExecutableFile->ToNewCString();

    result = PR_Delete(currentName);
    PR_ASSERT(result == 0); /* FIX: need to fe_deletefilelater() or something */
    
    delete currentName;
}

char* nsInstallExecute::toString()
{
    nsString fullPathString(mJarLocation);
    fullPathString.Append(*mExecutableFile);

    if (mExecutableFile == nsnull) 
    {
        // FIX!
       // return nsInstallErrorMessages::GetString(nsInstall::DETAILS_EXECUTE_PROGRESS, fullPathString);
    } 
    else 
    {
        // return nsInstallErrorMessages::GetString(nsInstall::DETAILS_EXECUTE_PROGRESS2, fullPathString);
    }

    return nsnull;
}


PRBool
nsInstallExecute::CanUninstall()
{
    return PR_FALSE;
}

PRBool
nsInstallExecute::RegisterPackageNode()
{
    return PR_FALSE;
}

