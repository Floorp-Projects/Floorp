/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Daniel Veditz <dveditz@netscape.com>
 *     Douglas Turner <dougt@netscape.com>
 */



#include "prmem.h"

#include "nsFileSpec.h"

#include "VerReg.h"
#include "nsInstallExecute.h"
#include "nsInstallResources.h"
#include "ScheduledTasks.h"

#include "nsInstall.h"
#include "nsIDOMInstallVersion.h"
#include "nsProcess.h"
#include "nsReadableUtils.h"

static NS_DEFINE_CID(kIProcessCID, NS_PROCESS_CID); 

MOZ_DECL_CTOR_COUNTER(nsInstallExecute)

nsInstallExecute:: nsInstallExecute(  nsInstall* inInstall,
                                      const nsString& inJarLocation,
                                      const nsString& inArgs,
                                      const PRBool inBlocking,
                                      PRInt32 *error)

: nsInstallObject(inInstall)
{
    MOZ_COUNT_CTOR(nsInstallExecute);

    if ((inInstall == nsnull) || (inJarLocation.IsEmpty()) )
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }

    mJarLocation        = inJarLocation;
    mArgs               = inArgs;
    mExecutableFile     = nsnull;
    mBlocking           = inBlocking;
    mPid                = nsnull;
}


nsInstallExecute::~nsInstallExecute()
{

    MOZ_COUNT_DTOR(nsInstallExecute);
}



PRInt32 nsInstallExecute::Prepare()
{
    if (mInstall == NULL || mJarLocation.IsEmpty()) 
        return nsInstall::INVALID_ARGUMENTS;

    return mInstall->ExtractFileFromJar(mJarLocation, nsnull, getter_AddRefs(mExecutableFile));
}

PRInt32 nsInstallExecute::Complete()
{
    PRInt32 result = NS_OK;
    PRInt32 rv = nsInstall::SUCCESS;
    char *cArgs[1];

    if (mExecutableFile == nsnull)
        return nsInstall::INVALID_ARGUMENTS;

    nsCOMPtr<nsIProcess> process = do_CreateInstance(kIProcessCID);

    cArgs[0] = ToNewCString(mArgs);

    if(cArgs[0] == nsnull)
      return nsInstall::OUT_OF_MEMORY;

    result = process->Init(mExecutableFile);
    if (NS_SUCCEEDED(result))
    {
        result = process->Run(mBlocking, (const char**)&cArgs, 1, mPid);
        if (NS_SUCCEEDED(result))
        {
            if (mBlocking)
            {
                // should be OK to delete now since execution done
                DeleteFileNowOrSchedule( mExecutableFile );
            }
            else
            {
                // don't try to delete now since execution is async
                ScheduleFileForDeletion( mExecutableFile );
            }
        }
        else
            rv = nsInstall::UNEXPECTED_ERROR;
    }
    else
        rv = nsInstall::UNEXPECTED_ERROR;

    if(cArgs[0])
        Recycle(cArgs[0]);

    return rv;
}

void nsInstallExecute::Abort()
{
    /* Get the names */
    if (mExecutableFile == nsnull) 
        return;

    DeleteFileNowOrSchedule(mExecutableFile);
}

char* nsInstallExecute::toString()
{
    char* buffer = new char[1024];
    char* rsrcVal = nsnull;

    if (buffer == nsnull || !mInstall)
        return nsnull;

    // if the FileSpec is NULL, just us the in jar file name.

    if (mExecutableFile == nsnull)
    {
        char *tempString = ToNewCString(mJarLocation);
        rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("Execute"));

        if (rsrcVal)
        {
            sprintf( buffer, rsrcVal, tempString);
            nsCRT::free(rsrcVal);
        }
        
        if (tempString)
            Recycle(tempString);
    }
    else
    {
        rsrcVal = mInstall->GetResourcedString(NS_ConvertASCIItoUCS2("Execute"));

        if (rsrcVal)
        {
            char* temp;
            mExecutableFile->GetPath(&temp);
            sprintf( buffer, rsrcVal, temp);
            nsCRT::free(rsrcVal);
        }
    }
    return buffer;
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

