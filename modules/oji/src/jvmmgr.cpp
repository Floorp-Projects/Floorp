/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

////////////////////////////////////////////////////////////////////////////////
// Plugin Manager Methods to support the JVM Plugin API
////////////////////////////////////////////////////////////////////////////////

#include "jvmmgr.h"
#include "xp.h"
#include "net.h"
#include "prefapi.h"
#include "xp_str.h"
#include "libmocha.h"
#include "np.h"

#include "xpgetstr.h"
extern "C" int XP_PROGRESS_STARTING_JAVA;
extern "C" int XP_PROGRESS_STARTING_JAVA_DONE;
extern "C" int XP_JAVA_NO_CLASSES;
extern "C" int XP_JAVA_WRONG_CLASSES;
extern "C" int XP_JAVA_STARTUP_FAILED;
extern "C" int XP_JAVA_DEBUGGER_FAILED;

// FIXME -- need prototypes for these functions!!! XXX
#ifdef XP_MAC
extern "C" {
OSErr ConvertUnixPathToMacPath(const char *, char **);
void startAsyncCursors(void);
void stopAsyncCursors(void);
}
#endif // XP_MAC

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_AGGREGATED(JVMMgr);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIJVMPluginManagerIID, NP_IJVMPLUGINMANAGER_IID);
static NS_DEFINE_IID(kIJVMPluginIID, NP_IJVMPLUGIN_IID);
static NS_DEFINE_IID(kISymantecDebugManagerIID, NP_ISYMANTECDEBUGMANAGER_IID);

NS_METHOD
JVMMgr::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    if (outer && !aIID.Equals(kISupportsIID))
        return NS_NOINTERFACE;   // XXX right error?
    JVMMgr* jvmMgr = new JVMMgr(outer);
    nsresult result = jvmMgr->QueryInterface(aIID, aInstancePtr);
    if (result != NS_OK) {
        delete jvmMgr;
    }
    return result;
}

NPIJVMPlugin*
JVMMgr::GetJVM(void)
{
    if (fJVM) {
        fJVM->AddRef();
        return fJVM;
    }
    NPIPlugin* plugin = NPL_LoadPluginByType(NPJVM_MIME_TYPE);
    if (plugin) {
        nsresult rslt = plugin->QueryInterface(kIJVMPluginIID, (void**)&fJVM);
        if (rslt == NS_OK) return fJVM;
    }
    return NULL;   
}

JVMMgr::JVMMgr(nsISupports* outer)
    : fJVM(NULL), fWaiting(0), fOldCursor(NULL), fProgramPath(NULL)
{
    NS_INIT_AGGREGATED(outer);
}

JVMMgr::~JVMMgr()
{
    if (fJVM) {
        nsrefcnt c = fJVM->Release();   // Release for QueryInterface in GetJVM
        // XXX unload plugin if c == 1 ? (should this be done inside Release?)
    }
}

NS_METHOD
JVMMgr::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (aIID.Equals(kIJVMPluginManagerIID)) {
        *aInstancePtr = this;
        AddRef();
        return NS_OK;
    }
#ifdef XP_PC
    // Aggregates...
    if (fDebugManager == NULL) {
        nsresult rslt =
            nsSymantecDebugManager::Create(this, kISupportsIID,
                                           (void**)&fDebugManager, this);
        if (rslt != NS_OK) return rslt;
    }
    return fDebugManager->QueryInterface(aIID, aInstancePtr);
#else
    return NS_NOINTERFACE;
#endif
}

////////////////////////////////////////////////////////////////////////////////

NS_METHOD_(void)
JVMMgr::BeginWaitCursor(void)
{
    if (fWaiting == 0) {
#ifdef XP_PC
        fOldCursor = (void*)GetCursor();
        HCURSOR newCursor = LoadCursor(NULL, IDC_WAIT);
        if (newCursor)
            SetCursor(newCursor);
#endif
#ifdef XP_MAC
        startAsyncCursors();
#endif
    }
    fWaiting++;
}

