/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.    You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.    Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.    All Rights
 * Reserved.
 */

#include "nsIPref.h"

#include "pratom.h"
#include "prefapi.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#ifdef XP_MAC
#include "nsINetSupport.h"
#include "nsIStreamListener.h"
#endif /* XP_MAC */
#include "nsIServiceManager.h"
#include "nsIFileLocator.h"
#include "nsCOMPtr.h"
#include "nsFileSpec.h"
#include "nsFileLocations.h"
#include "nsFileStream.h"
#include "nsIProfile.h"

#include "plhash.h"
#include "prmem.h"
#include "plstr.h"
#include "prprf.h"

#include "jsapi.h"

#ifdef _WIN32
#include "windows.h"
#endif /* _WIN32 */

#define XP_QSORT qsort

#define PREFS_HEADER_LINE_1 "// Mozilla User Preferences"
#define PREFS_HEADER_LINE_2	"// This is a generated file!"

#include "prefapi_private_data.h"

static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_IID(kIFileLocatorIID,      NS_IFILELOCATOR_IID);
static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kFileLocatorCID,       NS_FILELOCATOR_CID);

//========================================================================================
class nsPref: public nsIPref
//========================================================================================
{
    NS_DECL_ISUPPORTS

public:
    static nsPref *GetInstance();

#ifdef PREF_SUPPORT_OLD_PATH_STRINGS
    NS_IMETHOD ReadUserJSFile(const char *filename); // deprecated
    NS_IMETHOD ReadLIJSFile(const char *filename); // deprecated
    NS_IMETHOD EvaluateConfigScript(const char * js_buffer, PRUint32 length,
                    const char* filename, 
                    PRBool bGlobalContext, 
                    PRBool bCallbacks); // deprecated
    NS_IMETHOD SavePrefFileAs(const char *filename);
    NS_IMETHOD SaveLIPrefFile(const char *filename);

    // Path prefs
    NS_IMETHOD CopyPathPref(const char *pref, char ** return_buf);
    NS_IMETHOD SetPathPref(const char *pref, 
             const char *path, PRBool set_default);
#endif
    // Initialize/shutdown
    NS_IMETHOD StartUp();
    NS_IMETHOD StartUpWith(nsFileSpec* inSpec);
    NS_IMETHOD ShutDown();

    // Config file input
    NS_IMETHOD ReadUserJSFile(nsFileSpec* inSpec);
    NS_IMETHOD ReadLIJSFile(nsFileSpec* inSpec);

    NS_IMETHOD EvaluateConfigScript(const char * js_buffer, PRUint32 length,
                    PRBool bGlobalContext, 
                    PRBool bCallbacks);
    NS_IMETHOD EvaluateConfigScriptFile(const char * js_buffer, PRUint32 length,
                    nsFileSpec* inSpec, 
                    PRBool bGlobalContext, 
                    PRBool bCallbacks);

    NS_IMETHOD SavePrefFileAs(nsFileSpec* inSpec);
    NS_IMETHOD SaveLIPrefFile(nsFileSpec* inSpec);

    // JS stuff
    NS_IMETHOD GetConfigContext(JSContext **js_context);
    NS_IMETHOD GetGlobalConfigObject(JSObject **js_object);
    NS_IMETHOD GetPrefConfigObject(JSObject **js_object);

    // Getters
    NS_IMETHOD GetIntPref(const char *pref, PRInt32 * return_int);    
    NS_IMETHOD GetBoolPref(const char *pref, PRBool * return_val);    
    NS_IMETHOD GetBinaryPref(const char *pref, 
                 void * return_val, PRInt32 * buf_length);    
    NS_IMETHOD GetColorPref(const char *pref,
                uint8 *red, uint8 *green, uint8 *blue);
    NS_IMETHOD GetColorPrefDWord(const char *pref, PRUint32 *colorref);
    NS_IMETHOD GetRectPref(const char *pref, 
             PRInt16 *left, PRInt16 *top, 
             PRInt16 *right, PRInt16 *bottom);

    // Setters
    NS_IMETHOD SetCharPref(const char *pref,const char* value);
    NS_IMETHOD SetIntPref(const char *pref,PRInt32 value);
    NS_IMETHOD SetBoolPref(const char *pref,PRBool value);
    NS_IMETHOD SetBinaryPref(const char *pref,void * value, PRUint32 size);
    NS_IMETHOD SetColorPref(const char *pref, 
                uint8 red, uint8 green, uint8 blue);
    NS_IMETHOD SetColorPrefDWord(const char *pref, PRUint32 colorref);
    NS_IMETHOD SetRectPref(const char *pref, 
             PRInt16 left, PRInt16 top, PRInt16 right, PRInt16 bottom);
    
    NS_IMETHOD ClearUserPref(const char *pref);

    // Get Defaults
    NS_IMETHOD GetDefaultIntPref(const char *pref, PRInt32 * return_int);
    NS_IMETHOD GetDefaultBoolPref(const char *pref, PRBool * return_val);
    NS_IMETHOD GetDefaultBinaryPref(const char *pref, 
                    void * return_val, int * buf_length);
    NS_IMETHOD GetDefaultColorPref(const char *pref, 
                 PRUint8 *red, PRUint8 *green, PRUint8 *blue);
    NS_IMETHOD GetDefaultColorPrefDWord(const char *pref, 
                            PRUint32 *colorref);
    NS_IMETHOD GetDefaultRectPref(const char *pref, 
                PRInt16 *left, PRInt16 *top, 
                PRInt16 *right, PRInt16 *bottom);

    // Set defaults
    NS_IMETHOD SetDefaultCharPref(const char *pref,const char* value);
    NS_IMETHOD SetDefaultIntPref(const char *pref,PRInt32 value);
    NS_IMETHOD SetDefaultBoolPref(const char *pref,PRBool value);
    NS_IMETHOD SetDefaultBinaryPref(const char *pref,
                    void * value, PRUint32 size);
    NS_IMETHOD SetDefaultColorPref(const char *pref, 
                 PRUint8 red, PRUint8 green, PRUint8 blue);
    NS_IMETHOD SetDefaultRectPref(const char *pref, 
                PRInt16 left, PRInt16 top, 
                PRInt16 right, PRInt16 bottom);
    
    // Copy prefs
    NS_IMETHOD CopyCharPref(const char *pref, char ** return_buf);
    NS_IMETHOD CopyBinaryPref(const char *pref,
                    int *size, void ** return_value);

    NS_IMETHOD CopyDefaultCharPref( const char 
                    *pref,    char ** return_buffer );
    NS_IMETHOD CopyDefaultBinaryPref(const char *pref, 
                     int * size, void ** return_val);

    NS_IMETHOD GetFilePref(const char* pref, nsFileSpec** value);
    NS_IMETHOD SetFilePref(const char* pref, nsFileSpec* value, PRBool setDefault);
     
    // Pref info
    NS_IMETHOD PrefIsLocked(const char *pref, PRBool *res);

    // Save pref files
    NS_IMETHOD SavePrefFile();

    // Callbacks
    NS_IMETHOD RegisterCallback( const char* domain,
                         PrefChangedFunc callback, 
                         void* instance_data );
    NS_IMETHOD UnregisterCallback( const char* domain,
                 PrefChangedFunc callback, 
                 void* instance_data );
    NS_IMETHOD CopyPrefsTree(const char *srcRoot, const char *destRoot);
    NS_IMETHOD DeleteBranch(const char *branchName);

protected:

    nsPref();
    virtual ~nsPref();

    void useDefaultPrefFile();
    static nsPref *gInstance;
    
    nsFileSpec                    mFileSpec;
    nsFileSpec                    mLIFileSpec;
}; // class nsPref

