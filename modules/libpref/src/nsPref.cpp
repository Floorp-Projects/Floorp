/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Communications Corporation.    Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

#include "nsIPref.h"
#include "nsISecurityPref.h"

#include "nsIFileSpec.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "nsVoidArray.h"

#include "pratom.h"
#include "prefapi.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#ifdef XP_MAC
#include "nsIPrompt.h"
#include "nsIStreamListener.h"
#endif /* XP_MAC */
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsFileStream.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIProfile.h"
#include "nsQuickSort.h"

#include "nsTextFormatter.h"

#include "plhash.h"
#include "prmem.h"
#include "plstr.h"
#include "prprf.h"

#include "nsIJSRuntimeService.h"
#include "jsapi.h"

#include "nsQuickSort.h"
#include "nsXPIDLString.h"
#include "nsScriptSecurityManager.h"
#include "nsISignatureVerifier.h"
#include "nsIStringBundle.h"

#ifdef _WIN32
#include "windows.h"
#endif /* _WIN32 */

#define PREFS_HEADER_LINE_1 "# Mozilla User Preferences"
#define PREFS_HEADER_LINE_2	"// This is a generated file!"

#define INITIAL_MAX_DEFAULT_PREF_FILES 10
#include "prefapi_private_data.h"

#if defined(DEBUG_mcafee)
#define DEBUG_prefs
#endif

static NS_DEFINE_CID(kSecurityManagerCID,   NS_SCRIPTSECURITYMANAGER_CID);
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

//========================================================================================
class nsPref: public nsIPref, public nsISecurityPref
//========================================================================================
{
    NS_DECL_ISUPPORTS

public:
    static nsPref *GetInstance();

    /* Use xpidl-generated macro to declare everything required by nsIPref */
    NS_DECL_NSIPREF
    NS_DECL_NSISECURITYPREF

protected:

    nsPref();
    virtual ~nsPref();

    nsresult GetConfigContext(JSContext **js_context);
    nsresult GetGlobalConfigObject(JSObject **js_object);
    nsresult GetPrefConfigObject(JSObject **js_object);

    nsresult EvaluateConfigScript(const char * js_buffer,
                                  PRUint32 length,
                                  PRBool bGlobalContext, 
                                  PRBool bCallbacks);

    nsresult EvaluateConfigScriptFile(const char * js_buffer,
                                      PRUint32 length,
                                      nsIFileSpec* fileSpec, 
                                      PRBool bGlobalContext, 
                                      PRBool bCallbacks);
    
    nsresult useDefaultPrefFile();
    nsresult useUserPrefFile();
    nsresult useLockPrefFile();
    nsresult getLockPrefFileInfo();
    inline static nsresult SecurePrefCheck(const char* aPrefName);

    static nsPref *gInstance;
    
    nsIFileSpec*                    mFileSpec;
    nsIFileSpec*                    mLIFileSpec;

    static nsresult convertUTF8ToUnicode(const char *utf8String,
                                         PRUnichar **aResult);
}; // class nsPref

nsPref* nsPref::gInstance = NULL;

static PRInt32 g_InstanceCount = 0;

static PrefResult pref_OpenFileSpec(
    nsIFileSpec* fileSpec,
    PRBool is_error_fatal,
    PRBool verifyHash,
    PRBool bGlobalContext,
    PRBool skipFirstLine);

static PRBool pref_VerifyLockFileSpec(char* buf, long buflen);
extern "C" void pref_Alert(char* msg);

//----------------------------------------------------------------------------------------
static nsresult _convertRes(int res)
//----------------------------------------------------------------------------------------
{
    switch (res)
    {
        case PREF_OUT_OF_MEMORY:
            return NS_ERROR_OUT_OF_MEMORY;
        case PREF_NOT_INITIALIZED:
            return NS_ERROR_NOT_INITIALIZED;
        case PREF_TYPE_CHANGE_ERR:
        case PREF_ERROR:
        case PREF_BAD_LOCKFILE:
        case PREF_DEFAULT_VALUE_NOT_INITIALIZED:
            return NS_ERROR_UNEXPECTED;
        case PREF_VALUECHANGED:
            return NS_PREF_VALUE_CHANGED;
    }

    NS_ASSERTION((res >= PREF_DEFAULT_VALUE_NOT_INITIALIZED) && (res <= PREF_PROFILE_UPGRADE), "you added a new error code to prefapi.h and didn't update _convertRes");

    return NS_OK;
}

//----------------------------------------------------------------------------------------
// So discouraged is the use of nsIFileSpec, nobody wanted to have this routine be
// public - It might lead to continued use of nsIFileSpec. Right now, this code has
// such a need for it, here it is. Let's stop having to use it though.
static nsresult _nsIFileToFileSpec(nsIFile* inFile, nsIFileSpec **aFileSpec)
//----------------------------------------------------------------------------------------
{
   nsresult rv;
   nsCOMPtr<nsIFileSpec> newFileSpec;
   nsXPIDLCString pathBuf;
   
   rv = inFile->GetPath(getter_Copies(pathBuf));
   if (NS_FAILED(rv)) return rv;
   rv = NS_NewFileSpec(getter_AddRefs(newFileSpec));
   if (NS_FAILED(rv)) return rv;
   rv = newFileSpec->SetNativePath((const char *)pathBuf);
   if (NS_FAILED(rv)) return rv;
   
   *aFileSpec = newFileSpec;
   NS_ADDREF(*aFileSpec);
   
   return NS_OK;
}


//----------------------------------------------------------------------------------------
nsPref::nsPref()
//----------------------------------------------------------------------------------------
	:	mFileSpec(nsnull)
	,   mLIFileSpec(nsnull)
{
    PR_AtomicIncrement(&g_InstanceCount);
    NS_INIT_REFCNT();
}

//----------------------------------------------------------------------------------------
nsPref::~nsPref()
//----------------------------------------------------------------------------------------
{
    NS_IF_RELEASE(mFileSpec);
    NS_IF_RELEASE(mLIFileSpec);
    ShutDown();
    PR_AtomicDecrement(&g_InstanceCount);
    gInstance = NULL;
}

//----------------------------------------------------------------------------------------
nsresult nsPref::useDefaultPrefFile()
//----------------------------------------------------------------------------------------
{
   nsresult rv;
   nsCOMPtr<nsIFile> aFile;
      
   // Anything which calls NS_InitXPCOM will have this
   rv = NS_GetSpecialDirectory(NS_APP_PREFS_50_FILE, getter_AddRefs(aFile));

   if (!aFile)
   {
      // We know we have XPCOM directory services, but we might not have a provider which
      // knows about NS_APP_PREFS_50_FILE. Put the file in NS_XPCOM_CURRENT_PROCESS_DIR.
      rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR, getter_AddRefs(aFile));
      if (NS_FAILED(rv)) return rv;
      rv = aFile->Append("default_prefs.js");
      if (NS_FAILED(rv)) return rv;
   } 
   
   nsCOMPtr<nsIFileSpec> prefsFileSpec;
   
   // TODO: modify the rest of this code to take
   // nsIFile and not do this conversion into nsIFileSpec
    rv = _nsIFileToFileSpec(aFile, getter_AddRefs(prefsFileSpec));
    if (NS_FAILED(rv)) return rv;
      
    rv = ReadUserPrefsFrom(prefsFileSpec);
    if (NS_SUCCEEDED(rv)) {
        return rv;
    }

    // need to save the prefs now
    mFileSpec = prefsFileSpec;
    NS_ADDREF(mFileSpec);
    rv = SavePrefFile(); 

    return rv;
} // nsPref::useDefaultPrefFile

