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
#include "nsILocalFile.h"

#include "nspr.h"

#ifdef XP_MAC
#define INSTALL_LOG NS_LITERAL_CSTRING("Install Log")
#else
#define INSTALL_LOG NS_LITERAL_CSTRING("install.log")
#endif


nsLoggingProgressListener::nsLoggingProgressListener()
    : mLogStream(0)
{
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

NS_IMPL_ISUPPORTS1(nsLoggingProgressListener, nsIXPIListener)

NS_IMETHODIMP
nsLoggingProgressListener::OnInstallStart(const PRUnichar *URL)
{
    nsCOMPtr<nsIFile> iFile;
    nsFileSpec *logFile = nsnull;
    nsresult rv = NS_OK;

    // Not in stub installer
    if (!nsSoftwareUpdate::GetProgramDirectory())
    {
        nsCOMPtr<nsIProperties> dirSvc =
                 do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
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
        rv = iFile->AppendNative(INSTALL_LOG);
    else
        rv = iFile->AppendNative(nsDependentCString(nsSoftwareUpdate::GetLogName()));

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
#ifdef XP_MAC
        else
        {
            nsCOMPtr<nsILocalFileMac> iMacFile = do_QueryInterface(iFile);
            iMacFile->SetFileType('TEXT');
            iMacFile->SetFileCreator('R*ch');
        }
#endif
    }

    if (!bTryProfileDir)
    {
        rv = iFile->IsWritable(&bWritable);
        if (NS_FAILED(rv) || !bWritable)
            bTryProfileDir = PR_TRUE;
    }

    if (bTryProfileDir && !nsSoftwareUpdate::GetProgramDirectory())
    {   
        // failed to create the log file in the application directory
        // so try to create the log file in the user's profile directory
        // while we are not in the stub installer
        nsCOMPtr<nsIProperties> dirSvc =
                 do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        dirSvc->Get(NS_APP_USER_PROFILE_50_DIR, NS_GET_IID(nsIFile),
                    getter_AddRefs(iFile));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        if (!nsSoftwareUpdate::GetLogName())
            rv = iFile->AppendNative(INSTALL_LOG);
        else
            rv = iFile->AppendNative(nsDependentCString(nsSoftwareUpdate::GetLogName()));

        if (NS_FAILED(rv)) return rv;

        bExists = PR_FALSE;
        bWritable = PR_FALSE;
        rv = iFile->Exists(&bExists);
        if (NS_FAILED(rv)) return rv;
        if (!bExists)
        {
            rv = iFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
            if (NS_FAILED(rv)) return rv;

#ifdef XP_MAC
            nsCOMPtr<nsILocalFileMac> iMacFile = do_QueryInterface(iFile);
            iMacFile->SetFileType('TEXT');
            iMacFile->SetFileCreator('R*ch');
#endif
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
    *mLogStream << NS_ConvertUCS2toUTF8(URL).get() << "  --  " << time << nsEndl;
    *mLogStream << "-------------------------------------------------------------------------------" << nsEndl;
    *mLogStream << nsEndl;

    PL_strfree(time);
    if (logFile)
        delete logFile;

    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressListener::OnInstallDone(const PRUnichar *aURL, PRInt32 aStatus)
{
    if (mLogStream == nsnull) return NS_ERROR_NULL_POINTER;

    *mLogStream << nsEndl;

    switch (aStatus)
    {
    case nsInstall::SUCCESS:
        *mLogStream << "     Install completed successfully";
        break;

    case nsInstall::REBOOT_NEEDED:
        *mLogStream << "     Install completed successfully, restart required";
        break;

    case nsInstall::INSTALL_CANCELLED:
        *mLogStream << "     Install cancelled by script";
        break;

    case nsInstall::USER_CANCELLED:
        *mLogStream << "     Install cancelled by user";
        break;

    default:
        *mLogStream << "     Install **FAILED** with error " << aStatus;
        break;
    }

    char* time;
    GetTime(&time);

    *mLogStream << "  --  " << time << nsEndl << nsEndl;

    PL_strfree(time);

    mLogStream->close();
    delete mLogStream;
    mLogStream = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressListener::OnPackageNameSet(const PRUnichar *URL, const PRUnichar* UIPackageName)
{
    if (mLogStream == nsnull) return NS_ERROR_NULL_POINTER;

//    char* time;
//    GetTime(&time);

    nsCString name; name.AssignWithConversion(UIPackageName);
    nsCString uline;
    uline.SetCapacity(name.Length());
    for ( unsigned int i=0; i < name.Length(); ++i)
        uline.Append('-');

    *mLogStream << "     " << name.get() << nsEndl;
    *mLogStream << "     " << uline.get() << nsEndl;

    *mLogStream << nsEndl;
//    *mLogStream << "     Starting Installation at " << time << nsEndl;
//    *mLogStream << nsEndl;


//    PL_strfree(time);
    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressListener::OnItemScheduled(const PRUnichar* message )
{
    return NS_OK;
}

NS_IMETHODIMP
nsLoggingProgressListener::OnFinalizeProgress(const PRUnichar* message, PRInt32 itemNum, PRInt32 totNum )
{
    nsCString messageConverted;
    messageConverted.AssignWithConversion(message);

    if (mLogStream == nsnull) return NS_ERROR_NULL_POINTER;

    *mLogStream << "     [" << (itemNum) << "/" << totNum << "]\t" << messageConverted.get() << nsEndl;
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
nsLoggingProgressListener::OnLogComment(const PRUnichar* comment)
{
    nsCString commentConverted;
    commentConverted.AssignWithConversion(comment);

    if (mLogStream == nsnull) return NS_ERROR_NULL_POINTER;

    *mLogStream << "     ** " << commentConverted.get() << nsEndl;
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
        iFileMac->GetFSSpec(&fsSpec);
        *aOutFileSpec = new nsFileSpec(fsSpec, PR_FALSE);
    }
#else
    nsCAutoString path;
    rv = aInIFile->GetNativePath(path);
    if (NS_SUCCEEDED(rv))
    {
        *aOutFileSpec = new nsFileSpec(path.get(), PR_FALSE);
    }
    // NOTE: don't release path since nsFileSpec's mPath points to it
#endif

    if (!*aOutFileSpec)
        rv = NS_ERROR_FAILURE;

    return rv;
}

