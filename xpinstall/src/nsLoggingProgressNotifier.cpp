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

#include "nsIXPInstallProgress.h"
#include "nsLoggingProgressNotifier.h"

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
}

NS_IMPL_ISUPPORTS(nsLoggingProgressNotifier, nsIXPInstallProgress::GetIID());

NS_IMETHODIMP
nsLoggingProgressNotifier::BeforeJavascriptEvaluation()
{
    nsSpecialSystemDirectory logFile(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
    logFile += "Install.log";

    mLogStream = new nsOutputFileStream(logFile, PR_WRONLY | PR_CREATE_FILE | PR_APPEND, 0744 );
    mLogStream->seek(logFile.GetFileSize());
    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressNotifier::AfterJavascriptEvaluation()
{
    char* time;
    GetTime(&time);

    *mLogStream << nsEndl;
    *mLogStream << "     Finished Installation  " << time << nsEndl << nsEndl;

    PL_strfree(time);

    mLogStream->close();
    delete mLogStream;
    mLogStream = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressNotifier::InstallStarted(const char* UIPackageName)
{
    if (mLogStream == nsnull) return -1;

    char* time;
    GetTime(&time);
    
    *mLogStream << "---------------------------------------------------------------------------" << nsEndl;
    *mLogStream << UIPackageName << nsEndl;    
    *mLogStream << "---------------------------------------------------------------------------" << nsEndl;
    *mLogStream << nsEndl;
    *mLogStream << "     Starting Installation at " << nsAutoCString(time) << nsEndl;   
    *mLogStream << nsEndl;


    PL_strfree(time);
    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressNotifier::ItemScheduled(const char* message )
{
    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressNotifier::InstallFinalization(const char* message, PRInt32 itemNum, PRInt32 totNum )
{
    if (mLogStream == nsnull) return -1;

    *mLogStream << "     Item [" << (itemNum+1) << "/" << totNum << "]\t" << message << nsEndl;
    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressNotifier::InstallAborted()
{
    if (mLogStream == nsnull) return -1;

    char* time;
    GetTime(&time);
    
    *mLogStream << "     Aborted Installation at " << time << nsEndl << nsEndl;

    PL_strfree(time);
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
    
