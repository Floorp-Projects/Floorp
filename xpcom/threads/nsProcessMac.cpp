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
 *   Samir Gehani <sgehani@netscape.com>
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
#include "prthread.h"

#include <stdlib.h>
#include <Processes.h>

#include "nsILocalFileMac.h"

//-------------------------------------------------------------------//
// nsIProcess implementation
//-------------------------------------------------------------------//
NS_IMPL_ISUPPORTS1(nsProcess, nsIProcess)

//Constructor
nsProcess::nsProcess()
:mExitValue(-1),
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

    mExecutable = executable;
        
    return NS_OK;
}


NS_IMETHODIMP  
nsProcess::Run(PRBool blocking, const char **args, PRUint32 count, PRUint32 *pid)
{
    OSErr err = noErr;
    LaunchParamBlockRec	launchPB;
    FSSpec resolvedSpec;
    Boolean bDone = false;
    ProcessInfoRec info;

    nsCOMPtr<nsILocalFileMac> macExecutable = do_QueryInterface(mExecutable);
    macExecutable->GetFSSpec(&resolvedSpec);

    launchPB.launchAppSpec = &resolvedSpec;
    launchPB.launchAppParameters = NULL;
    launchPB.launchBlockID = extendedBlock;
    launchPB.launchEPBLength = extendedBlockLen;
    launchPB.launchFileFlags = NULL;
    launchPB.launchControlFlags = launchContinue + launchNoFileFlags + launchUseMinimum;
    if (!blocking)
        launchPB.launchControlFlags += launchDontSwitch;

    err = LaunchApplication(&launchPB);
    if (err != noErr)
        return NS_ERROR_FAILURE;
	
    // NOTE: blocking mode assumes you are running on a thread 
    //       other than the UI thread that has teh main event loop
    if (blocking)
    {
        do 
        {   
            info.processInfoLength = sizeof(ProcessInfoRec);
            info.processName = nil;
            info.processAppSpec = nil;
            err = GetProcessInformation(&launchPB.launchProcessSN, &info);
            
            if (err == noErr)
            {
                // still running so sleep some more (200 msecs)
                PR_Sleep(200);
            }
            else
            {
                // no longer in process manager's internal list: so assume done
                bDone = true;
            }
        } 
        while (!bDone);	        
    }

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
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsProcess::GetExitValue(PRInt32 *aExitValue)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