nsPref* nsPref::gInstance = NULL;

static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

static PrefResult pref_OpenFileSpec(
    const nsFileSpec& fileSpec,
    PRBool is_error_fatal,
    PRBool verifyHash,
    PRBool bGlobalContext,
    PRBool skipFirstLine);

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
            return NS_ERROR_UNEXPECTED;
        case PREF_VALUECHANGED:
            return NS_PREF_VALUE_CHANGED;
    };
    return NS_OK;
}

//----------------------------------------------------------------------------------------
nsPref::nsPref()
//----------------------------------------------------------------------------------------
{
    PR_AtomicIncrement(&g_InstanceCount);
    NS_INIT_REFCNT();
}

//----------------------------------------------------------------------------------------
nsPref::~nsPref()
//----------------------------------------------------------------------------------------
{
    ShutDown();
    PR_AtomicDecrement(&g_InstanceCount);
    gInstance = NULL;
}

//----------------------------------------------------------------------------------------
void nsPref::useDefaultPrefFile()
//----------------------------------------------------------------------------------------
{
    nsFileSpec prefsFile("default_prefs.js"); // in default working directory.
    nsresult rv;
    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_SUCCEEDED(rv) && locator)
    {
	    rv = locator->GetFileLocation(nsSpecialFileSpec::App_PreferencesFile50, &prefsFile);
	    NS_ASSERTION(NS_SUCCEEDED(rv), "ERROR: File locator cannot locate prefs file.");
    }
    if (prefsFile.Exists())
    {
        rv = StartUpWith(&prefsFile);
    }
    else
    {
      // no prefs file.  make a stub of one
      nsOutputFileStream stream(prefsFile);
      if (stream.is_open())
      {
        stream << PREFS_HEADER_LINE_1 << nsEndl << PREFS_HEADER_LINE_2 << nsEndl << nsEndl;
        stream.close();
      }
      
      if (prefsFile.Exists())
      {
        rv = StartUpWith(&prefsFile);

        // sspitzer:  eventually this code should be moved into the profile manager
        // that code should be setting up the prefs based on the what the user enters.
        NS_WITH_SERVICE(nsIProfile, profileService, kProfileCID, &rv);
        if (NS_SUCCEEDED(rv)) {
          char* currProfileName = nsnull;
          rv = profileService->GetCurrentProfile(&currProfileName);
          if (NS_SUCCEEDED(rv)) {
            char *imapDirStr;
            char *newsDirStr;
            char *emailStr;
            
            emailStr = PR_smprintf("%s@netscape.com",currProfileName);
#ifdef XP_UNIX
            imapDirStr = PR_smprintf("/u/%s/ImapMail", currProfileName);
            newsDirStr = PR_smprintf("/u/%s", currProfileName);
#else
#ifdef XP_MAC
            imapDirStr = PR_smprintf("HD:System Folder:Preferences:Netscape Users:%s:ImapMail", currProfileName);
            newsDirStr = PR_smprintf("HD:System Folder:Preferences:Netscape Users:%s:News", currProfileName);
#else
#ifdef XP_WIN
            imapDirStr = PR_smprintf("c:\\program files\\netscape\\users\\%s\\ImapMail",currProfileName);
            newsDirStr = PR_smprintf("c:\\program files\\netscape\\users\\%s\\News",currProfileName);
#else
#error you_need_to_edit_this_file_for_your_freak_os
#endif /* XP_WIN */
#endif /* XP_MAC */
#endif /* XP_UNIX */
            SetCharPref("browser.startup.homepage", "http://www.mozilla.org");
            SetCharPref("mail.accountmanager.accounts", "account0,account1");
            SetCharPref("mail.account.account0.identities", "id1");
            SetCharPref("mail.account.account0.server", "server0");
            SetCharPref("mail.account.account1.identities", "id1");
            SetCharPref("mail.account.account1.server", "server1");
            SetCharPref("mail.identity.id1.fullName", currProfileName);
            SetCharPref("mail.identity.id1.organization", "mozilla.org");
            SetCharPref("mail.identity.id1.smtp_name", currProfileName);
            SetCharPref("mail.identity.id1.smtp_server", "nsmail-2");
            SetCharPref("mail.identity.id1.useremail", emailStr);
            SetBoolPref("mail.identity.id1.send_html", PR_TRUE);
            SetIntPref("mail.identity.id1.wrap_column", 72);
            SetCharPref("mail.server.server0.directory", imapDirStr);
            SetCharPref("mail.server.server0.hostname", "nsmail-2");
            SetCharPref("mail.server.server0.password", "clear text password");
            SetCharPref("mail.server.server0.type", "imap");
            SetCharPref("mail.server.server0.userName", currProfileName);
            SetCharPref("mail.server.server1.directory", newsDirStr);
            SetCharPref("mail.server.server1.hostname", "news.mozilla.org");
            SetCharPref("mail.server.server1.type", "nntp");
            SetIntPref("news.max_articles",50);
            SetBoolPref("news.mark_old_read",PR_FALSE);
            PR_FREEIF(imapDirStr);
            PR_FREEIF(newsDirStr);
            PR_FREEIF(emailStr);
            
            // need to save the prefs now
            SavePrefFile();
          }
          if (currProfileName) PR_DELETE(currProfileName);
        }
      }
    }
    return;
} // nsPref::useDefaultPrefFile

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