//----------------------------------------------------------------------------------------
nsresult nsPref::useUserPrefFile()
//----------------------------------------------------------------------------------------
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIFile> aFile;
 
    static const char* userFiles[] = {"user.js"};
    
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(aFile));
    if (NS_SUCCEEDED(rv) && aFile)
    {
        rv = aFile->Append(userFiles[0]);
        if (NS_SUCCEEDED(rv))
        {
    nsCOMPtr<nsIFileSpec> userPrefFile;
            
            // TODO: modify the rest of this code to take
            // nsIFile and not do this conversion into nsIFileSpec
            rv = _nsIFileToFileSpec(aFile, getter_AddRefs(userPrefFile));
            
            if (NS_SUCCEEDED(rv))
            {
                if (NS_FAILED(StartUp()))
    	            return NS_ERROR_FAILURE;

                if (pref_OpenFileSpec(userPrefFile, PR_FALSE, PR_FALSE, PR_FALSE, PR_TRUE)
                    != PREF_NOERROR)
                    rv = NS_ERROR_FAILURE;
            }
        }
    }
    return rv;
} // nsPref::useUserPrefFile

//----------------------------------------------------------------------------------------
nsresult nsPref::getLockPrefFileInfo()
//----------------------------------------------------------------------------------------
{
    nsresult rv = NS_OK;
 
    gLockFileName = nsnull;
    gLockVendor = nsnull;
 
    if (gLockInfoRead)
        return rv;
    
    gLockInfoRead = PR_TRUE;

    if (NS_SUCCEEDED(rv = CopyCharPref("general.config.filename",
			&gLockFileName)) && (gLockFileName)) 
    {
#ifdef NS_DEBUG
        printf("\ngLockFileName %s \n", gLockFileName);
#endif
        if (NS_SUCCEEDED(rv = CopyCharPref("general.config.vendor",
			&gLockVendor)) && (gLockVendor)) 
        {
#ifdef NS_DEBUG
            printf("\ngLockVendor %s \n", gLockVendor);
#endif
        }
        else
        {
            /* No vendor name specified, hence cannot verify it */
            rv = NS_ERROR_FAILURE;
            return rv;
        }
    }
    else
    {
        /* config file is not specified, hence no need to read it.*/
        rv =  NS_OK;
        return rv;
    }
    return rv;
} // nsPref::getLockPrefFileInfo


//----------------------------------------------------------------------------------------
nsresult nsPref::useLockPrefFile()
//----------------------------------------------------------------------------------------
{
    nsresult rv = NS_OK;
    PrefResult result = PREF_NOERROR;
    nsCOMPtr<nsIFileSpec> lockPrefFile;
    nsXPIDLString prefVal;
    nsXPIDLCString lockFileName;
    nsXPIDLCString lockVendor;
    char *return_error = nsnull;
    PRUint32 fileNameLen = 0;
    PRUint32 vendorLen = 0;
    nsXPIDLCString configFile;
    if (NS_SUCCEEDED(rv = GetLocalizedUnicharPref("browser.startup.homepage",
			getter_Copies(prefVal)) && (prefVal))) 
    {
        printf("\nStartup homepage %s \n", (const char *)NS_ConvertUCS2toUTF8(prefVal));
    }

    if (NS_SUCCEEDED(rv = CopyCharPref("general.config.filename",
			getter_Copies(lockFileName)) && (lockFileName)))
    {
#ifdef NS_DEBUG
        printf("\nlockFile %s \n", (const char *)lockFileName);
#endif
    }

    if (NS_SUCCEEDED(rv = CopyCharPref("general.config.vendor",
			getter_Copies(lockVendor)) && (lockVendor))) 
    {
#ifdef NS_DEBUG
        printf("\nlockVendor %s \n", (const char *)lockVendor);
#endif
    }

    if ((gLockFileName == nsnull) && (gLockVendor == nsnull))
    {
        if ((lockFileName == nsnull) && (lockVendor == nsnull)) 
        {
            return NS_OK;
        }
        if ((lockFileName == nsnull) || (lockVendor == nsnull))
        {
            return_error = PL_strdup("The required configuration filename or vendor name is incorrect.Please check your preferences.");
            if (return_error) 
            {
                pref_Alert(return_error);
                return NS_ERROR_FAILURE;
            }
        }

        fileNameLen = PL_strlen(lockFileName);
        vendorLen = PL_strlen(lockVendor);
        if (PL_strncmp(lockFileName, lockVendor, fileNameLen -4) != 0)
        {
            return_error = PL_strdup("The required configuration filename and vendor name do not match.Please check your preferences.");
            if (return_error) 
            {
                pref_Alert(return_error);
                PR_FREEIF(return_error);
                return NS_ERROR_FAILURE;
            }            
        }
        else
        {
            configFile = nsXPIDLCString::Copy(lockFileName);
            if(configFile == nsnull)
                return NS_ERROR_FAILURE;
        }
    }
    else if ((gLockFileName == nsnull) || (gLockVendor == nsnull))
    {
        return_error = PL_strdup("The required configuration filename or vendor name is incorrect.Please contact your administrator.");
        if (return_error) 
        {
            pref_Alert(return_error);
            PR_FREEIF(return_error);
            return NS_ERROR_FAILURE;
        }      
    }
    else
    {
        fileNameLen = PL_strlen(gLockFileName);
        vendorLen = PL_strlen(gLockVendor);
        if (PL_strncmp(gLockFileName, gLockVendor, fileNameLen -4) != 0)
        {
            return_error = PL_strdup("The required configuration filename and vendor name do not match.Please contact your administrator.");
            if (return_error) 
            {
                pref_Alert(return_error);
                PR_FREEIF(return_error);
                return NS_ERROR_FAILURE;
            }            
        }
        else
        {
            configFile = nsXPIDLCString::Copy(lockFileName);
            if(configFile == nsnull)
                return NS_ERROR_FAILURE;
        }
    }

    
    {
        nsCOMPtr<nsIFile> aFile; 
        rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR, getter_AddRefs(aFile));
        
#ifdef XP_MAC
        aFile->Append("Essential Files");
#endif
        // TODO: Make the rest of this code use nsIFile and
        // avoid this conversion
        rv = _nsIFileToFileSpec(aFile, getter_AddRefs(lockPrefFile));
        
        if (NS_SUCCEEDED(rv) && lockPrefFile) 
        {
            if (NS_SUCCEEDED(lockPrefFile->AppendRelativeUnixPath(configFile)))
            {
                if (NS_FAILED(StartUp()))
    	            return NS_ERROR_FAILURE;

	            JS_BeginRequest(gMochaContext);
                result = pref_OpenFileSpec(lockPrefFile, PR_TRUE, PR_TRUE, PR_FALSE, PR_FALSE);
                if (result != PREF_NOERROR)
                {
                    rv = NS_ERROR_FAILURE;
                    if (result != PREF_BAD_LOCKFILE)
                    {
                        return_error = PL_strdup("The required configuration file netscape.cfg could not be found. Please reinstall the software or contact your administrator.");
                        if (return_error) 
                        {
                            pref_Alert(return_error);
                            PR_FREEIF(return_error);
                        }
                    }
                }
                JS_EndRequest(gMochaContext);
            }
        }
    GetLocalizedUnicharPref("browser.startup.homepage",getter_Copies(prefVal));
    printf("\nStartup homepage %s \n", (const char *)NS_ConvertUCS2toUTF8(prefVal));
    }
    return rv;
} // nsPref::useLockPrefFile

