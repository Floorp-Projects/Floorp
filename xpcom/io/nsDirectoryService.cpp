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




#ifdef XP_MAC
#define COMPONENT_REGISTRY_NAME "Component Registry"
#define COMPONENT_DIRECTORY     "Components"
#else
#define COMPONENT_REGISTRY_NAME "component.reg"  
#define COMPONENT_DIRECTORY     "components"    
#endif 




//----------------------------------------------------------------------------------------
static nsresult GetCurrentProcessDirectory(nsILocalFile** aFile)
//----------------------------------------------------------------------------------------
{
   //  Set the component registry location:
    nsresult rv;
    nsCOMPtr<nsIProperties> dirService;
    rv = nsDirectoryService::Create(nsnull, 
                                    NS_GET_IID(nsIProperties), 
                                    getter_AddRefs(dirService));  // needs to be around for life of product

    if (dirService)
    {
      nsCOMPtr <nsILocalFile> aLocalFile;
      dirService->Get("xpcom.currentProcessDirectory", NS_GET_IID(nsILocalFile), getter_AddRefs(aLocalFile));
      if (aLocalFile)
      {
        *aFile = aLocalFile;
        NS_ADDREF(*aFile);
        return NS_OK;
      }
    }

    nsLocalFile* localFile = new nsLocalFile;

    if (localFile == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(localFile);



#ifdef XP_PC
    char buf[MAX_PATH];
    if ( ::GetModuleFileName(0, buf, sizeof(buf)) ) {
        // chop of the executable name by finding the rightmost backslash
        char* lastSlash = PL_strrchr(buf, '\\');
        if (lastSlash)
            *(lastSlash + 1) = '\0';
        
        localFile->InitWithPath(buf);
        *aFile = localFile;
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

        	nsCOMPtr<nsILocalFileMac> localFileMac = do_QueryInterface((nsIFile*)localFile);
		    if (localFileMac) 
          {
				    localFileMac->InitWithFSSpec(&appFSSpec);
            *aFile = localFile;
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
        localFile->InitWithPath(moz5);
        *aFile = localFile;
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
            localFile->InitWithPath(buf);
            *aFile = localFile;
            return NS_OK;
        }
    }

#elif defined(XP_BEOS)

    char *moz5 = getenv("MOZILLA_FIVE_HOME");
    if (moz5)
    {
        localFile->InitWithPath(moz5);
        *aFile = localFile;
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
          localFile->InitWithPath(buf);
          *aFile = localFile;
          return NS_OK;
        }
      }
    }

#endif
    
    if (localFile)
       delete localFile;

    NS_ERROR("unable to get current process directory");
    return NS_ERROR_FAILURE;
} // GetCurrentProcessDirectory()


















nsDirectoryService* nsDirectoryService::mService = nsnull;

nsDirectoryService::nsDirectoryService()
{
    NS_INIT_REFCNT();
    mHashtable = new nsHashtable(256, PR_TRUE);
    NS_ASSERTION(mHashtable != NULL, "hashtable null error");
    
    NS_NewISupportsArray(getter_AddRefs(mProviders));
    NS_ASSERTION(mProviders != NULL, "providers null error");

	RegisterProvider(NS_STATIC_CAST(nsIDirectoryServiceProvider*, this));
}