NS_IMPL_ISUPPORTS(nsPref, kIPrefIID);




//========================================================================================
// nsIPref Implementation
//========================================================================================

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::StartUp()
//----------------------------------------------------------------------------------------
{
    useDefaultPrefFile();
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::StartUpWith(nsFileSpec* inFile)
//----------------------------------------------------------------------------------------
{
    if (mFileSpec == *inFile)
        return NS_OK;

    PRBool ok = PR_TRUE;

    /* --ML hash test */
    if (!gHashTable)
        gHashTable = PR_NewHashTable(2048, PR_HashString, PR_CompareStrings,
                         PR_CompareValues, &pref_HashAllocOps, NULL);
    if (!gHashTable)
        return PR_FALSE;

    mFileSpec = *inFile;

    if (!gMochaTaskState)
        gMochaTaskState = JS_Init((PRUint32) 0xffffffffL);

    if (!gMochaContext)
    {
        gMochaContext = JS_NewContext(gMochaTaskState, 8192);  /* ???? What size? */
        if (!gMochaContext)
            return PR_FALSE;

        JS_BeginRequest(gMochaContext);

        gGlobalConfigObject = JS_NewObject(gMochaContext, &global_class, NULL, NULL);
        if (!gGlobalConfigObject)
        {
            JS_EndRequest(gMochaContext);
            return PR_FALSE;
        }

        /* MLM - need a global object for set version call now. */
        JS_SetGlobalObject(gMochaContext, gGlobalConfigObject);

        JS_SetVersion(gMochaContext, JSVERSION_1_2);

        if (!JS_InitStandardClasses(gMochaContext, 
                        gGlobalConfigObject))
        {
            JS_EndRequest(gMochaContext);
            return PR_FALSE;
        }

        JS_SetBranchCallback(gMochaContext, pref_BranchCallback);
        JS_SetErrorReporter(gMochaContext, NULL);

        gMochaPrefObject = JS_DefineObject(gMochaContext, 
                            gGlobalConfigObject, 
                            "PrefConfig",
                            &autoconf_class, 
                            NULL, 
                            JSPROP_ENUMERATE|JSPROP_READONLY);
        
        if (gMochaPrefObject)
        {
            if (!JS_DefineProperties(gMochaContext,
                         gMochaPrefObject,
                         autoconf_props))
            {
                JS_EndRequest(gMochaContext);
                return PR_FALSE;
            }
            if (!JS_DefineFunctions(gMochaContext,
                        gMochaPrefObject,
                        autoconf_methods))
            {
                JS_EndRequest(gMochaContext);
                return PR_FALSE;
            }
        }

        ok = pref_InitInitialObjects();
    }
    else 
        JS_BeginRequest(gMochaContext);

    if (ok && mFileSpec.Exists())
        ok = (JSBool) (pref_OpenFileSpec(mFileSpec, PR_TRUE, PR_FALSE, PR_FALSE, PR_TRUE) == PREF_NOERROR);
    else if (!ok)
        gErrorOpeningUserPrefs = PR_TRUE;
    JS_EndRequest(gMochaContext);
    return ok;
} // nsPref::StartUpWith

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::ShutDown()
//----------------------------------------------------------------------------------------
{
    PREF_Cleanup();
    return NS_OK;
} // nsPref::ShutDown

/*
 * Config file input
 */

#ifdef PREF_SUPPORT_OLD_PATH_STRINGS
//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::ReadUserJSFile(const char *filename)
//----------------------------------------------------------------------------------------
{
    return _convertRes(PREF_ReadUserJSFile(filename));
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::ReadLIJSFile(const char *filename)
//----------------------------------------------------------------------------------------
{
    return _convertRes(PREF_ReadLIJSFile(filename));
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::EvaluateConfigScript(const char * js_buffer,
                         PRUint32 length,
                         const char* filename, 
                         PRBool bGlobalContext, 
                         PRBool bCallbacks)
//----------------------------------------------------------------------------------------
{
    return _convertRes(PREF_EvaluateConfigScript(js_buffer,
                                 length,
                                 filename,
                                 bGlobalContext,
                                 bCallbacks,
                                 PR_TRUE));
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::SavePrefFileAs(const char *filename)
//----------------------------------------------------------------------------------------
{
    return _convertRes(PREF_SavePrefFileAs(filename));
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::SaveLIPrefFile(const char *filename)
//----------------------------------------------------------------------------------------
{
    return _convertRes(PREF_SaveLIPrefFile(filename));
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::CopyPathPref(const char *pref_name, char ** return_buffer)
//----------------------------------------------------------------------------------------
{
    return _convertRes(PREF_CopyPathPref(pref_name, return_buffer));
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::SetPathPref(const char *pref_name, const char *path, PRBool set_default)
//----------------------------------------------------------------------------------------
{
    return _convertRes(PREF_SetPathPref(pref_name, path, set_default));
}

#endif /* PREF_SUPPORT_OLD_PATH_STRINGS */

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::ReadUserJSFile(nsFileSpec* fileSpec)
//----------------------------------------------------------------------------------------
{
    return pref_OpenFileSpec(*fileSpec, PR_FALSE, PR_FALSE, PR_TRUE, PR_FALSE);
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::ReadLIJSFile(nsFileSpec* fileSpec)
//----------------------------------------------------------------------------------------
{
    mLIFileSpec = *fileSpec;
    return pref_OpenFileSpec(*fileSpec, PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE);
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::EvaluateConfigScript(const char * js_buffer,
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
NS_IMETHODIMP nsPref::EvaluateConfigScriptFile(const char * js_buffer,
                         PRUint32 length,
                         nsFileSpec* fileSpec, 
                         PRBool bGlobalContext, 
                         PRBool bCallbacks)
//----------------------------------------------------------------------------------------
{
    return _convertRes(PREF_EvaluateConfigScript(js_buffer,
                                 length,
                                 fileSpec->GetCString(), // bad, but not used for parsing.
                                 bGlobalContext,
                                 bCallbacks,
                                 PR_TRUE));
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::SavePrefFileAs(nsFileSpec* fileSpec)
//----------------------------------------------------------------------------------------
{
    return _convertRes(PREF_SavePrefFileSpecWith(*fileSpec, (PLHashEnumerator)pref_savePref));
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::SaveLIPrefFile(nsFileSpec* fileSpec)
//----------------------------------------------------------------------------------------
{
    if (!gHashTable)
        return PREF_NOT_INITIALIZED;
    PREF_SetSpecialPrefsLocal();
    return _convertRes(PREF_SavePrefFileSpecWith(*fileSpec, (PLHashEnumerator)pref_saveLIPref));
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::SavePrefFile()
//----------------------------------------------------------------------------------------
{
    if (!gHashTable)
        return PREF_NOT_INITIALIZED;
#ifdef PREF_SUPPORT_OLD_PATH_STRINGS
    if (gFileName)
        return _convertRes(PREF_SavePrefFile());
#endif
    return _convertRes(PREF_SavePrefFileSpecWith(mFileSpec, (PLHashEnumerator)pref_savePref));
}

/*
 * JS stuff
 */

NS_IMETHODIMP nsPref::GetConfigContext(JSContext **js_context)
{
    return _convertRes(PREF_GetConfigContext(js_context));
}

NS_IMETHODIMP nsPref::GetGlobalConfigObject(JSObject **js_object)
{
    return _convertRes(PREF_GetGlobalConfigObject(js_object));
}

NS_IMETHODIMP nsPref::GetPrefConfigObject(JSObject **js_object)
{
    return _convertRes(PREF_GetPrefConfigObject(js_object));
}

/*
 * Getters
 */

NS_IMETHODIMP nsPref::GetIntPref(const char *pref, PRInt32 * return_int)
{
    return _convertRes(PREF_GetIntPref(pref, return_int));
}

NS_IMETHODIMP nsPref::GetBoolPref(const char *pref, PRBool * return_val)
{
    return _convertRes(PREF_GetBoolPref(pref, return_val));
}

NS_IMETHODIMP nsPref::GetBinaryPref(const char *pref, 
                        void * return_val, int * buf_length)
{
    return _convertRes(PREF_GetBinaryPref(pref, return_val, buf_length));
}

NS_IMETHODIMP nsPref::GetColorPref(const char *pref,
                     PRUint8 *red, PRUint8 *green, PRUint8 *blue)
{
    return _convertRes(PREF_GetColorPref(pref, red, green, blue));
}

NS_IMETHODIMP nsPref::GetColorPrefDWord(const char *pref, 
                    PRUint32 *colorref)
{
    return _convertRes(PREF_GetColorPrefDWord(pref, colorref));
}

NS_IMETHODIMP nsPref::GetRectPref(const char *pref, 
                    PRInt16 *left, PRInt16 *top, 
                    PRInt16 *right, PRInt16 *bottom)
{
    return _convertRes(PREF_GetRectPref(pref, left, top, right, bottom));
}

/*
 * Setters
 */

NS_IMETHODIMP nsPref::SetCharPref(const char *pref,const char* value)
{
    return _convertRes(PREF_SetCharPref(pref, value));
}

NS_IMETHODIMP nsPref::SetIntPref(const char *pref,PRInt32 value)
{
    return _convertRes(PREF_SetIntPref(pref, value));
}

NS_IMETHODIMP nsPref::SetBoolPref(const char *pref,PRBool value)
{
    return _convertRes(PREF_SetBoolPref(pref, value));
}

NS_IMETHODIMP nsPref::SetBinaryPref(const char *pref,void * value, PRUint32 size)
{
    return _convertRes(PREF_SetBinaryPref(pref, value, size));
}

NS_IMETHODIMP nsPref::SetColorPref(const char *pref, 
                     PRUint8 red, PRUint8 green, PRUint8 blue)
{
    return _convertRes(PREF_SetColorPref(pref, red, green, blue));
}

NS_IMETHODIMP nsPref::SetColorPrefDWord(const char *pref, 
                    PRUint32 value)
{
    return _convertRes(PREF_SetColorPrefDWord(pref, value));
}

NS_IMETHODIMP nsPref::SetRectPref(const char *pref, 
                    PRInt16 left, PRInt16 top, 
                    PRInt16 right, PRInt16 bottom)
{
    return _convertRes(PREF_SetRectPref(pref, left, top, right, bottom));
}

/*
 * Get Defaults
 */

NS_IMETHODIMP nsPref::GetDefaultIntPref(const char *pref,
                    PRInt32 * return_int)
{
    return _convertRes(PREF_GetDefaultIntPref(pref, return_int));
}

NS_IMETHODIMP nsPref::GetDefaultBoolPref(const char *pref,
                     PRBool * return_val)
{
    return _convertRes(PREF_GetDefaultBoolPref(pref, return_val));
}

NS_IMETHODIMP nsPref::GetDefaultBinaryPref(const char *pref, 
                         void * return_val,
                         int * buf_length)
{
    return _convertRes(PREF_GetDefaultBinaryPref(pref, return_val, buf_length));
}

NS_IMETHODIMP nsPref::GetDefaultColorPref(const char *pref, 
                        PRUint8 *red, PRUint8 *green, 
                        PRUint8 *blue)
{
    return _convertRes(PREF_GetDefaultColorPref(pref, red, green, blue));
}

NS_IMETHODIMP nsPref::GetDefaultColorPrefDWord(const char *pref, 
                                 PRUint32 *colorref)
{
    return _convertRes(PREF_GetDefaultColorPrefDWord(pref, colorref));
}

NS_IMETHODIMP nsPref::GetDefaultRectPref(const char *pref, 
                     PRInt16 *left, PRInt16 *top, 
                     PRInt16 *right, PRInt16 *bottom)
{
    return _convertRes(PREF_GetDefaultRectPref(pref, 
                             left, top, right, bottom));
}

/*
 * Set defaults
 */

NS_IMETHODIMP nsPref::SetDefaultCharPref(const char *pref,const char* value)
{
    return _convertRes(PREF_SetDefaultCharPref(pref, value));
}

NS_IMETHODIMP nsPref::SetDefaultIntPref(const char *pref,PRInt32 value)
{
    return _convertRes(PREF_SetDefaultIntPref(pref, value));
}

NS_IMETHODIMP nsPref::SetDefaultBoolPref(const char *pref, PRBool value)
{
    return _convertRes(PREF_SetDefaultBoolPref(pref, value));
}

NS_IMETHODIMP nsPref::SetDefaultBinaryPref(const char *pref,
                         void * value, PRUint32 size)
{
    return _convertRes(PREF_SetDefaultBinaryPref(pref, value, size));
}

NS_IMETHODIMP nsPref::SetDefaultColorPref(const char *pref, 
                        PRUint8 red, PRUint8 green, PRUint8 blue)
{
    return _convertRes(PREF_SetDefaultColorPref(pref, red, green, blue));
}

NS_IMETHODIMP nsPref::SetDefaultRectPref(const char *pref, 
                     PRInt16 left, PRInt16 top,
                     PRInt16 right, PRInt16 bottom)
{
    return _convertRes(PREF_SetDefaultRectPref(pref, left, top, right, bottom));
}

NS_IMETHODIMP nsPref::ClearUserPref(const char *pref_name)
{
    return _convertRes(PREF_ClearUserPref(pref_name));
}

/*
 * Copy prefs
 */

NS_IMETHODIMP nsPref::CopyCharPref(const char *pref, char ** return_buf)
{
    return _convertRes(PREF_CopyCharPref(pref, return_buf));
}

NS_IMETHODIMP nsPref::CopyBinaryPref(const char *pref,
                         int *size, void ** return_value)
{
    return _convertRes(PREF_CopyBinaryPref(pref, return_value, size));
}

NS_IMETHODIMP nsPref::CopyDefaultCharPref( const char *pref,
                         char ** return_buffer )
{
    return _convertRes(PREF_CopyDefaultCharPref(pref, return_buffer));
}

NS_IMETHODIMP nsPref::CopyDefaultBinaryPref(const char *pref, 
                            int * size, void ** return_val)
{
    return _convertRes(PREF_CopyDefaultBinaryPref(pref, return_val, size));
}

#if 0
/*
 * Path prefs
 */

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::CopyPathPref(const char *pref, char ** return_buf)
//----------------------------------------------------------------------------------------
{
    return _convertRes(PREF_CopyPathPref(pref, return_buf));
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::SetPathPref(const char *pref, 
                    const char *path, PRBool set_default)
//----------------------------------------------------------------------------------------
{
    return _convertRes(PREF_SetPathPref(pref, path, set_default));
}
#endif

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::GetFilePref(const char *pref_name, nsFileSpec** value)
//----------------------------------------------------------------------------------------
{
    if (!value)
        return NS_ERROR_NULL_POINTER;        
    char *encodedString = nsnull;
    PrefResult result = PREF_CopyCharPref(pref_name, &encodedString);
    if (result != PREF_NOERROR)
        return _convertRes(result);

    nsInputStringStream stream(encodedString);
    nsPersistentFileDescriptor descriptor;
    stream >> descriptor;
    PR_Free(encodedString); // Allocated by PREF_CopyCharPref
    *value = new nsFileSpec(descriptor);
    if (!value)
      return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::SetFilePref(const char *pref_name, 
                    nsFileSpec* value, PRBool set_default)
//----------------------------------------------------------------------------------------
{
    if (!value)
        return NS_ERROR_NULL_POINTER;        
    nsresult rv = NS_OK;
    if (!value->Exists())
    {
        // nsPersistentFileDescriptor requires an existing
        // object. Make it first.
        nsFileSpec tmp(*value);
        tmp.CreateDir();
    }
    nsPersistentFileDescriptor descriptor(*value);
    char* encodedString = nsnull;
    nsOutputStringStream stream(encodedString);
    stream << descriptor;
    if (encodedString && *encodedString)
    {
        if (set_default)
            rv = PREF_SetDefaultCharPref(pref_name, encodedString);
        else
            rv = PREF_SetCharPref(pref_name, encodedString);
    }
    delete [] encodedString; // Allocated by nsOutputStringStream
    return _convertRes(rv);
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
NS_IMETHODIMP nsPref::CopyPrefsTree(const char *srcRoot, const char *destRoot)
//----------------------------------------------------------------------------------------
{
    return _convertRes(PREF_CopyPrefsTree(srcRoot, destRoot));
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPref::DeleteBranch(const char *branchName)
//----------------------------------------------------------------------------------------
{
    return _convertRes(PREF_DeleteBranch(branchName));
}

//========================================================================================
class nsPrefFactory: public nsIFactory
//========================================================================================
{
    NS_DECL_ISUPPORTS
    
    nsPrefFactory()
    {
        NS_INIT_REFCNT();
        PR_AtomicIncrement(&g_InstanceCount);
    }

    virtual ~nsPrefFactory()
    {
        PR_AtomicDecrement(&g_InstanceCount);
    }

    NS_IMETHOD CreateInstance(nsISupports *aDelegate,
                                                        const nsIID &aIID,
                                                        void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock) {
        if (aLock) {
            PR_AtomicIncrement(&g_LockCount);
        } else {
            PR_AtomicDecrement(&g_LockCount);
        }
        return NS_OK;
    };
};

static NS_DEFINE_IID(kFactoryIID, NS_IFACTORY_IID);

NS_IMPL_ISUPPORTS(nsPrefFactory, kFactoryIID);

//----------------------------------------------------------------------------------------
nsresult nsPrefFactory::CreateInstance(
    nsISupports *aDelegate,
    const nsIID &aIID,
    void **aResult)
//----------------------------------------------------------------------------------------
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

//----------------------------------------------------------------------------------------
extern "C" NS_EXPORT nsresult NSGetFactory(
    nsISupports* serviceMgr,
    const nsCID &aClass,
    const char *aClassName,
    const char *aProgID,
    nsIFactory **aFactory)
//----------------------------------------------------------------------------------------
{
    if (aFactory == NULL)
        return NS_ERROR_NULL_POINTER;

    if (aClass.Equals(kPrefCID))
    {
        nsPrefFactory *factory = new nsPrefFactory();
        nsresult res = factory->QueryInterface(kFactoryIID, (void **) aFactory);
        if (NS_FAILED(res))
        {
            *aFactory = NULL;
            delete factory;
        }
        return res;
    }
    return NS_NOINTERFACE;
}

//----------------------------------------------------------------------------------------
extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* serviceMgr)
//----------------------------------------------------------------------------------------
{
    return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

//----------------------------------------------------------------------------------------
extern "C" NS_EXPORT nsresult NSRegisterSelf(nsISupports* aServMgr, const char *path)
//----------------------------------------------------------------------------------------
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    nsIComponentManager* compMgr;
    rv = servMgr->GetService(kComponentManagerCID, 
                                                     nsIComponentManager::GetIID(), 
                                                     (nsISupports**)&compMgr);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kPrefCID,
                                    "Preferences Service",
                                    "component://netscape/preferences",
                                    path, 
                                    PR_TRUE, PR_TRUE);

    (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
    return rv;
}

//----------------------------------------------------------------------------------------
extern "C" NS_EXPORT nsresult NSUnregisterSelf(nsISupports* aServMgr, const char *path)
//----------------------------------------------------------------------------------------
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    nsIComponentManager* compMgr;
    rv = servMgr->GetService(
        kComponentManagerCID, 
        nsIComponentManager::GetIID(), 
        (nsISupports**)&compMgr);
    if (NS_FAILED(rv))
        return rv;

    rv = compMgr->UnregisterComponent(kPrefCID, path);

    (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
    return rv;
}

//========================================================================================
// C++ implementations of old C routines
//========================================================================================

//----------------------------------------------------------------------------------------
PrefResult pref_OpenFileSpec(
    const nsFileSpec& fileSpec,
    PRBool is_error_fatal,
    PRBool verifyHash,
    PRBool bGlobalContext,
    PRBool skipFirstLine)
//----------------------------------------------------------------------------------------
{
    PrefResult result = PREF_NOERROR;

    if (!fileSpec.Exists())
        return PREF_ERROR;

    PRUint32 fileLength = fileSpec.GetFileSize();
    if (fileLength == 0)
        return PREF_ERROR;
        
    nsInputFileStream stream(fileSpec);
    if (!stream.is_open())
        result =  PREF_ERROR;
    
    char* readBuf = nsnull;
    if (result == PREF_NOERROR)
    {
        readBuf = (char*)PR_MALLOC(fileLength * sizeof(char));
        if (!readBuf)
            result = PREF_OUT_OF_MEMORY;
    }
    
    if (result == PREF_NOERROR)
    {
        PRUint32 bytesRead = stream.read(readBuf, fileLength);
        if (bytesRead < fileLength)
            result = PREF_ERROR;
        else if (verifyHash && !pref_VerifyLockFile(readBuf, fileLength))
            result = PREF_BAD_LOCKFILE;
        else if (PREF_EvaluateConfigScript(readBuf, fileLength,
                fileSpec.GetCString(), bGlobalContext, PR_FALSE, skipFirstLine))
            result = PREF_NOERROR;
        PR_Free(readBuf);
    }

    // If the user prefs file exists but generates an error,
    //   don't clobber the file when we try to save it
    if ((!readBuf || result != PREF_NOERROR) && is_error_fatal)
        gErrorOpeningUserPrefs = PR_TRUE;
#ifdef XP_PC
    if (gErrorOpeningUserPrefs && is_error_fatal)
        MessageBox(nsnull,"Error in preference file (prefs.js).  Default preferences will be used.","Netscape - Warning", MB_OK);
#endif

    JS_GC(gMochaContext);
    return result;
} // pref_OpenFile

//----------------------------------------------------------------------------------------
PR_IMPLEMENT(PrefResult) PREF_SavePrefFileSpecWith(
    const nsFileSpec& fileSpec,
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
    XP_QSORT(valueArray, gHashTable->nentries, sizeof(char*), pref_CompareStrings);
    char** walker = valueArray;
    for (PRUint32 valueIdx = 0; valueIdx < gHashTable->nentries; valueIdx++,walker++)
    {
        if (*walker)
        {
            stream << *walker;
            PR_Free(*walker);
        }
    }
    PR_Free(valueArray);
    return PREF_NOERROR;
}

//----------------------------------------------------------------------------------------
extern "C" JSBool pref_InitInitialObjects()
// Initialize default preference JavaScript buffers from
// appropriate TEXT resources
//----------------------------------------------------------------------------------------
{
    nsresult rv;
    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_FAILED(rv))
    	return JS_TRUE;
    nsFileSpec componentsDir;
    rv = locator->GetFileLocation(nsSpecialFileSpec::App_ComponentsDirectory, &componentsDir);
    if (NS_FAILED(rv))
    	return JS_TRUE;
	static const char* specialFiles[] = {
		"initpref.js"
#ifdef XP_MAC
	,	"macprefs.js"
#elif defined(XP_PC)
	,	"winpref.js"
#elif defined(XP_UNIX)
	,	"unixpref.js"
#endif
	};
    JSBool worked = (JSBool)(pref_OpenFileSpec(
    	componentsDir + specialFiles[0],
    	PR_FALSE,
    	PR_FALSE,
    	PR_FALSE,
    	PR_FALSE) == PREF_NOERROR);
    // Parse all the random files that happen to be in the components directory.
	for (nsDirectoryIterator i(componentsDir); i.Exists(); i++)
	{
		const char* leafName = i.Spec().GetLeafName();
		PRBool shouldParse = PR_TRUE;
		// Skip non-js files
		if (strstr(leafName, ".js") + strlen(".js") != leafName + strlen(leafName))
			shouldParse = PR_FALSE;
		// Skip files in the special list.
		if (shouldParse)
		{
			for (int j = 0; j < sizeof(specialFiles) / sizeof(char*); j++)
				if (strcmp(leafName, specialFiles[j]) == 0)
					shouldParse = PR_FALSE;
		}
		if (shouldParse)
		{
		    worked = (JSBool)(pref_OpenFileSpec(
		    	i.Spec(),
		    	PR_FALSE,
		    	PR_FALSE,
		    	PR_FALSE,
		    	PR_FALSE) == PREF_NOERROR);
		}
		nsCRT::free((char*)leafName);
	}
	// Finally, parse any other special files (platform-specific ones).
	for (int k = 1; k < sizeof(specialFiles) / sizeof(char*); k++)
	{
	    worked = (JSBool)(pref_OpenFileSpec(
    		componentsDir + specialFiles[k],
	    	PR_FALSE,
	    	PR_FALSE,
	    	PR_FALSE,
	    	PR_FALSE) == PREF_NOERROR);
	}
	NS_ASSERTION(worked, "Config file was not read");
    return JS_TRUE;
}
