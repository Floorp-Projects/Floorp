/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Don Bragg <dbragg@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*****************************************************************************
 * 
 * nsProcess is used to execute new processes and specify if you want to
 * wait (blocking) or continue (non-blocking).
 *
 *****************************************************************************
 */

#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsProcess.h"
#include "prtypes.h"
#include "prio.h"
#include "prenv.h"
#include "nsCRT.h"

#include <stdlib.h>

#if defined(XP_WIN)
#include "prmem.h"
#include "nsString.h"
#include "nsLiteralString.h"
#include "nsReadableUtils.h"
#include <windows.h>
#endif

#if defined(XP_MACOSX)
#include <Processes.h>
#include "nsILocalFileMac.h"
#endif

//-------------------------------------------------------------------//
// nsIProcess implementation
//-------------------------------------------------------------------//
NS_IMPL_ISUPPORTS1(nsProcess, nsIProcess)

//Constructor
nsProcess::nsProcess():mExitValue(-1),
                       mProcess(nsnull)
{
}

NS_IMETHODIMP
nsProcess::Init(nsIFile* executable)
{
    PRBool isFile;

    //First make sure the file exists
    nsresult rv = executable->IsFile(&isFile);
    if (NS_FAILED(rv)) return rv;
    if (!isFile)
        return NS_ERROR_FAILURE;

    //Store the nsIFile in mExecutable
    mExecutable = executable;
    //Get the path because it is needed by the NSPR process creation
#ifdef XP_WIN 
    rv = mExecutable->GetNativeTarget(mTargetPath);
    if (NS_FAILED(rv) || mTargetPath.IsEmpty() )
#endif
        rv = mExecutable->GetNativePath(mTargetPath);

    return rv;
}


#if defined(XP_WIN)
static int assembleCmdLine(char *const *argv, char **cmdLine)
{
    char *const *arg;
    char *p, *q;
    int cmdLineSize;
    int numBackslashes;
    int i;
    int argNeedQuotes;

    /*
     * Find out how large the command line buffer should be.
     */
    cmdLineSize = 0;
    for (arg = argv; *arg; arg++) {
        /*
         * \ and " need to be escaped by a \.  In the worst case,
         * every character is a \ or ", so the string of length
         * may double.  If we quote an argument, that needs two ".
         * Finally, we need a space between arguments, and
         * a null byte at the end of command line.
         */
        cmdLineSize += 2 * strlen(*arg)  /* \ and " need to be escaped */
                + 2                      /* we quote every argument */
                + 1;                     /* space in between, or final null */
    }
    p = *cmdLine = (char *) PR_MALLOC(cmdLineSize);
    if (p == NULL) {
        return -1;
    }

    for (arg = argv; *arg; arg++) {
        /* Add a space to separates the arguments */
        if (arg != argv) {
            *p++ = ' '; 
        }
        q = *arg;
        numBackslashes = 0;
        argNeedQuotes = 0;

        /* If the argument contains white space, it needs to be quoted. */
        if (strpbrk(*arg, " \f\n\r\t\v")) {
            argNeedQuotes = 1;
        }

        if (argNeedQuotes) {
            *p++ = '"';
        }
        while (*q) {
            if (*q == '\\') {
                numBackslashes++;
                q++;
            } else if (*q == '"') {
                if (numBackslashes) {
                    /*
                     * Double the backslashes since they are followed
                     * by a quote
                     */
                    for (i = 0; i < 2 * numBackslashes; i++) {
                        *p++ = '\\';
                    }
                    numBackslashes = 0;
                }
                /* To escape the quote */
                *p++ = '\\';
                *p++ = *q++;
            } else {
                if (numBackslashes) {
                    /*
                     * Backslashes are not followed by a quote, so
                     * don't need to double the backslashes.
                     */
                    for (i = 0; i < numBackslashes; i++) {
                        *p++ = '\\';
                    }
                    numBackslashes = 0;
                }
                *p++ = *q++;
            }
        }

        /* Now we are at the end of this argument */
        if (numBackslashes) {
            /*
             * Double the backslashes if we have a quote string
             * delimiter at the end.
             */
            if (argNeedQuotes) {
                numBackslashes *= 2;
            }
            for (i = 0; i < numBackslashes; i++) {
                *p++ = '\\';
            }
        }
        if (argNeedQuotes) {
            *p++ = '"';
        }
    } 

    *p = '\0';
    return 0;
}
#endif

