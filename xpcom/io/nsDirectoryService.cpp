/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsDirectoryService.h"
#include "nsILocalFile.h"
#include "nsLocalFile.h"
#include "nsDebug.h"

#ifdef XP_MAC
#include <Folders.h>
#include <Files.h>
#include <Memory.h>
#include <Processes.h>
#elif defined(XP_PC)
#include <windows.h>
#include <shlobj.h>
#include <stdlib.h>
#include <stdio.h>
#elif defined(XP_UNIX)
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include "prenv.h"
#elif defined(XP_BEOS)
#include <FindDirectory.h>
#include <Path.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <OS.h>
#include <image.h>
#include "prenv.h"
#endif







//----------------------------------------------------------------------------------------
static nsresult GetCurrentProcessDirectory(nsILocalFile* aFile)
//----------------------------------------------------------------------------------------
{
#ifdef XP_PC
    char buf[MAX_PATH];
    if ( ::GetModuleFileName(0, buf, sizeof(buf)) ) {
        // chop of the executable name by finding the rightmost backslash
        char* lastSlash = PL_strrchr(buf, '\\');
        if (lastSlash)
            *(lastSlash + 1) = '\0';
        
        aFile->InitWithPath(buf);
        return NS_OK;
    }

#elif defined(XP_MAC)
    // get info for the the current process to determine the directory
    // its located in
    OSErr err;
    ProcessSerialNumber psn;
    if (!(err = GetCurrentProcess(&psn)))
    {
        ProcessInfoRec pInfo;
        FSSpec         tempSpec;

        // initialize ProcessInfoRec before calling
        // GetProcessInformation() or die horribly.
        pInfo.processName = nil;
        pInfo.processAppSpec = &tempSpec;
        pInfo.processInfoLength = sizeof(ProcessInfoRec);

        if (!(err = GetProcessInformation(&psn, &pInfo)))
        {
            FSSpec appFSSpec = *(pInfo.processAppSpec);
            
            // Truncate the nsame so the spec is just to the app directory
            appFSSpec.name[0] = 0;

        	nsCOMPtr<nsILocalFileMac> localFileMac = do_QueryInterface(aFile);
			if (localFileMac) {
				localFileMac->InitWithFSSpec(&appFSSpec);
                return NS_OK;
            }
        }
    }

#elif defined(XP_UNIX)

    // In the absence of a good way to get the executable directory let
    // us try this for unix:
    //	- if MOZILLA_FIVE_HOME is defined, that is it
    //	- else give the current directory
    char buf[MAXPATHLEN];
    char *moz5 = PR_GetEnv("MOZILLA_FIVE_HOME");
    if (moz5)
    {
        aFile->InitWithPath(moz5);
        return NS_OK;
    }
    else
    {
        static PRBool firstWarning = PR_TRUE;

        if(firstWarning) {
            // Warn that MOZILLA_FIVE_HOME not set, once.
            printf("Warning: MOZILLA_FIVE_HOME not set.\n");
            firstWarning = PR_FALSE;
        }

        // Fall back to current directory.
        if (getcwd(buf, sizeof(buf)))
        {
            aFile->InitWithPath(buf);
            return NS_OK;
        }
    }

#elif defined(XP_BEOS)

    char *moz5 = getenv("MOZILLA_FIVE_HOME");
    if (moz5)
    {
        aFile->InitWithPath(moz5);
        return NS_OK;
    }
    else
    {
      static char buf[MAXPATHLEN];
      int32 cookie = 0;
      image_info info;
      char *p;
      *buf = 0;
      if(get_next_image_info(0, &cookie, &info) == B_OK)
      {
        strcpy(buf, info.name);
        if((p = strrchr(buf, '/')) != 0)
        {
          *p = 0;
          aFile->InitWithPath(buf);
          return NS_OK;
        }
      }
    }

#endif

    NS_ERROR("unable to get current process directory");
    return NS_ERROR_FAILURE;
} // GetCurrentProcessDirectory()


















nsDirectoryService* nsDirectoryService::mService = nsnull;

nsDirectoryService::nsDirectoryService(nsISupports* outer)
{
    NS_INIT_AGGREGATED(outer);
}