NS_METHOD
nsDirectoryService::Create(nsISupports *outer, REFNSIID aIID, void **aResult)
{
   NS_ENSURE_ARG_POINTER(aResult);
	 
    if (mService == nsnull)
    {
        mService = new nsDirectoryService();
        if (mService == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    return  mService->QueryInterface(aIID, aResult);
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
    if (mHashtable)
        mHashtable->Enumerate(ReleaseValues);
}

NS_IMPL_ISUPPORTS3(nsDirectoryService, nsIProperties, nsIDirectoryService, nsIDirectoryServiceProvider)


NS_IMETHODIMP
nsDirectoryService::Define(const char* prop, nsISupports* initialValue)
{
    return Set(prop, initialValue);
}

NS_IMETHODIMP
nsDirectoryService::Undefine(const char* prop)
{
    nsStringKey key(prop);
    if (!mHashtable->Exists(&key))
        return NS_ERROR_FAILURE;

    nsISupports* prevValue = (nsISupports*)mHashtable->Remove(&key);
    NS_IF_RELEASE(prevValue);
    return NS_OK;
}

typedef struct FileData

{
  const char* property;
  nsIFile*    file;
  PRBool	  persistant;

} FileData;

static PRBool FindProviderFile(nsISupports* aElement, void *aData)
{
  nsCOMPtr<nsIDirectoryServiceProvider> prov = do_QueryInterface(aElement);
  if (!prov)
    return PR_FALSE;

  FileData* fileData = (FileData*)aData;
  prov->GetFile(fileData->property, &fileData->persistant, &(fileData->file) );
  if (fileData->file)
    return PR_FALSE;

  return PR_TRUE;
}

NS_IMETHODIMP
nsDirectoryService::Get(const char* prop, const nsIID & uuid, void* *result)
{
    nsStringKey key(prop);
    if (!mHashtable->Exists(&key))
    {
      // it is not one of our defaults, lets check any providers
      FileData fileData;
      fileData.property   = prop;
      fileData.file       = nsnull;
      fileData.persistant = PR_TRUE;

      mProviders->EnumerateForwards(FindProviderFile, &fileData);
      
      if (fileData.file)
      {
        if (!fileData.persistant)
		{
			nsresult rv = (fileData.file)->QueryInterface(uuid, result);
			NS_RELEASE(fileData.file);
			return rv;
		}
		Set(prop, NS_STATIC_CAST(nsIFile*, fileData.file));
        NS_RELEASE(fileData.file);
      } 
    }

    
    // now check again to see if it was added above.
    if (mHashtable->Exists(&key))
    {
      nsCOMPtr<nsIFile> ourFile;
      nsISupports* value = (nsISupports*)mHashtable->Get(&key);
      
      if (value && NS_SUCCEEDED(value->QueryInterface(NS_GET_IID(nsIFile), getter_AddRefs(ourFile))))
      {
        nsCOMPtr<nsIFile> cloneFile;
        ourFile->Clone(getter_AddRefs(cloneFile));
        return cloneFile->QueryInterface(uuid, result);
      }
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDirectoryService::Set(const char* prop, nsISupports* value)
{
    nsStringKey key(prop);
    if (mHashtable->Exists(&key) || value == nsnull)
        return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIFile> ourFile;
    value->QueryInterface(NS_GET_IID(nsIFile), getter_AddRefs(ourFile));
    if (ourFile)
    {
        nsIFile* cloneFile;
        ourFile->Clone(&cloneFile);

        nsISupports* prevValue = (nsISupports*)mHashtable->Put(&key, 
			                                                   NS_STATIC_CAST(nsISupports*,cloneFile));
        NS_IF_RELEASE(prevValue);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;   
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

NS_IMETHODIMP
nsDirectoryService::RegisterProvider(nsIDirectoryServiceProvider *prov)
{
  if (!prov)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISupports> supports = do_QueryInterface(prov);
  if (supports)
    return mProviders->AppendElement(supports);
  return NS_ERROR_FAILURE;
}




NS_IMETHODIMP
nsDirectoryService::GetFile(const char *prop, PRBool *persistant, nsIFile **_retval)
{
	nsCOMPtr<nsILocalFile> localFile;
	nsresult rv;

	*_retval = nsnull;
	*persistant = PR_TRUE;

	// check to see if it is one of our defaults
        
    if (strncmp(prop, "xpcom.currentProcess.componentRegistry", 38) == 0)
    {
        rv = GetCurrentProcessDirectory(getter_AddRefs(localFile));
        if (NS_FAILED(rv)) return rv;
		localFile->Append(COMPONENT_REGISTRY_NAME);           
    }
    else if (strncmp(prop, "xpcom.currentProcess.componentDirectory", 39) == 0)
    {
        rv = GetCurrentProcessDirectory(getter_AddRefs(localFile));
        if (NS_FAILED(rv)) return rv;
		localFile->Append(COMPONENT_DIRECTORY);           
    }

	if (localFile)
		return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)_retval);

	return NS_OK;
}




