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
 *     Daniel Veditz <dveditz@netscape.com>
 */

#include "xpistub.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#include "nsSpecialSystemDirectory.h" 

#include "nscore.h"
#include "nspr.h"

#include "nsStubNotifier.h"

#include "nsISoftwareUpdate.h"
#include "nsSoftwareUpdateIIDs.h"



//------------------------------------------------------------------------
//          globals
//------------------------------------------------------------------------

static nsIXPINotifier      *gNotifier = 0;
static nsISoftwareUpdate   *gXPI = 0;

static NS_DEFINE_IID(kSoftwareUpdateCID, NS_SoftwareUpdate_CID);



//------------------------------------------------------------------------
//          XPI_Init()
//------------------------------------------------------------------------
PR_PUBLIC_API(nsresult) XPI_Init(   pfnXPIStart    startCB, 
                                    pfnXPIProgress progressCB,
                                    pfnXPIFinal    finalCB     )
{
    nsresult    rv;

    rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, 0);
    if (NS_SUCCEEDED(rv))
    {
        rv = nsComponentManager::CreateInstance(kSoftwareUpdateCID, 
                                                nsnull,
                                                nsISoftwareUpdate::GetIID(),
                                                (void**) &gXPI);

        if (NS_SUCCEEDED(rv))
        {
            gNotifier = new nsStubNotifier( startCB, progressCB, finalCB );
            if (!gNotifier)
            {
                gXPI->Release();
                rv = NS_ERROR_OUT_OF_MEMORY;
            }
            else
                gNotifier->AddRef();
        }
    }

    return rv;
}



//------------------------------------------------------------------------
//          XPI_Exit()
//------------------------------------------------------------------------
PR_PUBLIC_API(void) XPI_Exit()
{
    if (gNotifier)
        gNotifier->Release();

    if (gXPI)
        gXPI->Release();

    // XXX How do I shut down XPCOM? Do I need to?
}




//------------------------------------------------------------------------
//          XPI_Install()
//------------------------------------------------------------------------
PR_PUBLIC_API(nsresult) XPI_Install(
#ifndef XP_MAC
                                    const char* aFile, 
#else
                                    const FSSpec& aFile,
#endif
                                    const char* aArgs, 
                                    long aFlags         )
{
    nsresult                rv = NS_ERROR_NULL_POINTER;
    nsString                args(aArgs);
    nsCOMPtr<nsIFileSpec>   iFile;
    nsFileSpec              file(aFile);
    nsFileURL               URL(file);
    nsString                URLstr(URL.GetURLString());

    NS_NewFileSpecWithSpec( file, getter_AddRefs(iFile) );

    if (iFile && gXPI)
        rv = gXPI->InstallJar( iFile, URLstr.GetUnicode(), args.GetUnicode(), 
                               aFlags, gNotifier );

    return rv;
}