NS_METHOD
nsDirectoryService::Create(nsISupports *outer, REFNSIID aIID, void **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
	 NS_ENSURE_PROPER_AGGREGATION(outer, aIID);

    if (mService == nsnull)
    {
        mService = new nsDirectoryService(outer);
        if (mService == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    return  mService->AggregatedQueryInterface(aIID, aResult);
}

PRBool
nsDirectoryService::ReleaseValues(nsHashKey* key, void* data, void* closure)
{
    nsISupports* value = (nsISupports*)data;
    NS_IF_RELEASE(value);
    return PR_TRUE;
}

nsDirectoryService::~nsDirectoryService()
{
    Enumerate(ReleaseValues);
}

NS_IMPL_AGGREGATED(nsDirectoryService);

NS_METHOD
nsDirectoryService::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);

	 if (aIID.Equals(NS_GET_IID(nsISupports)))
	     *aInstancePtr = GetInner();
	 else if (aIID.Equals(NS_GET_IID(nsIProperties)))
	     *aInstancePtr = NS_STATIC_CAST(nsIProperties*, this);
	 else {
	     *aInstancePtr = nsnull;
		  return NS_NOINTERFACE;
    } 

	 NS_ADDREF((nsISupports*)*aInstancePtr);
    return NS_OK;
}

NS_IMETHODIMP
nsDirectoryService::Define(const char* prop, nsISupports* initialValue)
{
    nsStringKey key(prop);
    if (Exists(&key))
        return NS_ERROR_FAILURE;

    nsISupports* prevValue = (nsISupports*)Put(&key, initialValue);
    NS_ASSERTION(prevValue == NULL, "hashtable error");
    NS_IF_ADDREF(initialValue);
    return NS_OK;
}

NS_IMETHODIMP
nsDirectoryService::Undefine(const char* prop)
{
    nsStringKey key(prop);
    if (!Exists(&key))
        return NS_ERROR_FAILURE;

    nsISupports* prevValue = (nsISupports*)Remove(&key);
    NS_IF_RELEASE(prevValue);
    return NS_OK;
}

NS_IMETHODIMP
nsDirectoryService::Get(const char* prop, const nsIID & uuid, void* *result)
{
    nsStringKey key(prop);
    if (!Exists(&key))
    {
        // check to see if it is one of our defaults
        
        if (strncmp(prop, "xpcom.currentProcess.componentRegistry", 38) == 0)
        {
            nsLocalFile* localFile = new nsLocalFile;

    if (localFile == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(localFile);

            nsresult rv = GetCurrentProcessDirectory(localFile);
            if (NS_FAILED(rv)) 
                return rv;
 
#ifdef XP_MAC
            localFile->Append("Component Registry");           
#else
            localFile->Append("component.reg");           
#endif /* XP_MAC */
    
            Set(prop, NS_STATIC_CAST(nsILocalFile*, localFile));
            rv =localFile->QueryInterface(uuid, result);
            NS_RELEASE(localFile);
            return rv;
        }
        else if (strncmp(prop, "xpcom.currentProcess.componentDirectory", 39) == 0)
        {
            nsLocalFile* localFile = new nsLocalFile;

    if (localFile == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(localFile);

            nsresult rv = GetCurrentProcessDirectory(localFile);
            if (NS_FAILED(rv)) 
                return rv;
 
#ifdef XP_MAC
            localFile->Append("Components");           
#else
            localFile->Append("components");           
#endif /* XP_MAC */
    
            rv =localFile->QueryInterface(uuid, result);
            NS_RELEASE(localFile);
            return rv;
        }
              
        return NS_ERROR_FAILURE;
    }


    nsISupports* value = (nsISupports*)nsHashtable::Get(&key);
    return value->QueryInterface(uuid, result);
}

NS_IMETHODIMP
nsDirectoryService::Set(const char* prop, nsISupports* value)
{
    nsStringKey key(prop);
    if (Exists(&key))
        return NS_ERROR_FAILURE;

    nsISupports* prevValue = (nsISupports*)Put(&key, value);
    NS_IF_RELEASE(prevValue);
    NS_IF_ADDREF(value);
    return NS_OK;
}

NS_IMETHODIMP
nsDirectoryService::Has(const char *prop, PRBool *_retval)
{
    *_retval = PR_FALSE;
    nsCOMPtr<nsIFile> value;
    nsresult rv = Get(prop, NS_GET_IID(nsIFile), getter_AddRefs(value));
    if (NS_FAILED(rv)) 
        return rv;
    
    if (value)
    {
        *_retval = PR_TRUE;
    }
    
    return rv;
}
