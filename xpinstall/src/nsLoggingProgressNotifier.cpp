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
 *     Douglas Turner <dougt@netscape.com>
 */

#include "nsIXPINotifier.h"
#include "nsLoggingProgressNotifier.h"

#include "nsInstall.h"

#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"

#include "nspr.h"



nsLoggingProgressNotifier::nsLoggingProgressNotifier()
{
    NS_INIT_REFCNT();
}

nsLoggingProgressNotifier::~nsLoggingProgressNotifier()
{
    if (mLogStream)
    {
        NS_ASSERTION(PR_FALSE, "We're being destroyed before script finishes!");
        mLogStream->close();
        delete mLogStream;
        mLogStream = 0;
    }
}

NS_IMPL_ISUPPORTS(nsLoggingProgressNotifier, nsIXPINotifier::GetIID());

NS_IMETHODIMP
nsLoggingProgressNotifier::BeforeJavascriptEvaluation(const PRUnichar *URL)
{
    nsSpecialSystemDirectory logFile(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
    logFile += "Install.log";

    mLogStream = new nsOutputFileStream(logFile, PR_WRONLY | PR_CREATE_FILE | PR_APPEND, 0744 );
    if (!mLogStream) 
        return NS_ERROR_NULL_POINTER;

    char* time;
    GetTime(&time);

    mLogStream->seek(logFile.GetFileSize());

    *mLogStream << "---------------------------------------------------------------------------" << nsEndl;
    *mLogStream << nsAutoCString(URL) << "     --     " << time << nsEndl;    
    *mLogStream << "---------------------------------------------------------------------------" << nsEndl;
    *mLogStream << nsEndl;

    PL_strfree(time);
    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressNotifier::AfterJavascriptEvaluation(const PRUnichar *URL)
{
    if (mLogStream == nsnull) return NS_ERROR_NULL_POINTER;
    
    char* time;
    GetTime(&time);

//    *mLogStream << nsEndl;
    *mLogStream << "     Finished Installation  " << time << nsEndl << nsEndl;

    PL_strfree(time);

    mLogStream->close();
    delete mLogStream;
    mLogStream = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressNotifier::InstallStarted(const PRUnichar *URL, const PRUnichar* UIPackageName)
{
    if (mLogStream == nsnull) return NS_ERROR_NULL_POINTER;

//    char* time;
//    GetTime(&time);

    nsCString name(UIPackageName);
    nsCString uline;
    uline.SetCapacity(name.Length());
    for ( int i=0; i < name.Length(); ++i)
        uline.SetCharAt('-', i);

    *mLogStream << "     " << name.GetBuffer() << nsEndl;
    *mLogStream << "     " << uline.GetBuffer() << nsEndl;

    *mLogStream << nsEndl;
//    *mLogStream << "     Starting Installation at " << time << nsEndl;   
//    *mLogStream << nsEndl;


//    PL_strfree(time);
    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressNotifier::ItemScheduled(const PRUnichar* message )
{
    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressNotifier::FinalizeProgress(const PRUnichar* message, PRInt32 itemNum, PRInt32 totNum )
{
    if (mLogStream == nsnull) return NS_ERROR_NULL_POINTER;

    *mLogStream << "     Item [" << (itemNum+1) << "/" << totNum << "]\t" << nsAutoCString(message) << nsEndl;
    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressNotifier::FinalStatus(const PRUnichar *URL, PRInt32 status)
{
    if (mLogStream == nsnull) return NS_ERROR_NULL_POINTER;

    *mLogStream << nsEndl;

    switch (status)
    {
    case nsInstall::SUCCESS:
        *mLogStream << "     Install completed successfully" << nsEndl;
        break;

    case nsInstall::REBOOT_NEEDED:
        *mLogStream << "     Install completed successfully, restart required" << nsEndl;
        break;

    case nsInstall::ABORT_INSTALL:
        *mLogStream << "     Install script aborted" << nsEndl;
        break;

    case nsInstall::USER_CANCELLED:
        *mLogStream << "     Install cancelled by user" << nsEndl;
        break;

    default:
        *mLogStream << "     Install **FAILED** with error " << status << nsEndl;
        break;
    }

    return NS_OK;
}

void 
nsLoggingProgressNotifier::GetTime(char** aString)
{
    PRExplodedTime et;
    char line[256];
    PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &et);
    PR_FormatTimeUSEnglish(line, sizeof(line), "%m/%d/%Y %H:%M:%S", &et);
    *aString = PL_strdup(line);
}

NS_IMETHODIMP
nsLoggingProgressNotifier::LogComment(const PRUnichar* comment)
{
    if (mLogStream == nsnull) return NS_ERROR_NULL_POINTER;

    *mLogStream << "     ** " << nsAutoCString(comment) << nsEndl;    
    return NS_OK;
}

