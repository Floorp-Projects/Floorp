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

#include "nsSpecialSystemDirectory.h"


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
    NS_ASSERTION(mProviders.get() != NULL, "providers null error");

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
        
    if (strncmp(prop, "xpcom.currentProcess", 38) == 0)
    {
        rv = GetCurrentProcessDirectory(getter_AddRefs(localFile));
    }
    else if (strncmp(prop, "xpcom.currentProcess.componentRegistry", 38) == 0)
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
    
    else if (strncmp(prop, "system.OS_DriveDirectory",  24) == 0) 
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::OS_DriveDirectory);
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.OS_TemporaryDirectory",  28) == 0)
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::OS_TemporaryDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.OS_CurrentProcessDirectory",  35) == 0)  
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::OS_CurrentProcessDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.OS_CurrentWorkingDirectory",  33) == 0)   
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::OS_CurrentWorkingDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
       
#ifdef XP_MAC          
    else if (strncmp(prop, "system.Directory",  22) == 0)  
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Mac_SystemDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.DesktopDirectory",  23) == 0)  
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Mac_DesktopDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.TrashDirectory",  21) == 0)  
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Mac_TrashDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.StartupDirectory",  23) == 0)  
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Mac_StartupDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.ShutdownDirectory",  24) == 0)  
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Mac_ShutdownDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.AppleMenuDirectory",  25) == 0)  
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Mac_AppleMenuDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.ControlPanelDirectory",  28) == 0)  
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Mac_ControlPanelDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.ExtensionDirectory",  25) == 0)  
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Mac_ExtensionDirectory);
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.FontsDirectory",  21) == 0)  
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Mac_FontsDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.PreferencesDirectory",  27) == 0)  
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Mac_PreferencesDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.DocumentsDirectory",  25) == 0)  
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Mac_DocumentsDirectory);
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.InternetSearchDirectory",  30) == 0)  
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Mac_InternetSearchDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }   
#elif defined (XP_PC)        
    else if (strncmp(prop, "system.SystemDirectory",  22) == 0)  
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_SystemDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.WindowsDirectory",  23) == 0)  
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_WindowsDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.HomeDirectory",  20) == 0)  
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_HomeDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Desktop",  14) == 0)      
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Desktop); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Programs",  15) == 0)      
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Programs); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Controls",  15) == 0)      
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Controls); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Printers",  15) == 0)      
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Printers); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Personal",  15) == 0)    
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Personal); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Favorites",  16) == 0)    
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Favorites); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Startup",  14) == 0)    
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Startup);
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Recent",  13) == 0)    
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Recent); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Sendto",  13) == 0)    
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Sendto); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Bitbucket",  16) == 0)    
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Bitbucket); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Startmenu",  16) == 0)    
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Startmenu); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Desktopdirectory",  23) == 0)    
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Desktopdirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Drives",  13) == 0)    
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Drives);
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Network",  14) == 0)    
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Network); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Nethood",  14) == 0)    
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Nethood); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Fonts",  12) == 0)    
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Fonts);
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Templates",  16) == 0)    
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Templates); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Common_Startmenu",  23) == 0)    
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Common_Startmenu); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Common_Programs",  22) == 0)    
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Common_Programs); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Common_Startup",  21) == 0)   
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Common_Startup); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Common_Desktopdirectory",  30) == 0)   
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Common_Desktopdirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Appdata",  14) == 0)    
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Appdata); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.Printhood",  16) == 0)    
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Win_Printhood); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
#elif defined (XP_UNIX)       

    else if (strncmp(prop, "system.LocalDirectory",  21) == 0)
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Unix_LocalDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.LibDirectory",  19) == 0)
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Unix_LibDirectory);
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.HomeDirectory",  20) == 0)
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::Unix_HomeDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
#elif defined (XP_BEOS)
    else if (strncmp(prop, "system.SettingsDirectory",  24) == 0)
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::BeOS_SettingsDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.HomeDirectory",  20) == 0)
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::BeOS_HomeDirectory);
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.DesktopDirectory",  23) == 0)
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::BeOS_DesktopDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
    else if (strncmp(prop, "system.SystemDirectory",  22) == 0)
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::BeOS_SystemDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
#elif defined (XP_OS2)
    else if (strncmp(prop, "system.SystemDirectory",  22) == 0)
    {
        nsSpecialSystemDirectory fileSpec(nsSpecialSystemDirectory::OS2_SystemDirectory); 
        rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(localFile));  
        if (NS_FAILED(rv)) return rv; 
    }
#endif

	if (localFile)
		return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)_retval);

	return NS_OK;
}