//----------------------------------------------------------------------------------------
nsPref* nsPref::GetInstance()
//----------------------------------------------------------------------------------------
{
    if (!gInstance)
    {
        gInstance = new nsPref();
        gInstance->StartUp();
    }
    return gInstance;
} // nsPref::GetInstance

//----------------------------------------------------------------------------------------
nsresult nsPref::SecurePrefCheck(const char* aPrefName)
//----------------------------------------------------------------------------------------
{
    static const char capabilityPrefix[] = "capability.";
    if ((aPrefName[0] == 'c' || aPrefName[0] == 'C') &&
        PL_strncasecmp(aPrefName, capabilityPrefix, sizeof(capabilityPrefix)-1) == 0)
    {
        nsresult rv;
        NS_WITH_SERVICE(nsIScriptSecurityManager, secMan, kSecurityManagerCID, &rv);
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        PRBool enabled;
        rv = secMan->IsCapabilityEnabled("CapabilityPreferencesAccess", &enabled);
        if (NS_FAILED(rv) || !enabled)
            return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsPref, nsIPref, nsISecurityPref);

//========================================================================================
// nsIPref Implementation
//========================================================================================

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::StartUp()
// It must be safe to call this multiple times.
//----------------------------------------------------------------------------------------
{
    return PREF_Init(nsnull) ? NS_OK : NS_ERROR_FAILURE;
} // nsPref::StartUp

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::ReadUserPrefs()
//----------------------------------------------------------------------------------------
{
    nsresult rv = StartUp(); // just to be sure
    if (NS_SUCCEEDED(rv)) {
      rv = getLockPrefFileInfo();
	  rv = useDefaultPrefFile();  // really should return a value...
    }
    if (NS_SUCCEEDED(rv))
		  useUserPrefFile(); 
/*
#ifndef NS_DEBUG
#ifndef XP_MAC
    rv = useLockPrefFile();
#endif
#endif
*/

    return rv;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::ReadUserPrefsFrom(nsIFileSpec* inFile)
//----------------------------------------------------------------------------------------
{
    if (mFileSpec == inFile)
        return NS_OK;

    NS_IF_RELEASE(mFileSpec);
    mFileSpec = inFile;
    NS_ADDREF(mFileSpec);
    
    gErrorOpeningUserPrefs = PR_FALSE;

    if (NS_FAILED(StartUp()))
    	return NS_ERROR_FAILURE;

	nsresult rv = NS_OK;
    if (pref_OpenFileSpec(mFileSpec, PR_TRUE, PR_FALSE, PR_FALSE, PR_TRUE)
        != PREF_NOERROR)
        rv = NS_ERROR_FAILURE;
    // pref_OpenFileSpec will set this for us, we don't need to.
    //    gErrorOpeningUserPrefs = NS_FAILED(rv);
    return rv;
} // nsPref::ReadUserPrefsFrom

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::ResetPrefs()
//----------------------------------------------------------------------------------------
{
    nsresult rv;
    PREF_CleanupPrefs();
    rv = StartUp();
    return rv;
} // nsPref::ResetPrefs


//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::ShutDown()
//----------------------------------------------------------------------------------------
{
    printf("PREF_Cleanup()\n");
    PREF_Cleanup();
    return NS_OK;
} // nsPref::ShutDown

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::ReadUserJSFile(nsIFileSpec* fileSpec)
//----------------------------------------------------------------------------------------
{
    return pref_OpenFileSpec(fileSpec, PR_FALSE, PR_FALSE, PR_TRUE, PR_FALSE);
}

#ifdef MOZ_OLD_LI_STUFF
//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::ReadLIJSFile(nsIFileSpec* fileSpec)
//----------------------------------------------------------------------------------------
{
    NS_IF_RELEASE(mLIFileSpec);
    mLIFileSpec = fileSpec;
    NS_IF_ADDREF(mLIFileSpec);
    return pref_OpenFileSpec(fileSpec, PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE);
}
#endif

//----------------------------------------------------------------------------------------
nsresult nsPref::EvaluateConfigScript(const char * js_buffer,
                         PRUint32 length,
                         PRBool bGlobalContext, 
                         PRBool bCallbacks)
//----------------------------------------------------------------------------------------
{
    return _convertRes(PREF_EvaluateConfigScript(js_buffer,
                                 length,
                                 nsnull, // bad, but not used for parsing.
                                 bGlobalContext,
                                 bCallbacks,
                                 PR_TRUE));
}

//----------------------------------------------------------------------------------------
nsresult nsPref::EvaluateConfigScriptFile(const char * js_buffer,
                         PRUint32 length,
                         nsIFileSpec* fileSpec, 
                         PRBool bGlobalContext, 
                         PRBool bCallbacks)
//----------------------------------------------------------------------------------------
{
    nsXPIDLCString path;
    fileSpec->GetNativePath(getter_Copies(path));
    return _convertRes(PREF_EvaluateConfigScript(js_buffer,
                                 length,
                                 (const char *)path, // bad, but not used for parsing.
                                 bGlobalContext,
                                 bCallbacks,
                                 PR_TRUE));
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::SavePrefFileAs(nsIFileSpec* fileSpec)
//----------------------------------------------------------------------------------------
{
    return _convertRes(PREF_SavePrefFileSpecWith(fileSpec, (PLHashEnumerator)pref_savePref));
}


#ifdef MOZ_OLD_LI_STUFF
//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::SaveLIPrefFile(nsIFileSpec* fileSpec)
//----------------------------------------------------------------------------------------
{
    if (!gHashTable)
        return PREF_NOT_INITIALIZED;
    PREF_SetSpecialPrefsLocal();
    return _convertRes(PREF_SavePrefFileSpecWith(fileSpec, (PLHashEnumerator)pref_saveLIPref));
}
#endif /* MOZ_OLD_LI_STUFF */

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::SavePrefFile()
//----------------------------------------------------------------------------------------
{
    if (!gHashTable || !mFileSpec)
        return PREF_NOT_INITIALIZED;
    return _convertRes(PREF_SavePrefFileSpecWith(mFileSpec, (PLHashEnumerator)pref_savePref));
}

/*
 * JS stuff
 */

nsresult nsPref::GetConfigContext(JSContext **js_context)
{
    return _convertRes(PREF_GetConfigContext(js_context));
}

nsresult nsPref::GetGlobalConfigObject(JSObject **js_object)
{
    return _convertRes(PREF_GetGlobalConfigObject(js_object));
}

nsresult nsPref::GetPrefConfigObject(JSObject **js_object)
{
    return _convertRes(PREF_GetPrefConfigObject(js_object));
}

/*
 * Getters
 */
NS_IMETHODIMP nsPref::GetPrefType(const char *pref, PRInt32 * return_type)
{
  // XXX The current implementation returns the raw type - the
  // enums defining the types have been duplicated. If the old
  // types don't all make sense in the context of the new API,
  // we might need to do some translation.

  *return_type = PREF_GetPrefType(pref);
  return NS_OK;
}

NS_IMETHODIMP nsPref::GetIntPref(const char *pref, PRInt32 * return_int)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_GetIntPref(pref, return_int, PR_FALSE));
}

NS_IMETHODIMP nsPref::GetBoolPref(const char *pref, PRBool * return_val)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_GetBoolPref(pref, return_val, PR_FALSE));
}