// XXXldb |args| has the wrong const-ness
NS_IMETHODIMP  
nsProcess::Run(PRBool blocking, const char **args, PRUint32 count, PRUint32 *pid)
{
    nsresult rv = NS_OK;

#if defined(XP_MACOSX)
    // You can't pass arguments to mac apps, tell the caller that it
    // just aint going to work.
    if (count) {
        return NS_ERROR_INVALID_ARG;
    }
#else
    // make sure that when we allocate we have 1 greater than the
    // count since we need to null terminate the list for the argv to
    // pass into PR_CreateProcess
    char **my_argv = NULL;
    my_argv = (char **)malloc(sizeof(char *) * (count + 2) );
    if (!my_argv) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // copy the args
    PRUint32 i;
    for (i=0; i < count; i++) {
        my_argv[i+1] = NS_CONST_CAST(char*, args[i]);
    }
    // we need to set argv[0] to the program name.
    my_argv[0] = mTargetPath.BeginWriting();
    // null terminate the array
    my_argv[count+1] = NULL;
#endif

#if defined(XP_WIN)
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION procInfo;
    BOOL retVal;
    char *cmdLine;

    if (assembleCmdLine(my_argv, &cmdLine) == -1) {
        nsMemory::Free(my_argv);
        return NS_ERROR_FILE_EXECUTION_FAILED;    
    }

    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);

    retVal = CreateProcess(NULL,
                           // NS_CONST_CAST(char*, mTargetPath.get()),
                           cmdLine,
                           NULL,  /* security attributes for the new
                                   * process */
                           NULL,  /* security attributes for the primary
                                   * thread in the new process */
                           FALSE,  /* inherit handles */
                           0,     /* creation flags */
                           NULL,  /* env */
                           NULL,  /* current drive and directory */
                           &startupInfo,
                           &procInfo
                          );
    PR_FREEIF( cmdLine );
    if (blocking) {
 
        // if success, wait for process termination. the early returns and such
        // are a bit ugly but preserving the logic of the nspr code I copied to 
        // minimize our risk abit.

        if ( retVal == TRUE ) {
            DWORD dwRetVal;
            unsigned long exitCode;

            dwRetVal = WaitForSingleObject(procInfo.hProcess, INFINITE);
            if (dwRetVal == WAIT_FAILED) {
                nsMemory::Free(my_argv);
                return PR_FAILURE;
            }
            if (GetExitCodeProcess(procInfo.hProcess, &exitCode) == FALSE) {
                mExitValue = exitCode;
                nsMemory::Free(my_argv);
               return PR_FAILURE;
            }
            mExitValue = exitCode;
            CloseHandle(procInfo.hProcess);
        }
        else
            rv = PR_FAILURE;
    } 
    else {

        // map return value into success code

        if ( retVal == TRUE ) 
            rv = PR_SUCCESS;
        else
            rv = PR_FAILURE;
    }

#elif defined(XP_MACOSX)
    FSSpec resolvedSpec;
    OSErr err = noErr;

    nsCOMPtr<nsILocalFileMac> macExecutable = do_QueryInterface(mExecutable);
    macExecutable->GetFSSpec(&resolvedSpec);

    LaunchParamBlockRec launchPB;
    launchPB.launchAppSpec = &resolvedSpec;
    launchPB.launchAppParameters = NULL;
    launchPB.launchBlockID = extendedBlock;
    launchPB.launchEPBLength = extendedBlockLen;
    launchPB.launchFileFlags = NULL;
    launchPB.launchControlFlags =
        launchContinue + launchNoFileFlags + launchUseMinimum;
    if (!blocking)
        launchPB.launchControlFlags += launchDontSwitch;

    err = LaunchApplication(&launchPB);

    // NOTE: blocking mode assumes you are running on a thread
    //       other than the UI thread that has the main event loop
    if (blocking && err == noErr) {
        while (1) {
            ProcessInfoRec info;
            info.processInfoLength = sizeof(ProcessInfoRec);
            info.processName = NULL;
            info.processAppSpec = NULL;

            err = GetProcessInformation(&launchPB.launchProcessSN, &info);

            if (err != noErr) {
                // The process is no longer in the process manager's internal
                // list, assume the process is done.
                err = noErr;

                break;
            }

            // still running so sleep some more (200 msecs)
            PR_Sleep(200);
        }
    }

    if (err != noErr) {
        rv = PR_FAILURE;
    }
#else
    if ( blocking ) {
        mProcess = PR_CreateProcess(mTargetPath.get(), my_argv, NULL, NULL);
        if (mProcess)
            rv = PR_WaitProcess(mProcess, &mExitValue);
    }
    else {
        rv = PR_CreateProcessDetached(mTargetPath.get(), my_argv, NULL, NULL);
    }
#endif

#if !defined(XP_MACOSX)
    // free up our argv
    nsMemory::Free(my_argv);
#endif

    if (rv != PR_SUCCESS)
        return NS_ERROR_FILE_EXECUTION_FAILED;

    return NS_OK;
}

NS_IMETHODIMP nsProcess::InitWithPid(PRUint32 pid)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsProcess::GetLocation(nsIFile** aLocation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsProcess::GetPid(PRUint32 *aPid)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsProcess::GetProcessName(char** aProcessName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsProcess::GetProcessSignature(PRUint32 *aProcessSignature)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsProcess::Kill()
{
    nsresult rv = NS_OK;
    if (mProcess)
        rv = PR_KillProcess(mProcess);
    
    return rv;
}

NS_IMETHODIMP
nsProcess::GetExitValue(PRInt32 *aExitValue)
{
    *aExitValue = mExitValue;
    
    return NS_OK;
}
