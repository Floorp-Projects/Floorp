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
 *     Douglas Turner <dougt@netscape.com>
 *     Pierre Phaneuf <pp@ludusdesign.com>
 *     Samir Gehani <sgehani@netscape.com>
 */

#include "nsLoggingProgressNotifier.h"

#include "nsInstall.h"

#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"

#include "nspr.h"

#ifdef XP_MAC
#define INSTALL_LOG "Install Log"
#else
#define INSTALL_LOG "install.log"
#endif


nsLoggingProgressListener::nsLoggingProgressListener()
    : mLogStream(0)
{
    NS_INIT_ISUPPORTS();
}

nsLoggingProgressListener::~nsLoggingProgressListener()
{
    if (mLogStream)
    {
        NS_WARN_IF_FALSE(PR_FALSE, "We're being destroyed before script finishes!");
        mLogStream->close();
        delete mLogStream;
        mLogStream = 0;
    }
}

NS_IMPL_ISUPPORTS(nsLoggingProgressListener, NS_GET_IID(nsIXPIListener));

NS_IMETHODIMP
nsLoggingProgressListener::BeforeJavascriptEvaluation(const PRUnichar *URL)
{
    nsCOMPtr<nsIFile> iFile;
    nsFileSpec *logFile = nsnull;
    nsresult rv = NS_OK;

    // Not in stub installer
    if (!nsSoftwareUpdate::GetProgramDirectory()) 
    {
        NS_WITH_SERVICE(nsIProperties, dirSvc, 
                        NS_DIRECTORY_SERVICE_PROGID, &rv);
        if (!dirSvc) return NS_ERROR_FAILURE;
        dirSvc->Get(NS_OS_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile),
                    getter_AddRefs(iFile));
    }
    // In stub installer
    else
    {
        rv = nsSoftwareUpdate::GetProgramDirectory()->Clone(
             getter_AddRefs(iFile));
    }

    if (NS_FAILED(rv)) return rv;

    if (!nsSoftwareUpdate::GetLogName())
        rv = iFile->Append(INSTALL_LOG);
    else
        rv = iFile->Append(nsSoftwareUpdate::GetLogName());

    if (NS_FAILED(rv)) return rv;

    // create log file if it doesn't exist (to work around a mac filespec bug)
    PRBool bExists = PR_FALSE, bTryProfileDir = PR_FALSE, bWritable = PR_FALSE;
    rv = iFile->Exists(&bExists);
    if (NS_FAILED(rv)) return rv;
    if (!bExists)
    {
        rv = iFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
        if (NS_FAILED(rv))
            bTryProfileDir = PR_TRUE;
    }
            
    if (!bTryProfileDir)
    {
        rv = iFile->IsWritable(&bWritable);
        if (NS_FAILED(rv) || !bWritable)
            bTryProfileDir = PR_TRUE;
    }

    if (bTryProfileDir)
    {
        // failed to create the log file in the application directory 
        // so try to create the log file in the user's profile directory
        NS_WITH_SERVICE(nsIProperties, dirSvc, 
                        NS_DIRECTORY_SERVICE_PROGID, &rv);
        if (!dirSvc) return NS_ERROR_FAILURE;
        dirSvc->Get(NS_APP_USER_PROFILE_50_DIR, NS_GET_IID(nsIFile),
                    getter_AddRefs(iFile));
    
        if (!nsSoftwareUpdate::GetLogName())
            rv = iFile->Append(INSTALL_LOG);
        else
            rv = iFile->Append(nsSoftwareUpdate::GetLogName());

        if (NS_FAILED(rv)) return rv;

        bExists = PR_FALSE;
        bWritable = PR_FALSE;
        rv = iFile->Exists(&bExists);
        if (NS_FAILED(rv)) return rv;
        if (!bExists)
        {
            rv = iFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
            if (NS_FAILED(rv)) return rv;
        }
         
        rv = iFile->IsWritable(&bWritable);
        if (NS_FAILED(rv) || !bWritable) return NS_ERROR_FAILURE;
    }

    rv = Convert_nsIFile_To_nsFileSpec(iFile, &logFile);
    if (NS_FAILED(rv)) return rv;
    if (!logFile) return NS_ERROR_NULL_POINTER;

    mLogStream = new nsOutputFileStream(*logFile, PR_WRONLY | PR_CREATE_FILE | PR_APPEND, 0744 );
    if (!mLogStream) 
        return NS_ERROR_NULL_POINTER;

    char* time;
    GetTime(&time);

    mLogStream->seek(logFile->GetFileSize());

    *mLogStream << "-------------------------------------------------------------------------------" << nsEndl;
    *mLogStream << NS_ConvertUCS2toUTF8(URL) << "  --  " << time << nsEndl;
    *mLogStream << "-------------------------------------------------------------------------------" << nsEndl;
    *mLogStream << nsEndl;

    PL_strfree(time);
    if (logFile)
        delete logFile;

    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressListener::AfterJavascriptEvaluation(const PRUnichar *URL)
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
nsLoggingProgressListener::InstallStarted(const PRUnichar *URL, const PRUnichar* UIPackageName)
{
    if (mLogStream == nsnull) return NS_ERROR_NULL_POINTER;

//    char* time;
//    GetTime(&time);

    nsCString name; name.AssignWithConversion(UIPackageName);
    nsCString uline;
    uline.SetCapacity(name.Length());
    for ( unsigned int i=0; i < name.Length(); ++i)
        uline.Append('-');

    *mLogStream << "     " << name.GetBuffer() << nsEndl;
    *mLogStream << "     " << uline.GetBuffer() << nsEndl;

    *mLogStream << nsEndl;
//    *mLogStream << "     Starting Installation at " << time << nsEndl;   
//    *mLogStream << nsEndl;


//    PL_strfree(time);
    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressListener::ItemScheduled(const PRUnichar* message )
{
    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressListener::FinalizeProgress(const PRUnichar* message, PRInt32 itemNum, PRInt32 totNum )
{
    if (mLogStream == nsnull) return NS_ERROR_NULL_POINTER;

    *mLogStream << "     [" << (itemNum) << "/" << totNum << "]\t" << NS_ConvertUCS2toUTF8(message) << nsEndl;
    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressListener::FinalStatus(const PRUnichar *URL, PRInt32 status)
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
        *mLogStream << "     Install cancelled by script" << nsEndl;
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
nsLoggingProgressListener::GetTime(char** aString)
{
    PRExplodedTime et;
    char line[256];
    PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &et);
    PR_FormatTimeUSEnglish(line, sizeof(line), "%m/%d/%Y %H:%M:%S", &et);
    *aString = PL_strdup(line);
}

NS_IMETHODIMP
nsLoggingProgressListener::LogComment(const PRUnichar* comment)
{
    if (mLogStream == nsnull) return NS_ERROR_NULL_POINTER;

    *mLogStream << "     ** " << NS_ConvertUCS2toUTF8(comment) << nsEndl;    
    return NS_OK;
}

nsresult
Convert_nsIFile_To_nsFileSpec(nsIFile *aInIFile, nsFileSpec **aOutFileSpec)
{
    nsresult rv = NS_OK;

    if (!aInIFile || !aOutFileSpec)
        return NS_ERROR_FAILURE;

    *aOutFileSpec = nsnull;
    
#ifdef XP_MAC
    FSSpec fsSpec;
    nsCOMPtr<nsILocalFileMac> iFileMac;

    iFileMac = do_QueryInterface(aInIFile, &rv);
    if (NS_SUCCEEDED(rv))
    {
        iFileMac->GetResolvedFSSpec(&fsSpec);
        *aOutFileSpec = new nsFileSpec(fsSpec, PR_FALSE);
    }
#else
    char *path = nsnull;

    rv = aInIFile->GetPath(&path);
    if (NS_SUCCEEDED(rv))
    {
        *aOutFileSpec = new nsFileSpec(path, PR_FALSE);
    }
    // NOTE: don't release path since nsFileSpec's mPath points to it 
#endif

    if (!*aOutFileSpec)
        rv = NS_ERROR_FAILURE;

    return rv;
}