NS_IMETHODIMP nsPref::GetBinaryPref(const char *pref, 
                        void * return_val, int * buf_length)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_GetBinaryPref(pref, return_val, buf_length, PR_FALSE));
}


NS_IMETHODIMP nsPref::GetColorPrefDWord(const char *pref, 
                    PRUint32 *colorref)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_GetColorPrefDWord(pref, colorref, PR_FALSE));
}


/*
 * Setters
 */

NS_IMETHODIMP nsPref::SetCharPref(const char *pref,const char* value)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_SetCharPref(pref, value));
}

NS_IMETHODIMP nsPref::SetUnicharPref(const char *pref, const PRUnichar *value)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return SetCharPref(pref, NS_ConvertUCS2toUTF8(value));
}

NS_IMETHODIMP nsPref::SetIntPref(const char *pref,PRInt32 value)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_SetIntPref(pref, value));
}

NS_IMETHODIMP nsPref::SetBoolPref(const char *pref,PRBool value)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_SetBoolPref(pref, value));
}

NS_IMETHODIMP nsPref::SetBinaryPref(const char *pref,void * value, PRUint32 size)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_SetBinaryPref(pref, value, size));
}


/*
 * Get Defaults
 */

NS_IMETHODIMP nsPref::GetDefaultIntPref(const char *pref,
                    PRInt32 * return_int)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_GetIntPref(pref, return_int, PR_TRUE));
}

NS_IMETHODIMP nsPref::GetDefaultBoolPref(const char *pref,
                     PRBool * return_val)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_GetBoolPref(pref, return_val, PR_TRUE));
}

NS_IMETHODIMP nsPref::GetDefaultBinaryPref(const char *pref, 
                         void * return_val,
                         int * buf_length)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_GetBinaryPref(pref, return_val, buf_length, PR_TRUE));
}


/*
 * Set defaults
 */

NS_IMETHODIMP nsPref::SetDefaultCharPref(const char *pref,const char* value)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_SetDefaultCharPref(pref, value));
}

NS_IMETHODIMP nsPref::SetDefaultUnicharPref(const char *pref,
                                            const PRUnichar *value)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return SetDefaultCharPref(pref, NS_ConvertUCS2toUTF8(value));
}

NS_IMETHODIMP nsPref::SetDefaultIntPref(const char *pref,PRInt32 value)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_SetDefaultIntPref(pref, value));
}

NS_IMETHODIMP nsPref::SetDefaultBoolPref(const char *pref, PRBool value)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_SetDefaultBoolPref(pref, value));
}

NS_IMETHODIMP nsPref::SetDefaultBinaryPref(const char *pref,
                         void * value, PRUint32 size)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_SetDefaultBinaryPref(pref, value, size));
}


NS_IMETHODIMP nsPref::ClearUserPref(const char *pref_name)
{
    if (NS_FAILED(SecurePrefCheck(pref_name))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_ClearUserPref(pref_name));
}

/*
 * Copy prefs
 */
#if defined(DEBUG_tao_)
//3456789012345678901234567890123456789012 34567890123456789012345678901234567890
static const char strArr[][64] = {
    "browser.startup.homepage",
    "browser.throbber.url",
    "startup.homepage_override_url",
    NULL};

static void checkPref(const char* fname, const char* pref) {
    int i=0;
    nsCString cstr(strArr[i]);
    while (cstr.Length()) { 
        if (pref == cstr) {
            printf("\n --> %s:: SHALL use GetLocalizedUnicharPrefto get --%s--\n", fname, pref); 
            NS_ASSERTION(0, "\n\n");
            return;
        }
        cstr = strArr[++i];
    }
}
#endif

NS_IMETHODIMP nsPref::CopyCharPref(const char *pref, char ** return_buf)
{
#if defined(DEBUG_tao_)
    checkPref("CopyCharPref", pref);
#endif
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_CopyCharPref(pref, return_buf, PR_FALSE));
}

// unicode "%s" format string
static const PRUnichar unicodeFormatter[] = {
    (PRUnichar)'%',
    (PRUnichar)'s',
    (PRUnichar)0,
};

NS_IMETHODIMP nsPref::CopyUnicharPref(const char *pref, PRUnichar ** return_buf)
{
#if defined(DEBUG_tao_)
    checkPref("CopyUnicharPref", pref);
#endif
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    nsresult rv;
    
    // get the UTF8 string for conversion
    nsXPIDLCString utf8String;
    rv = CopyCharPref(pref, getter_Copies(utf8String));
    if (NS_FAILED(rv)) return rv;

    return convertUTF8ToUnicode(utf8String, return_buf);
}

NS_IMETHODIMP
nsPref::GetLocalizedUnicharPref(const char *pref, PRUnichar **return_buf)
{
    nsresult rv;

#if defined(DEBUG_tao_)
    printf("\n --> nsPref::GetLocalizedUnicharPref(%s) --", pref);
#endif
    // if the user has set this pref, then just return the user value
    if (PREF_HasUserPref(pref))
        return CopyUnicharPref(pref, return_buf);

    // user has not set the pref, so the default value
    // contains a URL to a .properties file
    
    nsXPIDLCString propertyFileURL;
    rv = CopyCharPref(pref, getter_Copies(propertyFileURL));
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIStringBundleService> bundleService =
        do_GetService(kStringBundleServiceCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStringBundle> bundle;
    rv = bundleService->CreateBundle(propertyFileURL, nsnull,
                                     getter_AddRefs(bundle));
    if (NS_FAILED(rv)) return rv;

    // string names are in unicdoe
    nsAutoString stringId;
    stringId.AssignWithConversion(pref);
    
    return bundle->GetStringFromName(stringId.GetUnicode(), return_buf);
}

nsresult
nsPref::convertUTF8ToUnicode(const char *utf8String, PRUnichar ** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    // convert to PRUnichar using nsTextFormatter
    // this is so ugly, it allocates memory at least 4 times :(
    PRUnichar *unicodeString =
        nsTextFormatter::smprintf(unicodeFormatter, utf8String);
    if (!unicodeString) return NS_ERROR_OUT_OF_MEMORY;

    // use the right allocator
    *aResult = nsCRT::strdup(unicodeString);
    nsTextFormatter::smprintf_free(unicodeString);
    if (!*aResult) return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

NS_IMETHODIMP nsPref::CopyBinaryPref(const char *pref,
                         int *size, void ** return_value)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_CopyBinaryPref(pref, return_value, size, PR_FALSE));
}

NS_IMETHODIMP nsPref::CopyDefaultCharPref( const char *pref,
                         char ** return_buffer )
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_CopyCharPref(pref, return_buffer, PR_TRUE));
}

