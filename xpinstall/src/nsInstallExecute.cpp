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
#include "nsIDOMInstallVersion.h"

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
    if (mExecutableFile)
        delete mExecutableFile;
}



PRInt32 nsInstallExecute::Prepare()
{
    if (mInstall == NULL || mJarLocation == "null") 
        return nsInstall::INVALID_ARGUMENTS;

    return mInstall->ExtractFileFromJar(mJarLocation, nsnull, &mExecutableFile);
}

PRInt32 nsInstallExecute::Complete()
{
    if (mExecutableFile == nsnull)
        return nsInstall::INVALID_ARGUMENTS;

    nsFileSpec appPath( *mExecutableFile, false);
    
    if (!appPath.Exists())
	{
		return nsInstall::INVALID_ARGUMENTS;
	}

    PRInt32 result = appPath.Execute( mArgs );
    
    return result;
}

void nsInstallExecute::Abort()
{
    /* Get the names */
    if (mExecutableFile == nsnull) 
        return;

    mExecutableFile->Delete(PR_FALSE);
    
    if ( mExecutableFile->Exists() )
    {
        /* FIX: need to fe_deletefilelater() or something */
    }
}

char* nsInstallExecute::toString()
{
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