NS_METHOD_(void)
JVMMgr::EndWaitCursor(void)
{
    fWaiting--;
    if (fWaiting == 0) {
#ifdef XP_PC
        if (fOldCursor)
            SetCursor((HCURSOR)fOldCursor);
#endif
#ifdef XP_MAC
        stopAsyncCursors();
#endif
        fOldCursor = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////

NS_METHOD_(const char*)
JVMMgr::GetProgramPath(void)
{
    return fProgramPath;
}

NS_METHOD_(const char*)
JVMMgr::GetTempDirPath(void)
{
    // XXX I don't need a static really, the browser holds the tempDir name
    // as a static string -- it's just the XP_TempDirName that strdups it.
    static const char* tempDirName = NULL;
    if (tempDirName == NULL)
        tempDirName = XP_TempDirName();
    return tempDirName;
}

NS_METHOD
JVMMgr::GetFileName(const char* fn, FileNameType type,
                    char* resultBuf, PRUint32 bufLen)
{
    // XXX This should be rewritten so that we don't have to malloc the name.
    XP_FileType filetype;

    if (type == SIGNED_APPLET_DBNAME)
        filetype = xpSignedAppletDB;
    else if (type == TEMP_FILENAME)
        filetype = xpTemporary;
    else 
        return NS_ERROR_ILLEGAL_VALUE;

    char* tempName = WH_FileName(fn, filetype);
    if (tempName == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    XP_STRNCPY_SAFE(resultBuf, tempName, bufLen);
    XP_FREE(tempName);
    return NS_OK;
}

NS_METHOD
JVMMgr::NewTempFileName(const char* prefix, char* resultBuf, PRUint32 bufLen)
{
    // XXX This should be rewritten so that we don't have to malloc the name.
    char* tempName = WH_TempName(xpTemporary, prefix);
    if (tempName == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    XP_STRNCPY_SAFE(resultBuf, tempName, bufLen);
    XP_FREE(tempName);
    return NS_OK;
}

NS_METHOD_(PRBool)
JVMMgr::HandOffJSLock(PRThread* oldOwner, PRThread* newOwner)
{
    return LM_HandOffJSLock(oldOwner, newOwner);
}

////////////////////////////////////////////////////////////////////////////////

// Should be in a header; must solve build-order problem first
extern "C" void VR_Initialize(JRIEnv* env);
extern "C" void SU_Initialize(JRIEnv * env);

JVMStatus
JVMMgr::StartupJVM(void)
{
    // Be sure to check the prefs first before asking java to startup.
    if (GetJVMStatus() == JVMStatus_Disabled) {
        return JVMStatus_Disabled;
    }
    JVMStatus status = fJVM->StartupJVM();
    if (status == JVMStatus_Running) {
        JVMEnv* env = fJVM->EnsureExecEnv();

        /* initialize VersionRegistry native routines */
        /* it is not an error that prevents java from starting if this stuff throws exceptions */
#ifdef XP_MAC
		if (env->ExceptionOccurred()) {
#ifdef DEBUG
            env->ExceptionDescribe();
#endif	
            env->ExceptionClear();
            return ShutdownJVM();
        }
#else        

        SU_Initialize(env);
        if (JRI_ExceptionOccurred(env)) {
#ifdef DEBUG
            fJVM->PrintToConsole("LJ:  SU_Initialize failed.  Bugs to atotic.\n");
            JRI_ExceptionDescribe(env);
#endif	
            JRI_ExceptionClear(env);
            return ShutdownJVM();
        }
#endif /* !XP_MAC */
    }
    return status;
}

JVMStatus
JVMMgr::ShutdownJVM(PRBool fullShutdown)
{
    if (fJVM)
        return fJVM->ShutdownJVM();
    return JVMStatus_Enabled;   // XXX what if there's no plugin available?
}

////////////////////////////////////////////////////////////////////////////////

static int PR_CALLBACK
JavaPrefChanged(const char *prefStr, void* data)
{
    JVMMgr* mgr = (JVMMgr*)data;
    XP_Bool prefBool;
    PREF_GetBoolPref(prefStr, &prefBool);
    mgr->SetJVMEnabled((PRBool)prefBool);
    return 0;
} 

void
JVMMgr::EnsurePrefCallbackRegistered(void)
{
    if (fRegisteredJavaPrefChanged != PR_TRUE) {
        fRegisteredJavaPrefChanged = PR_TRUE;
        PREF_RegisterCallback("security.enable_java", JavaPrefChanged, this);
        JavaPrefChanged("security.enable_java", this);
    }
}

PRBool
JVMMgr::GetJVMEnabled(void)
{
    EnsurePrefCallbackRegistered();
    return fJVM->GetJVMEnabled();
}

void
JVMMgr::SetJVMEnabled(PRBool enable)
{
    fJVM->SetJVMEnabled(enable);
}

JVMStatus
JVMMgr::GetJVMStatus(void)
{
    EnsurePrefCallbackRegistered();
    return fJVM->GetJVMStatus();
}

////////////////////////////////////////////////////////////////////////////////

static const char*
ConvertToPlatformPathList(const char* cp)
{
    const char* c = strdup(cp);
    if (c == NULL)
        return NULL;
#ifdef XP_MAC
    {
        const char* path;
        const char* strtok_path;
		
        char* result = (char*) malloc(1);
        if (result == NULL)
            return NULL;
        result[0] = '\0';
        path = c;
        strtok_path = path;
#ifndef NSPR20
        while ((path = XP_STRTOK(strtok_path, PATH_SEPARATOR_STR)))
#else
        while ((path = XP_STRTOK(strtok_path, PR_PATH_SEPARATOR_STR)))
#endif
        {
            const char* macPath;
            OSErr err = ConvertUnixPathToMacPath(path, &macPath);
            char* r = PR_smprintf("%s\r%s", result, macPath);
			
            strtok_path = NULL;

            if (r == NULL)
                return result;	/* return what we could come up with */
            free(result);
            result = r;
        }
        free(c);
        return result;
    }
#else
    return c;
#endif
}

NS_METHOD_(void)
JVMMgr::ReportJVMError(JVMEnv* env, JVMError err)
{
    MWContext* cx = XP_FindSomeContext();
    char *s;
    switch (err) {
      case JVMError_NoClasses: {
          const char* cp = fJVM->GetClassPath();
          const char* jarName = fJVM->GetSystemJARPath();
          cp = ConvertToPlatformPathList(cp);
          s = PR_smprintf(XP_GetString(XP_JAVA_NO_CLASSES),
                          jarName, jarName,
                          (cp ? cp : "<none>"));
          free((void*)cp);
          break;
      }

      case JVMError_JavaError: {
          const char* msg = GetJavaErrorString(env);
#ifdef DEBUG
# ifdef XP_MAC
		  env->ExceptionDescribe();
# else
          JRI_ExceptionDescribe(env);
# endif
#endif
          s = PR_smprintf(XP_GetString(XP_JAVA_STARTUP_FAILED), 
                          (msg ? msg : ""));
          if (msg) free((void*)msg);
          break;
      }
      case JVMError_NoDebugger: {
          s = PR_smprintf(XP_GetString(XP_JAVA_DEBUGGER_FAILED));
          break;
      }
      default:
        return;	/* don't report anything */
    }
    if (s) {
        FE_Alert(cx, s);
        free(s);
    }
}

/* stolen from java_lang_Object.h (jri version) */
#define classname_java_lang_Object	"java/lang/Object"
#define name_java_lang_Object_toString	"toString"
#define sig_java_lang_Object_toString 	"()Ljava/lang/String;"

const char*
JVMMgr::GetJavaErrorString(JVMEnv* env)
{
    /* XXX javah is a pain wrt mixing JRI and JDK native methods. 
       Since we need to call a method on Object, we'll do it the hard way 
       to avoid using javah for this.
       Maybe we need a new JRI entry point for toString. Yikes! */

#ifdef XP_MAC
    jthrowable exc = env->ExceptionOccurred();
    if (exc == NULL) {
        return strdup("");	/* XXX better "no error" message? */
    }

    /* Got to do this before trying to find a class (like Object). 
       This is because the runtime refuses to do this with a pending 
       exception! I think it's broken. */

    env->ExceptionClear();

    jclass classObject = env->FindClass(classname_java_lang_Object);
    jmethodID toString = env->GetMethodID(classObject, name_java_lang_Object_toString, sig_java_lang_Object_toString);
    jstring excString = (jstring) env->CallObjectMethod(exc, toString);

	jboolean isCopy;
    const char* msg = env->GetStringUTFChars(excString, &isCopy);
    if (msg != NULL) {
	    const char* dupmsg = (msg == NULL ? NULL : strdup(msg));
    	env->ReleaseStringUTFChars(excString, msg);
    	msg = dupmsg;
    }
    return msg;
#else
    const char* msg;
    struct java_lang_Class* classObject;
    JRIMethodID toString;
    struct java_lang_String* excString;
    struct java_lang_Throwable* exc = JRI_ExceptionOccurred(env);

    if (exc == NULL) {
        return strdup("");	/* XXX better "no error" message? */
    }

    /* Got to do this before trying to find a class (like Object). 
       This is because the runtime refuses to do this with a pending 
       exception! I think it's broken. */

    JRI_ExceptionClear(env);

    classObject = (struct java_lang_Class*)
        JRI_FindClass(env, classname_java_lang_Object);
    toString = JRI_GetMethodID(env, classObject,
                               name_java_lang_Object_toString,
                               sig_java_lang_Object_toString);
    excString = (struct java_lang_String *)
        JRI_CallMethod(env)(env, JRI_CallMethod_op,
                            (struct java_lang_Object*)exc, toString);

    /* XXX change to JRI_GetStringPlatformChars */
    msg = JRI_GetStringPlatformChars(env, excString, NULL, 0);
    if (msg == NULL) return NULL;
    return strdup(msg);
#endif
}

NS_METHOD_(PRBool)
JVMMgr::SupportsURLProtocol(const char* protocol)
{
    int type = NET_URL_Type(protocol);
    return (PRBool)(type != 0);
}

////////////////////////////////////////////////////////////////////////////////

extern "C" PR_IMPLEMENT(JVMMgr*)
JVM_GetJVMMgr(void)
{
    extern nsISupports* thePluginManager;
    if (thePluginManager == NULL)
        return NULL;
    JVMMgr* mgr;
    nsresult res =
        thePluginManager->QueryInterface(kIJVMPluginManagerIID, (void**)&mgr);
    if (res != NS_OK)
        return NULL;
    return mgr;
}

////////////////////////////////////////////////////////////////////////////////