NS_IMETHODIMP nsPref::CopyDefaultUnicharPref( const char *pref,
                                              PRUnichar ** return_buf)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    nsresult rv;
    
    nsXPIDLCString utf8String;
    rv = CopyDefaultCharPref(pref, getter_Copies(utf8String));
    if (NS_FAILED(rv)) return rv;
    
    return convertUTF8ToUnicode(utf8String, return_buf);
}

NS_IMETHODIMP nsPref::CopyDefaultBinaryPref(const char *pref, 
                            int * size, void ** return_val)
{
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;
    return _convertRes(PREF_CopyBinaryPref(pref, return_val, size, PR_TRUE));
}


//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::GetFilePref(const char *pref_name, nsIFileSpec** value)
//----------------------------------------------------------------------------------------
{
    if (!value)
        return NS_ERROR_NULL_POINTER;   
    if (NS_FAILED(SecurePrefCheck(pref_name))) return NS_ERROR_FAILURE;

        nsresult rv = nsComponentManager::CreateInstance(
        	(const char*)NS_FILESPEC_PROGID,
        	(nsISupports*)nsnull,
        	(const nsID&)NS_GET_IID(nsIFileSpec),
        	(void**)value);
        NS_ASSERTION(NS_SUCCEEDED(rv), "ERROR: Could not make a file spec.");
    if (!*value)
      return NS_ERROR_FAILURE;

    char *encodedString = nsnull;
    rv = CopyCharPref(pref_name, &encodedString);
    if (NS_FAILED(rv)) return rv;

	PRBool valid;
    (*value)->SetPersistentDescriptorString(encodedString);
    (*value)->IsValid(&valid);
    if (! valid)
    	/* if the ecodedString wasn't a valid persitent descriptor, it might be a valid native path*/
    	(*value)->SetNativePath(encodedString);
    
    PR_Free(encodedString); // Allocated by CopyCharPref
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::SetFilePref(const char *pref_name, 
                    nsIFileSpec* value, PRBool set_default)
//----------------------------------------------------------------------------------------
{
    if (!value)
        return NS_ERROR_NULL_POINTER;    
    if (NS_FAILED(SecurePrefCheck(pref_name))) return NS_ERROR_FAILURE;
    nsresult rv = NS_OK;
    if (!Exists(value))
    {
        // nsPersistentFileDescriptor requires an existing
        // object. Make it first. COM makes this difficult, of course...
	    nsIFileSpec* tmp = nsnull;
        rv = nsComponentManager::CreateInstance(
        	(const char*)NS_FILESPEC_PROGID,
        	(nsISupports*)nsnull,
        	(const nsID&)NS_GET_IID(nsIFileSpec),
        	(void**)&tmp);
        NS_ASSERTION(NS_SUCCEEDED(rv), "ERROR: Could not make a file spec.");
	    if (!tmp)
	    	return NS_ERROR_FAILURE;
		tmp->FromFileSpec(value);
        tmp->CreateDir();
        NS_RELEASE(tmp);
    }
    char* encodedString = nsnull;
    value->GetPersistentDescriptorString(&encodedString);
    if (encodedString && *encodedString)
    {
        if (set_default)
            rv = PREF_SetDefaultCharPref(pref_name, encodedString);
        else
            rv = PREF_SetCharPref(pref_name, encodedString);
    }
    PR_FREEIF(encodedString); // Allocated by nsOutputStringStream
    return _convertRes(rv);
}

NS_IMETHODIMP
nsPref::GetFileXPref(const char *aPref, nsILocalFile ** aResult)
{
    nsresult rv;
    nsCOMPtr<nsILocalFile> file = do_CreateInstance(NS_LOCAL_FILE_PROGID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsXPIDLCString descriptorString;
    rv = CopyCharPref(aPref, getter_Copies(descriptorString));
    if (NS_FAILED(rv)) return rv;

    rv = file->SetPersistentDescriptor(descriptorString);
    NS_ENSURE_SUCCESS(rv, rv);

    *aResult = file;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsPref::SetFileXPref(const char *aPref, nsILocalFile *aValue)
{
    nsresult rv;
    nsXPIDLCString descriptorString;

    rv = aValue->GetPersistentDescriptor(getter_Copies(descriptorString));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = SetCharPref(aPref, descriptorString);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

/*
 * Pref access without security check - these are here
 * to support nsScriptSecurityManager.
 * These functions are part of nsISecurityPref, not nsIPref.
 * **PLEASE** do not call these functions from elsewhere
 */
NS_IMETHODIMP nsPref::SecurityGetBoolPref(const char *pref, PRBool * return_val)
{
    return _convertRes(PREF_GetBoolPref(pref, return_val, PR_FALSE));
}

NS_IMETHODIMP nsPref::SecuritySetBoolPref(const char *pref, PRBool value)
{
    return _convertRes(PREF_SetBoolPref(pref, value));
}

NS_IMETHODIMP nsPref::SecurityGetCharPref(const char *pref, char ** return_buf)
{
#if defined(DEBUG_tao_)
    checkPref("CopyCharPref", pref);
#endif
    return _convertRes(PREF_CopyCharPref(pref, return_buf, PR_FALSE));
}

NS_IMETHODIMP nsPref::SecuritySetCharPref(const char *pref, const char* value)
{
    return _convertRes(PREF_SetCharPref(pref, value));
}

NS_IMETHODIMP nsPref::SecurityGetIntPref(const char *pref, PRInt32 * return_val)
{
    return _convertRes(PREF_GetIntPref(pref, return_val, PR_FALSE));
}

NS_IMETHODIMP nsPref::SecuritySetIntPref(const char *pref, PRInt32 value)
{
    return _convertRes(PREF_SetIntPref(pref, value));
}

NS_IMETHODIMP nsPref::SecurityClearUserPref(const char *pref_name)
{
    return _convertRes(PREF_ClearUserPref(pref_name));
}

/*
 * Pref info
 */

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::PrefIsLocked(const char *pref, PRBool *res)
//----------------------------------------------------------------------------------------
{
    if (res == NULL)
        return NS_ERROR_INVALID_POINTER;
    if (NS_FAILED(SecurePrefCheck(pref))) return NS_ERROR_FAILURE;

    *res = PREF_PrefIsLocked(pref);
    return NS_OK;
}

/*
 * Callbacks
 */

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::RegisterCallback( const char* domain,
                    PrefChangedFunc callback, 
                    void* instance_data )
//----------------------------------------------------------------------------------------
{
    PREF_RegisterCallback(domain, callback, instance_data);
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::UnregisterCallback( const char* domain,
                        PrefChangedFunc callback, 
                        void* instance_data )
//----------------------------------------------------------------------------------------
{
    return _convertRes(PREF_UnregisterCallback(domain, callback, instance_data));
}

/*
 * Tree editing
 */

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::DeleteBranch(const char *branchName)
//----------------------------------------------------------------------------------------
{
    return _convertRes(PREF_DeleteBranch(branchName));
}

#include "prprf.h"
/*
 * Creates an iterator over the children of a node.
 */
typedef struct 
{
	char*		 childList;
	char*		 parent;
	unsigned int bufsize;
} PrefChildIter; 

/* if entry begins with the given string, i.e. if string is
  "a"
  and entry is
  "a.b.c" or "a.b"
  then add "a.b" to the list. */
static PRIntn PR_CALLBACK
pref_addChild(PLHashEntry *he, int i, void *arg)
{
	PrefChildIter* pcs = (PrefChildIter*) arg;
    const char *pref = (const char*)he->key;
	if ( PL_strncmp(pref, pcs->parent, PL_strlen(pcs->parent)) == 0 )
	{
		char buf[512];
		char* nextdelim;
		PRUint32 parentlen = PL_strlen(pcs->parent);
		char* substring;

		PL_strncpy(buf, (char*)he->key, PR_MIN(512, PL_strlen((char*)he->key) + 1));
		nextdelim = buf + parentlen;
		if (parentlen < PL_strlen(buf))
		{
			/* Find the next delimiter if any and truncate the string there */
			nextdelim = PL_strstr(nextdelim, ".");
			if (nextdelim)
			{
				*nextdelim = ';';
				*(nextdelim + 1) = '\0';
			}
		}

		substring = PL_strstr(pcs->childList, buf);
		if (!substring)
		{
			unsigned int newsize = PL_strlen(pcs->childList) + PL_strlen(buf) + 2;
			if (newsize > pcs->bufsize)
			{
				pcs->bufsize *= 3;
				pcs->childList = (char*) realloc(pcs->childList, sizeof(char) * pcs->bufsize);
				if (!pcs->childList)
					return HT_ENUMERATE_STOP;
			}
			PL_strcat(pcs->childList, buf);
		}
	}
	return 0;
}

#ifdef XP_OS2_VACPP
/* See comment in xpcom/ds/nsHashtable.cpp. */
#define pref_addChild (PRIntn(*_Optlink)(PLHashEntry*,PRIntn,void *))(pref_addChild)
#endif

NS_IMETHODIMP nsPref::CreateChildList(const char* parent_node, char **child_list)
{
	PrefChildIter pcs;

	pcs.bufsize = 2048;
	pcs.childList = (char*) malloc(sizeof(char) * pcs.bufsize);
	if (*parent_node > 0)
		pcs.parent = PR_smprintf("%s.", parent_node);
	else
		pcs.parent = PL_strdup("");
	if (!pcs.parent || !pcs.childList)
		return PREF_OUT_OF_MEMORY;
	pcs.childList[0] = '\0';

	PL_HashTableEnumerateEntries(gHashTable, pref_addChild, &pcs);

	*child_list = pcs.childList;
	PR_Free(pcs.parent);
	
	return (pcs.childList == NULL) ? PREF_OUT_OF_MEMORY : PREF_OK;
}

NS_IMETHODIMP nsPref::NextChild(const char *child_list, PRInt16 *indx, char **listchild)
{
	char* temp = (char*)&child_list[*indx];
	char* child = strtok(temp, ";");
	if (child)
	{
		*indx += PL_strlen(child) + 1;
		*listchild = child;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

struct EnumerateData {
    const char *parent;
    nsVoidArray *pref_list;
};

PR_STATIC_CALLBACK(PRIntn)
pref_enumChild(PLHashEntry *he, int i, void *arg)
{
    EnumerateData *d = (EnumerateData *) arg;
    if (PL_strncmp((char*)he->key, d->parent, PL_strlen(d->parent)) == 0) {
        d->pref_list->AppendElement((void *)he->key);
    }
    return HT_ENUMERATE_NEXT;
}

NS_IMETHODIMP
nsPref::EnumerateChildren(const char *parent, PrefEnumerationFunc callback, void *arg) 
{
    // this will contain a list of all the pref name strings
    // allocate on the stack for speed
    nsAutoVoidArray prefArray;

    EnumerateData ed;
    ed.parent = parent;
    ed.pref_list = &prefArray;
    PL_HashTableEnumerateEntries(gHashTable, pref_enumChild, &ed);

    // now that we've built up the list, run the callback on
    // all the matching elements
    PRInt32 numPrefs = prefArray.Count();
    PRInt32 i;
    for (i=0; i < numPrefs; i++) {
        char *prefName = (char *)prefArray.ElementAt(i);
        (*callback)((char*)prefName, arg);
    }
    
    return NS_OK;
}

//========================================================================================
// C++ implementations of old C routines
//========================================================================================

//----------------------------------------------------------------------------------------
PrefResult pref_OpenFileSpec(
    nsIFileSpec* fileSpec,
    PRBool is_error_fatal,
    PRBool verifyHash,
    PRBool bGlobalContext,
    PRBool skipFirstLine)
//----------------------------------------------------------------------------------------
{
    PrefResult result = PREF_NOERROR;
    char *return_error;

    if (NS_FAILED(fileSpec->ResolveSymlink()))
      return PREF_ERROR;

    if (!Exists(fileSpec))
        return PREF_ERROR;
    
    char* readBuf;
    if (NS_FAILED(fileSpec->GetFileContents(&readBuf)))
    	return PREF_ERROR;
    long fileLength = PL_strlen(readBuf);
    if (verifyHash && !pref_VerifyLockFileSpec(readBuf, fileLength)) 
    {
        result = PREF_BAD_LOCKFILE;
        if (is_error_fatal) 
        {
            return_error = PL_strdup("An error occurred reading the startup configuration file. Please contact your administrator.");
            if (return_error) 
            {
                pref_Alert(return_error);
                PR_Free(return_error);
            }
            PR_Free(readBuf);
            return result;
        }
    }
    if (result == PREF_NOERROR)
    {
        if (!PREF_EvaluateConfigScript(readBuf, fileLength,
                nsnull, bGlobalContext, PR_TRUE, skipFirstLine))
        {
            result = PREF_ERROR;
            if (verifyHash) 
            {
                PR_Free(readBuf);
                return result;
            }
        }
    }
    PR_Free(readBuf);

    // If the user prefs file exists but generates an error,
    //   don't clobber the file when we try to save it
    if ((!readBuf || result != PREF_NOERROR) && is_error_fatal)
        gErrorOpeningUserPrefs = PR_TRUE;
#if defined(XP_PC) && !defined(XP_OS2)
    if (gErrorOpeningUserPrefs && is_error_fatal)
        MessageBox(nsnull,"Error in preference file (prefs.js).  Default preferences will be used.","Netscape - Warning", MB_OK);
#endif

    JS_GC(gMochaContext);
    return result;
} // pref_OpenFile

//----------------------------------------------------------------------------------------
// Computes the MD5 hash of the given buffer (not including the first line)
//   and verifies the first line of the buffer expresses the correct hash in the form:
//   // xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx
//   where each 'xx' is a hex value. */
//----------------------------------------------------------------------------------------
PRBool pref_VerifyLockFileSpec(char* buf, long buflen)
//----------------------------------------------------------------------------------------
{

    PRBool success = PR_FALSE;
	const int obscure_value = 7;
	const long hash_length = 51;		/* len = 48 chars of MD5 + // + EOL */
    unsigned char *digest;
    char szHash[64];
   
	/* Unobscure file by subtracting some value from every char. */
	long i;
	for (i = 0; i < buflen; i++)
		buf[i] -= obscure_value;

    if (buflen >= hash_length)
    {
        const unsigned char magic_key[] = "VonGloda5652TX75235ISBN";
   	    unsigned char *pStart = (unsigned char*) buf + hash_length;
	    unsigned int len;
       
        nsresult rv;

        NS_WITH_SERVICE(nsISignatureVerifier, verifier, SIGNATURE_VERIFIER_PROGID, &rv);
        if (NS_FAILED(rv)) return success; // No signature verifier available
       
        //-- Calculate the digest
        PRUint32 id;
        rv = verifier->HashBegin(nsISignatureVerifier::MD5, &id);
        if (NS_FAILED(rv)) return success;

        rv = verifier->HashUpdate(id, (const char*)magic_key, sizeof(magic_key));
        if (NS_FAILED(rv)) return success;

        rv = verifier->HashUpdate(id, (const char*)pStart, (unsigned int)(buflen - hash_length));
        if (NS_FAILED(rv)) return success;
  
        digest = (unsigned char*)PR_MALLOC(nsISignatureVerifier::MD5_LENGTH);
        if (digest == nsnull) return success;

        rv = verifier->HashEnd(id,&digest, &len, nsISignatureVerifier::MD5_LENGTH);
        if (NS_FAILED(rv)) { PR_FREEIF(digest); return success; }
            
	
        
	    PR_snprintf(szHash, 64, "%x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
	        (int)digest[0],(int)digest[1],(int)digest[2],(int)digest[3],
	        (int)digest[4],(int)digest[5],(int)digest[6],(int)digest[7],
	        (int)digest[8],(int)digest[9],(int)digest[10],(int)digest[11],
	        (int)digest[12],(int)digest[13],(int)digest[14],(int)digest[15]);

        printf("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
	        (int)digest[0],(int)digest[1],(int)digest[2],(int)digest[3],
	        (int)digest[4],(int)digest[5],(int)digest[6],(int)digest[7],
	        (int)digest[8],(int)digest[9],(int)digest[10],(int)digest[11],
	        (int)digest[12],(int)digest[13],(int)digest[14],(int)digest[15]); 

   
		success = ( PL_strncmp((const char*) buf + 3, szHash, (PRUint32)(hash_length - 4)) == 0 );

        PR_FREEIF(digest);
 
	}
    return success;

}

//----------------------------------------------------------------------------------------
PrefResult PREF_SavePrefFileSpecWith(
    nsIFileSpec* fileSpec,
    PLHashEnumerator heSaveProc)
//----------------------------------------------------------------------------------------
{
    if (!gHashTable)
        return PREF_NOT_INITIALIZED;

    /* ?! Don't save (blank) user prefs if there was an error reading them */
    if (gErrorOpeningUserPrefs)
        return PREF_NOERROR;

    char** valueArray = (char**) PR_Calloc(sizeof(char*), gHashTable->nentries);
    if (!valueArray)
        return PREF_OUT_OF_MEMORY;

    nsOutputFileStream stream(fileSpec);
    if (!stream.is_open())
        return PREF_ERROR;

    stream << PREFS_HEADER_LINE_1 << nsEndl << PREFS_HEADER_LINE_2 << nsEndl << nsEndl;
    
    /* LI_STUFF here we pass in the heSaveProc proc used so that li can do its own thing */
    PR_HashTableEnumerateEntries(gHashTable, heSaveProc, valueArray);
    
    /* Sort the preferences to make a readable file on disk */
    NS_QuickSort(valueArray, gHashTable->nentries, sizeof(char*), pref_CompareStrings, NULL);
    char** walker = valueArray;
    for (PRUint32 valueIdx = 0; valueIdx < gHashTable->nentries; valueIdx++,walker++)
    {
        if (*walker)
        {
            stream << *walker << nsEndl;
            PR_Free(*walker);
        }
    }
    PR_Free(valueArray);
	fileSpec->CloseStream();
    return PREF_NOERROR;
}

/// Note: inplaceSortCallback is a small C callback stub for NS_QuickSort
static int PR_CALLBACK
inplaceSortCallback(const void *data1, const void *data2, void *privateData)
{
	char *name1 = nsnull;
	char *name2 = nsnull;
	nsIFileSpec *file1= *(nsIFileSpec **)data1;
	nsIFileSpec *file2= *(nsIFileSpec **)data2;
	nsresult rv;
	int sortResult = 0;

	rv = file1->GetLeafName(&name1);
	NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get the leaf name");
	if (NS_SUCCEEDED(rv)) {
		rv = file2->GetLeafName(&name2);
		NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get the leaf name");
		if (NS_SUCCEEDED(rv)) {
			if (name1 && name2) {
				// we want it so foo.js will come before foo-<bar>.js
				// "foo." is before "foo-", so we have to reverse the order to accomplish
				sortResult = PL_strcmp(name2,name1);
			}
			if (name1) nsCRT::free((char*)name1);
			if (name2) nsCRT::free((char*)name2);
		}
	}
	return sortResult;
}

//----------------------------------------------------------------------------------------
extern "C" JSBool pref_InitInitialObjects()
// Initialize default preference JavaScript buffers from
// appropriate TEXT resources
//----------------------------------------------------------------------------------------
{
    JSBool funcResult;
    nsresult rv;
    PRBool exists;
    nsCOMPtr<nsIFile> aFile;
    nsCOMPtr <nsIFileSpec> defaultPrefDir;
        
    rv = NS_GetSpecialDirectory(NS_APP_PREF_DEFAULTS_50_DIR, getter_AddRefs(aFile));
    if (NS_FAILED(rv))
    	return JS_FALSE;
    rv = _nsIFileToFileSpec(aFile, getter_AddRefs(defaultPrefDir));
    if (NS_FAILED(rv))
    	return JS_FALSE;
    
	static const char* specialFiles[] = {
		"initpref.js"
#ifdef XP_MAC
	,	"macprefs.js"
#elif defined(XP_OS2)
   ,	"os2pref.js"
#elif defined(XP_PC)
	,	"winpref.js"
#elif defined(XP_UNIX)
	,	"unix.js"
#endif
	};
    
	int k=0;
	JSBool worked = JS_FALSE;
 	nsIFileSpec ** defaultPrefFiles = new nsIFileSpec*[INITIAL_MAX_DEFAULT_PREF_FILES];
	int maxDefaultPrefFiles = INITIAL_MAX_DEFAULT_PREF_FILES;
	int numFiles = 0;

	// Parse all the random files that happen to be in the components directory.
    nsCOMPtr<nsIDirectoryIterator> dirIterator;
    rv = nsComponentManager::CreateInstance(
        	(const char*)NS_DIRECTORYITERATOR_PROGID,
        	(nsISupports*)nsnull,
        	(const nsID&)NS_GET_IID(nsIDirectoryIterator),
        	getter_AddRefs(dirIterator));
    NS_ASSERTION(NS_SUCCEEDED(rv), "ERROR: Could not make a directory iterator.");
    if (!dirIterator || NS_FAILED(dirIterator->Init(defaultPrefDir, PR_TRUE)))
    	return JS_FALSE;

	// Get any old child of the components directory. Warning: aliases get resolved, so
	// SetLeafName will not work here.
    nsCOMPtr<nsIFile> aFile2;
    nsCOMPtr <nsIFileSpec> specialChild;
        
    rv = NS_GetSpecialDirectory(NS_APP_PREF_DEFAULTS_50_DIR, getter_AddRefs(aFile2));
    if (NS_FAILED(rv))
    	return JS_TRUE;

    // TODO: Convert the rest of this code to nsIFile
    // and avoid this conversion to nsIFileSpec
    rv = _nsIFileToFileSpec(aFile2, getter_AddRefs(specialChild));   
    if (NS_FAILED(rv))
    	return JS_TRUE;
    	
	if NS_FAILED(specialChild->AppendRelativeUnixPath((char*)specialFiles[0]))
	{
		funcResult = JS_FALSE;
		goto done;
	}
	
    if (NS_FAILED(specialChild->Exists(&exists)))
	{
		funcResult = JS_FALSE;
		goto done;
	}

    if (exists)
    {
        worked = (JSBool)(pref_OpenFileSpec(
    	    specialChild,
    	    PR_FALSE,
    	    PR_FALSE,
    	    PR_FALSE,
    	    PR_FALSE) == PREF_NOERROR);
	    NS_ASSERTION(worked, "initpref.js not parsed successfully");
    }
    // Keep this child

#ifdef DEBUG_prefs
    printf("Parsing default JS files.\n");
#endif /* DEBUG_prefs */
	for (; Exists(dirIterator); dirIterator->Next())
	{
		nsCOMPtr<nsIFileSpec> child;
		PRBool shouldParse = PR_TRUE;
		if NS_FAILED(dirIterator->GetCurrentSpec(getter_AddRefs(child)))
			continue;
		char* leafName;
		rv = child->GetLeafName(&leafName);
		if (NS_SUCCEEDED(rv))
        {
		    // Skip non-js files
		    if (PL_strstr(leafName, ".js") + PL_strlen(".js") != leafName + PL_strlen(leafName))
			    shouldParse = PR_FALSE;
		    // Skip files in the special list.
		    if (shouldParse)
		    {
			    for (int j = 0; j < (int) (sizeof(specialFiles) / sizeof(char*)); j++)
				    if (PL_strcmp(leafName, specialFiles[j]) == 0)
					    shouldParse = PR_FALSE;
		    }
		    if (shouldParse)
		    {
    #ifdef DEBUG_prefs
                printf("Adding %s to the list to be sorted\n", leafName);
    #endif /* DEBUG_prefs */
			    rv = NS_NewFileSpec(&(defaultPrefFiles[numFiles]));
			    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to create a file spec");
			    if (NS_SUCCEEDED(rv) && defaultPrefFiles[numFiles]) {
				    rv = defaultPrefFiles[numFiles]->FromFileSpec(child);
				    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to set the spec");
				    if (NS_SUCCEEDED(rv)) {
					    numFiles++;
				    }
				    if (numFiles == maxDefaultPrefFiles) {
					    // double the size of the array
					    nsIFileSpec **newArray;
					    newArray = new nsIFileSpec*[maxDefaultPrefFiles * 2];
					    nsCRT::memcpy(newArray,defaultPrefFiles,maxDefaultPrefFiles*sizeof(nsIFileSpec *));
					    maxDefaultPrefFiles *= 2;
					    delete [] defaultPrefFiles;
					    defaultPrefFiles = newArray; 
				    }
			    }
		    }
		    if (leafName) nsCRT::free((char*)leafName);
	    }
    }
#ifdef DEBUG_prefs
	printf("Sort defaultPrefFiles.  we need them sorted so all-ns.js will override all.js (where override == parsed later)\n");
#endif /* DEBUG_prefs */
	NS_QuickSort((void *)defaultPrefFiles, numFiles,sizeof(nsIFileSpec *), inplaceSortCallback, nsnull);

	for (k=0;k<numFiles;k++) {
		char* currentLeafName = nsnull;
		if (defaultPrefFiles[k]) {
				rv = defaultPrefFiles[k]->GetLeafName(&currentLeafName);
#ifdef DEBUG_prefs
				printf("Parsing %s\n", currentLeafName);
#endif /* DEBUG_prefs */
				if (currentLeafName) nsCRT::free((char*)currentLeafName);

				if (NS_SUCCEEDED(rv)) {
					worked = (JSBool)(pref_OpenFileSpec(
						defaultPrefFiles[k],
						PR_FALSE,
						PR_FALSE,
						PR_FALSE,
						PR_FALSE) == PREF_NOERROR);
					NS_ASSERTION(worked, "Config file not parsed successfully");
				}
		}
	}
	for (k=0;k<numFiles;k++) {
		NS_IF_RELEASE(defaultPrefFiles[k]);
	}
	delete [] defaultPrefFiles;
	defaultPrefFiles = nsnull;

#ifdef DEBUG_prefs
            printf("Parsing platform-specific JS files.\n");
#endif /* DEBUG_prefs */
	// Finally, parse any other special files (platform-specific ones).
	for (k = 1; k < (int) (sizeof(specialFiles) / sizeof(char*)); k++)
	{
	     nsCOMPtr<nsIFile> aFile3;
        nsCOMPtr<nsIFileSpec> anotherSpecialChild2;
                 
        rv = NS_GetSpecialDirectory(NS_APP_PREF_DEFAULTS_50_DIR, getter_AddRefs(aFile3));
        if (NS_FAILED(rv)) continue;
        rv = aFile3->Append((char*)specialFiles[k]);
        if (NS_FAILED(rv)) continue;
    
        // TODO: Convert the rest of this code to nsIFile
        // and avoid this conversion to nsIFileSpec
        rv = _nsIFileToFileSpec(aFile3, getter_AddRefs(anotherSpecialChild2));
        if (NS_FAILED(rv)) continue;        

#ifdef DEBUG_prefs
            printf("Parsing %s\n", specialFiles[k]);
#endif /* DEBUG_prefs */

        if (NS_FAILED(anotherSpecialChild2->Exists(&exists)) || !exists)
	    	continue;

	    worked = (JSBool)(pref_OpenFileSpec(
    		anotherSpecialChild2,
	    	PR_FALSE,
	    	PR_FALSE,
	    	PR_FALSE,
	    	PR_FALSE) == PREF_NOERROR);
		NS_ASSERTION(worked, "<platform>.js was not parsed successfully");
	}
done:
    return JS_TRUE;
}

//----------------------------------------------------------------------------------------
// Module implementation for the pref library
class nsPrefModule : public nsIModule
{
public:
    nsPrefModule();
    virtual ~nsPrefModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mFactory;
};


//----------------------------------------------------------------------------------------
// Functions used to create new instances of a given object by the
// generic factory.

static NS_IMETHODIMP
CreateNewPref(nsISupports *aDelegate, REFNSIID aIID, void **aResult)
{
    if (aDelegate != NULL)
        return NS_ERROR_NO_AGGREGATION;

    nsPref *t = nsPref::GetInstance();
    
    if (t == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    
    nsresult res = t->QueryInterface(aIID, aResult);
    
    if (NS_FAILED(res))
        *aResult = NULL;

    return res;
}

// The list of components we register
static nsModuleComponentInfo components[] = {
    { NS_PREF_CLASSNAME, NS_PREF_CID, NS_PREF_PROGID, CreateNewPref, },
};

extern "C" JSRuntime* PREF_GetJSRuntime()
{
    nsresult rv;

    NS_WITH_SERVICE(nsIJSRuntimeService, rtsvc, "nsJSRuntimeService", &rv);
    if (NS_FAILED(rv)) return nsnull;

    JSRuntime* rt;
    rv = rtsvc->GetRuntime(&rt);
    if (NS_FAILED(rv)) return nsnull;

    return rt;
}

////////////////////////////////////////////////////////////////////////
// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
//
NS_IMPL_NSGETMODULE("nsPrefModule", components)
