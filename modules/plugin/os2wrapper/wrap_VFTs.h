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
 * The Original Code is InnoTek Plugin Wrapper code.
 *
 * The Initial Developer of the Original Code is
 * InnoTek Systemberatung GmbH.
 * Portions created by the Initial Developer are Copyright (C) 2003-2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   InnoTek Systemberatung GmbH / Knut St. Osmundsen
 *   Peter Weilbacher <mozilla@weilbacher.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 *  VFTable declarations.
 */

#ifndef __npVFTs_h__
#define __npVFTs_h__


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/** @group Visual Age for C++ v3.6.5 target (OS/2).
 * @{ */
/** Indicate Visual Age for C++ v3.6.5 target */
#define VFT_VAC365          1
/** VFTable/Interface Calling Convention for Win32. */
#define VFTCALL             _Optlink
/** First Entry which VAC uses. */
#define VFTFIRST_DECL       unsigned    uFirst[2];
#define VFTFIRST_VAL()      {0, 0},
/** This deltas which VAC uses. */
#define VFTDELTA_DECL(n)    unsigned    uDelta##n;
#define VFTDELTA_VAL()      0,
/** This calculates the this value to be used when calling a virtual function. */
#define VFTTHIS(pVFT, function, pvThis) ((char*)(pvThis) + (pVFT)->uDelta##function)
/** @} */





/** @group Macros for calling virtual functions.
 * @{ */
#define VFTCALL0(pVFT, function, pvThis) \
    (pVFT)->function(VFTTHIS(pVFT, function, pvThis))
#define VFTCALL1(pVFT, function, pvThis,              arg1) \
    (pVFT)->function(VFTTHIS(pVFT, function, pvThis), arg1)
#define VFTCALL2(pVFT, function, pvThis,              arg1, arg2) \
    (pVFT)->function(VFTTHIS(pVFT, function, pvThis), arg1, arg2)
#define VFTCALL3(pVFT, function, pvThis,              arg1, arg2, arg3) \
    (pVFT)->function(VFTTHIS(pVFT, function, pvThis), arg1, arg2, arg3)
#define VFTCALL4(pVFT, function, pvThis,              arg1, arg2, arg3, arg4) \
    (pVFT)->function(VFTTHIS(pVFT, function, pvThis), arg1, arg2, arg3, arg4)
#define VFTCALL5(pVFT, function, pvThis,              arg1, arg2, arg3, arg4, arg5) \
    (pVFT)->function(VFTTHIS(pVFT, function, pvThis), arg1, arg2, arg3, arg4, arg5)
#define VFTCALL6(pVFT, function, pvThis,              arg1, arg2, arg3, arg4, arg5, arg6) \
    (pVFT)->function(VFTTHIS(pVFT, function, pvThis), arg1, arg2, arg3, arg4, arg5, arg6)
#define VFTCALL7(pVFT, function, pvThis,              arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
    (pVFT)->function(VFTTHIS(pVFT, function, pvThis), arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define VFTCALL8(pVFT, function, pvThis,              arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) \
    (pVFT)->function(VFTTHIS(pVFT, function, pvThis), arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define VFTCALL9(pVFT, function, pvThis,              arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) \
    (pVFT)->function(VFTTHIS(pVFT, function, pvThis), arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)
#define VFTCALL10(pVFT, function, pvThis,             arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10) \
    (pVFT)->function(VFTTHIS(pVFT, function, pvThis), arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10)
#define VFTCALL11(pVFT, function, pvThis,             arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11) \
    (pVFT)->function(VFTTHIS(pVFT, function, pvThis), arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11)
#define VFTCALL12(pVFT, function, pvThis,             arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12) \
    (pVFT)->function(VFTTHIS(pVFT, function, pvThis), arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12)
#define VFTCALL13(pVFT, function, pvThis,             arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13) \
    (pVFT)->function(VFTTHIS(pVFT, function, pvThis), arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13)
#define VFTCALL14(pVFT, function, pvThis,             arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14) \
    (pVFT)->function(VFTTHIS(pVFT, function, pvThis), arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14)
#define VFTCALL15(pVFT, function, pvThis,             arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15) \
    (pVFT)->function(VFTTHIS(pVFT, function, pvThis), arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15)
#define VFTCALL16(pVFT, function, pvThis,             arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15, arg16) \
    (pVFT)->function(VFTTHIS(pVFT, function, pvThis), arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15, arg16)
/** @} */


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** @name VFTs
 * @{
 */

/**
 * nsISupports vftable.
 */
typedef struct vftable_nsISupports
{
    VFTFIRST_DECL
    nsresult (*VFTCALL QueryInterface)(void *pvThis, REFNSIID aIID, void** aInstancePtr);
    VFTDELTA_DECL(QueryInterface)
    nsrefcnt (*VFTCALL AddRef)(void *pvThis);
    VFTDELTA_DECL(AddRef)
    nsrefcnt (*VFTCALL Release)(void *pvThis);
    VFTDELTA_DECL(Release)
} VFTnsISupports;


/**
 * nsIServiceManager vftable.
 */
typedef struct vftable_nsIServiceManager
{
    VFTnsISupports     base;
    nsresult (*VFTCALL GetService)(void *pvThis, const nsCID & aClass, const nsIID & aIID, void * *result);
    VFTDELTA_DECL(GetService)
    nsresult (*VFTCALL GetServiceByContractID)(void *pvThis, const char *aContractID, const nsIID & aIID, void * *result);
    VFTDELTA_DECL(GetServiceByContractID)
    nsresult (*VFTCALL IsServiceInstantiated)(void *pvThis, const nsCID & aClass, const nsIID & aIID, int *_retval);
    VFTDELTA_DECL(IsServiceInstantiated)
    nsresult (*VFTCALL IsServiceInstantiatedByContractID)(void *pvThis, const char *aContractID, const nsIID & aIID, int *_retval);
    VFTDELTA_DECL(IsServiceInstantiatedByContractID)
} VFTnsIServiceManager;


/**
 * nsIServiceManagerObsolete vftable.
 */
typedef struct vftable_nsIServiceManagerObsolete
{
    VFTnsISupports     base;
    #ifdef VFT_VAC365
    nsresult (*VFTCALL RegisterService)(void *pvThis, const nsCID& aClass, nsISupports* aService);
    VFTDELTA_DECL(RegisterService)
    nsresult (*VFTCALL RegisterServiceByContractID)(void *pvThis, const char* aContractID, nsISupports* aService);
    VFTDELTA_DECL(RegisterServiceByContractID)
    nsresult (*VFTCALL UnregisterService)(void *pvThis, const nsCID& aClass);
    VFTDELTA_DECL(UnregisterService)
    nsresult (*VFTCALL UnregisterServiceByContractID)(void *pvThis, const char* aContractID);
    VFTDELTA_DECL(UnregisterServiceByContractID)
    nsresult (*VFTCALL GetService)(void *pvThis, const nsCID& aClass, const nsIID& aIID, nsISupports* *result, nsIShutdownListener* shutdownListener);
    VFTDELTA_DECL(GetService)
    nsresult (*VFTCALL GetServiceByContractID)(void *pvThis, const char* aContractID, const nsIID& aIID, nsISupports* *result, nsIShutdownListener* shutdownListener);
    VFTDELTA_DECL(GetServiceByContractID)
    nsresult (*VFTCALL ReleaseService)(void *pvThis, const nsCID& aClass, nsISupports* service, nsIShutdownListener* shutdownListener);
    VFTDELTA_DECL(ReleaseService)
    nsresult (*VFTCALL ReleaseServiceByContractID)(void *pvThis, const char* aContractID, nsISupports* service, nsIShutdownListener* shutdownListener);
    VFTDELTA_DECL(ReleaseServiceByContractID)
    #else
    nsresult (*VFTCALL RegisterServiceByContractID)(void *pvThis, const char* aContractID, nsISupports* aService);
    VFTDELTA_DECL(RegisterServiceByContractID)
    nsresult (*VFTCALL RegisterService)(void *pvThis, const nsCID& aClass, nsISupports* aService);
    VFTDELTA_DECL(RegisterService)
    nsresult (*VFTCALL UnregisterServiceByContractID)(void *pvThis, const char* aContractID);
    VFTDELTA_DECL(UnregisterServiceByContractID)
    nsresult (*VFTCALL UnregisterService)(void *pvThis, const nsCID& aClass);
    VFTDELTA_DECL(UnregisterService)
    nsresult (*VFTCALL GetServiceByContractID)(void *pvThis, const char* aContractID, const nsIID& aIID, nsISupports* *result, nsIShutdownListener* shutdownListener);
    VFTDELTA_DECL(GetServiceByContractID)
    nsresult (*VFTCALL GetService)(void *pvThis, const nsCID& aClass, const nsIID& aIID, nsISupports* *result, nsIShutdownListener* shutdownListener);
    VFTDELTA_DECL(GetService)
    nsresult (*VFTCALL ReleaseServiceByContractID)(void *pvThis, const char* aContractID, nsISupports* service, nsIShutdownListener* shutdownListener);
    VFTDELTA_DECL(ReleaseServiceByContractID)
    nsresult (*VFTCALL ReleaseService)(void *pvThis, const nsCID& aClass, nsISupports* service, nsIShutdownListener* shutdownListener);
    VFTDELTA_DECL(ReleaseService)
    #endif
} VFTnsIServiceManagerObsolete;

/**
 * nsIFactory
 */
typedef struct vftable_nsIFactory
{
    VFTnsISupports  base;
    nsresult (*VFTCALL CreateInstance)(void *pvThis, nsISupports *aOuter, const nsIID & iid, void * *result);
    VFTDELTA_DECL(CreateInstance)
    nsresult (*VFTCALL LockFactory)(void *pvThis, PRBool lock);
    VFTDELTA_DECL(LockFactory)
} VFTnsIFactory;


/**
 * nsIPlugin
 */
typedef struct vftable_nsIPlugin
{
    VFTnsIFactory   base;
    nsresult (*VFTCALL CreatePluginInstance)(void *pvThis, nsISupports *aOuter, const nsIID & aIID, const char *aPluginMIMEType, void * *aResult);
    VFTDELTA_DECL(CreatePluginInstance)
    nsresult (*VFTCALL Initialize)(void *pvThis);
    VFTDELTA_DECL(Initialize)
    nsresult (*VFTCALL Shutdown)(void *pvThis);
    VFTDELTA_DECL(Shutdown)
    nsresult (*VFTCALL GetMIMEDescription)(void *pvThis, const char * *aMIMEDescription);
    VFTDELTA_DECL(GetMIMEDescription)
    nsresult (*VFTCALL GetValue)(void *pvThis, nsPluginVariable aVariable, void * aValue);
    VFTDELTA_DECL(GetValue)
} VFTnsIPlugin;


/**
 * nsIPluginManager
 */
typedef struct vftable_nsIPluginManager
{
    VFTnsISupports  base;
    nsresult (*VFTCALL GetValue)(void *pvThis, nsPluginManagerVariable variable, void * value);
    VFTDELTA_DECL(GetValue)
    nsresult (*VFTCALL ReloadPlugins)(void *pvThis, PRBool reloadPages);
    VFTDELTA_DECL(ReloadPlugins)
    nsresult (*VFTCALL UserAgent)(void *pvThis, const char * * resultingAgentString);
    VFTDELTA_DECL(UserAgent)
    nsresult (*VFTCALL GetURL)(void *pvThis, nsISupports* pluginInst, const char* url, const char* target,
                                  nsIPluginStreamListener* streamListener, const char* altHost, const char* referrer,
                                  PRBool forceJSEnabled);
    VFTDELTA_DECL(GetURL)
    nsresult (*VFTCALL PostURL)(void *pvThis, nsISupports* pluginInst, const char* url, PRUint32 postDataLen, const char* postData,
                                   PRBool isFile, const char* target, nsIPluginStreamListener* streamListener,
                                   const char* altHost, const char* referrer, PRBool forceJSEnabled,
                                   PRUint32 postHeadersLength, const char* postHeaders);
    VFTDELTA_DECL(PostURL)
    nsresult (*VFTCALL RegisterPlugin)(void *pvThis, REFNSIID aCID, const char *aPluginName, const char *aDescription, const char * * aMimeTypes,
                                          const char * * aMimeDescriptions, const char * * aFileExtensions, PRInt32 aCount);
    VFTDELTA_DECL(RegisterPlugin)
    nsresult (*VFTCALL UnregisterPlugin)(void *pvThis, REFNSIID aCID);
    VFTDELTA_DECL(UnregisterPlugin)
    nsresult (*VFTCALL GetURLWithHeaders)(void *pvThis, nsISupports* pluginInst, const char* url, const char* target,
                                             nsIPluginStreamListener* streamListener, const char* altHost, const char* referrer,
                                             PRBool forceJSEnabled, PRUint32 getHeadersLength, const char* getHeaders);
    VFTDELTA_DECL(GetURLWithHeaders)
} VFTnsIPluginManager;


/**
 * nsIJVMPlugin
 */
typedef struct vftable_nsIJVMPlugin
{
    VFTnsISupports  base;
    nsresult (*VFTCALL AddToClassPath)(void *pvThis, const char* dirPath);
    VFTDELTA_DECL(AddToClassPath)
    nsresult (*VFTCALL RemoveFromClassPath)(void *pvThis, const char* dirPath);
    VFTDELTA_DECL(RemoveFromClassPath)
    nsresult (*VFTCALL GetClassPath)(void *pvThis, const char* *result);
    VFTDELTA_DECL(GetClassPath)
    nsresult (*VFTCALL GetJavaWrapper)(void *pvThis, JNIEnv* jenv, jint obj, jobject *jobj);
    VFTDELTA_DECL(GetJavaWrapper)
    nsresult (*VFTCALL CreateSecureEnv)(void *pvThis, JNIEnv* proxyEnv, nsISecureEnv* *outSecureEnv);
    VFTDELTA_DECL(CreateSecureEnv)
    nsresult (*VFTCALL SpendTime)(void *pvThis, PRUint32 timeMillis);
    VFTDELTA_DECL(SpendTime)
    nsresult (*VFTCALL UnwrapJavaWrapper)(void *pvThis, JNIEnv* jenv, jobject jobj, jint* obj);
    VFTDELTA_DECL(UnwrapJavaWrapper)
} VFTnsIJVMPlugin;


/**
 * nsILiveconnect
 */
typedef struct vftable_nsILiveconnect
{
    VFTnsISupports  base;
    nsresult (*VFTCALL GetMember)(void *pvThis, JNIEnv *jEnv, jsobject jsobj, const jchar *name, jsize length, void* principalsArray[],
                                     int numPrincipals, nsISupports *securitySupports, jobject *pjobj);
    VFTDELTA_DECL(GetMember)
    nsresult (*VFTCALL GetSlot)(void *pvThis, JNIEnv *jEnv, jsobject jsobj, jint slot, void* principalsArray[],
                                   int numPrincipals, nsISupports *securitySupports, jobject *pjobj);
    VFTDELTA_DECL(GetSlot)
    nsresult (*VFTCALL SetMember)(void *pvThis, JNIEnv *jEnv, jsobject jsobj, const jchar* name, jsize length, jobject jobj, void* principalsArray[],
                                     int numPrincipals, nsISupports *securitySupports);
    VFTDELTA_DECL(SetMember)
    nsresult (*VFTCALL SetSlot)(void *pvThis, JNIEnv *jEnv, jsobject jsobj, jint slot, jobject jobj, void* principalsArray[],
                                   int numPrincipals, nsISupports *securitySupports);
    VFTDELTA_DECL(SetSlot)
    nsresult (*VFTCALL RemoveMember)(void *pvThis, JNIEnv *jEnv, jsobject jsobj, const jchar* name, jsize length,  void* principalsArray[],
                                        int numPrincipals, nsISupports *securitySupports);
    VFTDELTA_DECL(RemoveMember)
    nsresult (*VFTCALL Call)(void *pvThis, JNIEnv *jEnv, jsobject jsobj, const jchar* name, jsize length, jobjectArray jobjArr,  void* principalsArray[],
                                int numPrincipals, nsISupports *securitySupports, jobject *pjobj);
    VFTDELTA_DECL(Call)
    nsresult (*VFTCALL Eval)(void *pvThis, JNIEnv *jEnv, jsobject obj, const jchar *script, jsize length, void* principalsArray[],
                                int numPrincipals, nsISupports *securitySupports, jobject *pjobj);
    VFTDELTA_DECL(Eval)
    nsresult (*VFTCALL GetWindow)(void *pvThis, JNIEnv *jEnv, void *pJavaObject, void* principalsArray[], int numPrincipals,
                                     nsISupports *securitySupports, jsobject *pobj);
    VFTDELTA_DECL(GetWindow)
    nsresult (*VFTCALL FinalizeJSObject)(void *pvThis, JNIEnv *jEnv, jsobject jsobj);
    VFTDELTA_DECL(FinalizeJSObject)
    nsresult (*VFTCALL ToString)(void *pvThis, JNIEnv *jEnv, jsobject obj, jstring *pjstring);
    VFTDELTA_DECL(ToString)
} VFTnsILiveconnect;


/**
 * nsISecureLiveconnect
 */
typedef struct vftable_nsISecureLiveconnect
{
    VFTnsISupports  base;
    nsresult (*VFTCALL Eval)(void *pvThis, JNIEnv *jEnv, jsobject obj, const jchar *script, jsize length, void **pNSIPrincipaArray,
                                int numPrincipals, void *pNSISecurityContext, jobject *pjobj);
    VFTDELTA_DECL(Eval)
} VFTnsISecureLiveconnect;


/**
 * nsIComponentManagerObsolete
 */
typedef struct vftable_nsIComponentManagerObsolete
{
    VFTnsISupports  base;
    nsresult (*VFTCALL FindFactory)(void *pvThis,  const nsCID & aClass, nsIFactory **_retval);
    VFTDELTA_DECL(FindFactory)
    nsresult (*VFTCALL GetClassObject)(void *pvThis,  const nsCID & aClass, const nsIID & aIID, void * *_retval);
    VFTDELTA_DECL(GetClassObject)
    nsresult (*VFTCALL ContractIDToClassID)(void *pvThis,  const char *aContractID, nsCID *aClass);
    VFTDELTA_DECL(ContractIDToClassID)
    nsresult (*VFTCALL CLSIDToContractID)(void *pvThis,  const nsCID & aClass, char **aClassName, char **_retval);
    VFTDELTA_DECL(CLSIDToContractID)
    nsresult (*VFTCALL CreateInstance)(void *pvThis,  const nsCID & aClass, nsISupports *aDelegate, const nsIID & aIID, void * *_retval);
    VFTDELTA_DECL(CreateInstance)
    nsresult (*VFTCALL CreateInstanceByContractID)(void *pvThis,  const char *aContractID, nsISupports *aDelegate, const nsIID & IID, void * *_retval);
    VFTDELTA_DECL(CreateInstanceByContractID)
    nsresult (*VFTCALL RegistryLocationForSpec)(void *pvThis, nsIFile *aSpec, char **_retval);
    VFTDELTA_DECL(RegistryLocationForSpec)
    nsresult (*VFTCALL SpecForRegistryLocation)(void *pvThis,  const char *aLocation, nsIFile **_retval);
    VFTDELTA_DECL(SpecForRegistryLocation)
    nsresult (*VFTCALL RegisterFactory)(void *pvThis,  const nsCID & aClass, const char *aClassName, const char *aContractID, nsIFactory *aFactory, PRBool aReplace);
    VFTDELTA_DECL(RegisterFactory)
    nsresult (*VFTCALL RegisterComponent)(void *pvThis,  const nsCID & aClass, const char *aClassName, const char *aContractID, const char *aLocation, PRBool aReplace, PRBool aPersist);
    VFTDELTA_DECL(RegisterComponent)
    nsresult (*VFTCALL RegisterComponentWithType)(void *pvThis,  const nsCID & aClass, const char *aClassName, const char *aContractID, nsIFile *aSpec, const char *aLocation, PRBool aReplace, PRBool aPersist, const char *aType);
    VFTDELTA_DECL(RegisterComponentWithType)
    nsresult (*VFTCALL RegisterComponentSpec)(void *pvThis,  const nsCID & aClass, const char *aClassName, const char *aContractID, nsIFile *aLibrary, PRBool aReplace, PRBool aPersist);
    VFTDELTA_DECL(RegisterComponentSpec)
    nsresult (*VFTCALL RegisterComponentLib)(void *pvThis,  const nsCID & aClass, const char *aClassName, const char *aContractID, const char *aDllName, PRBool aReplace, PRBool aPersist);
    VFTDELTA_DECL(RegisterComponentLib)
    nsresult (*VFTCALL UnregisterFactory)(void *pvThis,  const nsCID & aClass, nsIFactory *aFactory);
    VFTDELTA_DECL(UnregisterFactory)
    nsresult (*VFTCALL UnregisterComponent)(void *pvThis,  const nsCID & aClass, const char *aLocation);
    VFTDELTA_DECL(UnregisterComponent)
    nsresult (*VFTCALL UnregisterComponentSpec)(void *pvThis,  const nsCID & aClass, nsIFile *aLibrarySpec);
    VFTDELTA_DECL(UnregisterComponentSpec)
    nsresult (*VFTCALL FreeLibraries)(void *pvThis);
    VFTDELTA_DECL(FreeLibraries)
    nsresult (*VFTCALL AutoRegister)(void *pvThis, PRInt32 when, nsIFile *directory);
    VFTDELTA_DECL(AutoRegister)
    nsresult (*VFTCALL AutoRegisterComponent)(void *pvThis, PRInt32 when, nsIFile *aFileLocation);
    VFTDELTA_DECL(AutoRegisterComponent)
    nsresult (*VFTCALL AutoUnregisterComponent)(void *pvThis, PRInt32 when, nsIFile *aFileLocation);
    VFTDELTA_DECL(AutoUnregisterComponent)
    nsresult (*VFTCALL IsRegistered)(void *pvThis,  const nsCID & aClass, PRBool *_retval);
    VFTDELTA_DECL(IsRegistered)
    nsresult (*VFTCALL EnumerateCLSIDs)(void *pvThis, nsIEnumerator **_retval);
    VFTDELTA_DECL(EnumerateCLSIDs)
    nsresult (*VFTCALL EnumerateContractIDs)(void *pvThis, nsIEnumerator **_retval);
    VFTDELTA_DECL(EnumerateContractIDs)
} VFTnsIComponentManagerObsolete;


/**
 * nsComponentManager.
 */
typedef struct vftable_nsIComponentManager
{
    VFTnsISupports  base;
    nsresult (*VFTCALL GetClassObject)(void *pvThis, const nsCID & aClass, const nsIID & aIID, void * *result);
    VFTDELTA_DECL(GetClassObject)
    nsresult (*VFTCALL GetClassObjectByContractID)(void *pvThis, const char *aContractID, const nsIID & aIID, void * *result);
    VFTDELTA_DECL(GetClassObjectByContractID)
    nsresult (*VFTCALL CreateInstance)(void *pvThis, const nsCID & aClass, nsISupports *aDelegate, const nsIID & aIID, void * *result);
    VFTDELTA_DECL(CreateInstance)
    nsresult (*VFTCALL CreateInstanceByContractID)(void *pvThis, const char *aContractID, nsISupports *aDelegate, const nsIID & aIID, void * *result);
    VFTDELTA_DECL(CreateInstanceByContractID)
} VFTnsIComponentManager;


/**
 * nsISecureEnv
 */
typedef struct vftable_nsISecureEnv
{
    VFTnsISupports  base;
    nsresult (*VFTCALL NewObject)(void *pvThis, jclass clazz, jmethodID methodID, jvalue *args, jobject* result, nsISecurityContext* ctx);
    VFTDELTA_DECL(NewObject)
    nsresult (*VFTCALL CallMethod)(void *pvThis, jni_type type, jobject obj, jmethodID methodID, jvalue *args, jvalue* result, nsISecurityContext* ctx);
    VFTDELTA_DECL(CallMethod)
    nsresult (*VFTCALL CallNonvirtualMethod)(void *pvThis, jni_type type, jobject obj, jclass clazz, jmethodID methodID, jvalue *args, jvalue* result, nsISecurityContext* ctx);
    VFTDELTA_DECL(CallNonvirtualMethod)
    nsresult (*VFTCALL GetField)(void *pvThis, jni_type type, jobject obj, jfieldID fieldID, jvalue* result, nsISecurityContext* ctx);
    VFTDELTA_DECL(GetField)
    nsresult (*VFTCALL SetField)(void *pvThis, jni_type type, jobject obj, jfieldID fieldID, jvalue val, nsISecurityContext* ctx);
    VFTDELTA_DECL(SetField)
    nsresult (*VFTCALL CallStaticMethod)(void *pvThis, jni_type type, jclass clazz, jmethodID methodID, jvalue *args, jvalue* result, nsISecurityContext* ctx);
    VFTDELTA_DECL(CallStaticMethod)
    nsresult (*VFTCALL GetStaticField)(void *pvThis, jni_type type, jclass clazz, jfieldID fieldID, jvalue* result, nsISecurityContext* ctx);
    VFTDELTA_DECL(GetStaticField)
    nsresult (*VFTCALL SetStaticField)(void *pvThis, jni_type type, jclass clazz, jfieldID fieldID, jvalue val, nsISecurityContext* ctx);
    VFTDELTA_DECL(SetStaticField)
    nsresult (*VFTCALL GetVersion)(void *pvThis, jint* version);
    VFTDELTA_DECL(GetVersion)
    nsresult (*VFTCALL DefineClass)(void *pvThis, const char* name, jobject loader, const jbyte *buf, jsize len, jclass* clazz);
    VFTDELTA_DECL(DefineClass)
    nsresult (*VFTCALL FindClass)(void *pvThis, const char* name, jclass* clazz);
    VFTDELTA_DECL(FindClass)
    nsresult (*VFTCALL GetSuperclass)(void *pvThis, jclass sub, jclass* super);
    VFTDELTA_DECL(GetSuperclass)
    nsresult (*VFTCALL IsAssignableFrom)(void *pvThis, jclass sub, jclass super, jboolean* result);
    VFTDELTA_DECL(IsAssignableFrom)
    nsresult (*VFTCALL Throw)(void *pvThis, jthrowable obj, jint* result);
    VFTDELTA_DECL(Throw)
    nsresult (*VFTCALL ThrowNew)(void *pvThis, jclass clazz, const char *msg, jint* result);
    VFTDELTA_DECL(ThrowNew)
    nsresult (*VFTCALL ExceptionOccurred)(void *pvThis,   jthrowable* result);
    VFTDELTA_DECL(ExceptionOccurred)
    nsresult (*VFTCALL ExceptionDescribe)(void *pvThis);
    VFTDELTA_DECL(ExceptionDescribe)
    nsresult (*VFTCALL ExceptionClear)(void *pvThis);
    VFTDELTA_DECL(ExceptionClear)
    nsresult (*VFTCALL FatalError)(void *pvThis, const char* msg);
    VFTDELTA_DECL(FatalError)
    nsresult (*VFTCALL NewGlobalRef)(void *pvThis, jobject lobj, jobject* result);
    VFTDELTA_DECL(NewGlobalRef)
    nsresult (*VFTCALL DeleteGlobalRef)(void *pvThis, jobject gref);
    VFTDELTA_DECL(DeleteGlobalRef)
    nsresult (*VFTCALL DeleteLocalRef)(void *pvThis, jobject obj);
    VFTDELTA_DECL(DeleteLocalRef)
    nsresult (*VFTCALL IsSameObject)(void *pvThis, jobject obj1, jobject obj2, jboolean* result);
    VFTDELTA_DECL(IsSameObject)
    nsresult (*VFTCALL AllocObject)(void *pvThis, jclass clazz, jobject* result);
    VFTDELTA_DECL(AllocObject)
    nsresult (*VFTCALL GetObjectClass)(void *pvThis, jobject obj, jclass* result);
    VFTDELTA_DECL(GetObjectClass)
    nsresult (*VFTCALL IsInstanceOf)(void *pvThis, jobject obj, jclass clazz, jboolean* result);
    VFTDELTA_DECL(IsInstanceOf)
    nsresult (*VFTCALL GetMethodID)(void *pvThis, jclass clazz, const char* name, const char* sig, jmethodID* id);
    VFTDELTA_DECL(GetMethodID)
    nsresult (*VFTCALL GetFieldID)(void *pvThis, jclass clazz, const char* name, const char* sig, jfieldID* id);
    VFTDELTA_DECL(GetFieldID)
    nsresult (*VFTCALL GetStaticMethodID)(void *pvThis, jclass clazz, const char* name, const char* sig, jmethodID* id);
    VFTDELTA_DECL(GetStaticMethodID)
    nsresult (*VFTCALL GetStaticFieldID)(void *pvThis, jclass clazz, const char* name, const char* sig, jfieldID* id);
    VFTDELTA_DECL(GetStaticFieldID)
    nsresult (*VFTCALL NewString)(void *pvThis, const jchar* unicode, jsize len, jstring* result);
    VFTDELTA_DECL(NewString)
    nsresult (*VFTCALL GetStringLength)(void *pvThis, jstring str, jsize* result);
    VFTDELTA_DECL(GetStringLength)
    nsresult (*VFTCALL GetStringChars)(void *pvThis, jstring str, jboolean *isCopy, const jchar** result);
    VFTDELTA_DECL(GetStringChars)
    nsresult (*VFTCALL ReleaseStringChars)(void *pvThis, jstring str, const jchar *chars);
    VFTDELTA_DECL(ReleaseStringChars)
    nsresult (*VFTCALL NewStringUTF)(void *pvThis, const char *utf, jstring* result);
    VFTDELTA_DECL(NewStringUTF)
    nsresult (*VFTCALL GetStringUTFLength)(void *pvThis, jstring str, jsize* result);
    VFTDELTA_DECL(GetStringUTFLength)
    nsresult (*VFTCALL GetStringUTFChars)(void *pvThis, jstring str, jboolean *isCopy, const char** result);
    VFTDELTA_DECL(GetStringUTFChars)
    nsresult (*VFTCALL ReleaseStringUTFChars)(void *pvThis, jstring str, const char *chars);
    VFTDELTA_DECL(ReleaseStringUTFChars)
    nsresult (*VFTCALL GetArrayLength)(void *pvThis, jarray array, jsize* result);
    VFTDELTA_DECL(GetArrayLength)
    nsresult (*VFTCALL NewObjectArray)(void *pvThis, jsize len, jclass clazz, jobject init, jobjectArray* result);
    VFTDELTA_DECL(NewObjectArray)
    nsresult (*VFTCALL GetObjectArrayElement)(void *pvThis, jobjectArray array, jsize index, jobject* result);
    VFTDELTA_DECL(GetObjectArrayElement)
    nsresult (*VFTCALL SetObjectArrayElement)(void *pvThis, jobjectArray array, jsize index, jobject val);
    VFTDELTA_DECL(SetObjectArrayElement)
    nsresult (*VFTCALL NewArray)(void *pvThis, jni_type element_type, jsize len, jarray* result);
    VFTDELTA_DECL(NewArray)
    nsresult (*VFTCALL GetArrayElements)(void *pvThis, jni_type type, jarray array, jboolean *isCopy, void* result);
    VFTDELTA_DECL(GetArrayElements)
    nsresult (*VFTCALL ReleaseArrayElements)(void *pvThis, jni_type type, jarray array,  void *elems, jint mode);
    VFTDELTA_DECL(ReleaseArrayElements)
    nsresult (*VFTCALL GetArrayRegion)(void *pvThis, jni_type type, jarray array, jsize start, jsize len, void* buf);
    VFTDELTA_DECL(GetArrayRegion)
    nsresult (*VFTCALL SetArrayRegion)(void *pvThis, jni_type type, jarray array, jsize start, jsize len, void* buf);
    VFTDELTA_DECL(SetArrayRegion)
    nsresult (*VFTCALL RegisterNatives)(void *pvThis, jclass clazz, const JNINativeMethod *methods, jint nMethods, jint* result);
    VFTDELTA_DECL(RegisterNatives)
    nsresult (*VFTCALL UnregisterNatives)(void *pvThis, jclass clazz, jint* result);
    VFTDELTA_DECL(UnregisterNatives)
    nsresult (*VFTCALL MonitorEnter)(void *pvThis, jobject obj, jint* result);
    VFTDELTA_DECL(MonitorEnter)
    nsresult (*VFTCALL MonitorExit)(void *pvThis, jobject obj, jint* result);
    VFTDELTA_DECL(MonitorExit)
    nsresult (*VFTCALL GetJavaVM)(void *pvThis, JavaVM **vm, jint* result);
    VFTDELTA_DECL(GetJavaVM)
} VFTnsISecureEnv;


/**
 * nsIPluginInstance
 */
typedef struct vftable_nsIPluginInstance
{
    VFTnsISupports  base;
    nsresult (*VFTCALL Initialize)(void *pvThis, nsIPluginInstancePeer *aPeer);
    VFTDELTA_DECL(Initialize)
    nsresult (*VFTCALL GetPeer)(void *pvThis, nsIPluginInstancePeer * *aPeer);
    VFTDELTA_DECL(GetPeer)
    nsresult (*VFTCALL Start)(void *pvThis);
    VFTDELTA_DECL(Start)
    nsresult (*VFTCALL Stop)(void *pvThis);
    VFTDELTA_DECL(Stop)
    nsresult (*VFTCALL Destroy)(void *pvThis);
    VFTDELTA_DECL(Destroy)
    nsresult (*VFTCALL SetWindow)(void *pvThis, nsPluginWindow * aWindow);
    VFTDELTA_DECL(SetWindow)
    nsresult (*VFTCALL NewStream)(void *pvThis, nsIPluginStreamListener **aListener);
    VFTDELTA_DECL(NewStream)
    nsresult (*VFTCALL Print)(void *pvThis, nsPluginPrint * aPlatformPrint);
    VFTDELTA_DECL(Print)
    nsresult (*VFTCALL GetValue)(void *pvThis, nsPluginInstanceVariable aVariable, void * aValue);
    VFTDELTA_DECL(GetValue)
    nsresult (*VFTCALL HandleEvent)(void *pvThis, nsPluginEvent * aEvent, PRBool *aHandled);
    VFTDELTA_DECL(HandleEvent)
} VFTnsIPluginInstance;


/**
 * nsIPluginInstancePeer
 */
typedef struct vftable_nsIPluginInstancePeer
{
    VFTnsISupports  base;
    nsresult (*VFTCALL GetValue)(void *pvThis, nsPluginInstancePeerVariable aVariable, void * aValue);
    VFTDELTA_DECL(GetValue)
    nsresult (*VFTCALL GetMIMEType)(void *pvThis, nsMIMEType *aMIMEType);
    VFTDELTA_DECL(GetMIMEType)
    nsresult (*VFTCALL GetMode)(void *pvThis, nsPluginMode *aMode);
    VFTDELTA_DECL(GetMode)
    nsresult (*VFTCALL NewStream)(void *pvThis, nsMIMEType aType, const char *aTarget, nsIOutputStream **aResult);
    VFTDELTA_DECL(NewStream)
    nsresult (*VFTCALL ShowStatus)(void *pvThis, const char *aMessage);
    VFTDELTA_DECL(ShowStatus)
    nsresult (*VFTCALL SetWindowSize)(void *pvThis, PRUint32 aWidth, PRUint32 aHeight);
    VFTDELTA_DECL(SetWindowSize)
} VFTnsIPluginInstancePeer;


/**
 * nsIPluginInstancePeer2
 */
typedef struct vftable_nsIPluginInstancePeer2
{
    VFTnsIPluginInstancePeer  base;
    nsresult (*VFTCALL GetJSWindow)(void *pvThis, JSObject * *aJSWindow);
    VFTDELTA_DECL(GetJSWindow)
    nsresult (*VFTCALL GetJSThread)(void *pvThis, PRUint32 *aJSThread);
    VFTDELTA_DECL(GetJSThread)
    nsresult (*VFTCALL GetJSContext)(void *pvThis, JSContext * *aJSContext);
    VFTDELTA_DECL(GetJSContext)
} VFTnsIPluginInstancePeer2;


/**
 * nsIInputStream
 */
typedef struct vftable_nsIInputStream
{
    VFTnsISupports  base;
    nsresult (*VFTCALL Close)(void *pvThis);
    VFTDELTA_DECL(Close)
    nsresult (*VFTCALL Available)(void *pvThis, PRUint32 *_retval);
    VFTDELTA_DECL(Available)
    nsresult (*VFTCALL Read)(void *pvThis, char * aBuf, PRUint32 aCount, PRUint32 *_retval);
    VFTDELTA_DECL(Read)
    nsresult (*VFTCALL ReadSegments)(void *pvThis, nsWriteSegmentFun aWriter, void * aClosure, PRUint32 aCount, PRUint32 *_retval);
    VFTDELTA_DECL(ReadSegments)
    nsresult (*VFTCALL IsNonBlocking)(void *pvThis, PRBool *_retval);
    VFTDELTA_DECL(IsNonBlocking)
} VFTnsIInputStream;


/**
 * nsIOutputStream
 */
typedef struct vftable_nsIOutputStream
{
    VFTnsISupports  base;
    nsresult (*VFTCALL Close)(void *pvThis);
    VFTDELTA_DECL(Close)
    nsresult (*VFTCALL Flush)(void *pvThis);
    VFTDELTA_DECL(Flush)
    nsresult (*VFTCALL Write)(void *pvThis, const char *aBuf, PRUint32 aCount, PRUint32 *_retval);
    VFTDELTA_DECL(Write)
    nsresult (*VFTCALL WriteFrom)(void *pvThis, nsIInputStream *aFromStream, PRUint32 aCount, PRUint32 *_retval);
    VFTDELTA_DECL(WriteFrom)
    nsresult (*VFTCALL WriteSegments)(void *pvThis, nsReadSegmentFun aReader, void * aClosure, PRUint32 aCount, PRUint32 *_retval);
    VFTDELTA_DECL(WriteSegments)
    nsresult (*VFTCALL IsNonBlocking)(void *pvThis, PRBool *_retval);
    VFTDELTA_DECL(IsNonBlocking)
} VFTnsIOutputStream;



/**
 * nsIPluginTagInfo
 */
typedef struct vftable_nsIPluginTagInfo
{
    VFTnsISupports  base;
    nsresult (*VFTCALL GetAttributes)(void *pvThis, PRUint16 & aCount, const char* const* & aNames, const char* const* & aValues);
    VFTDELTA_DECL(GetAttributes)
    nsresult (*VFTCALL GetAttribute)(void *pvThis, const char *aName, const char * *aResult);
    VFTDELTA_DECL(GetAttribute)
} VFTnsIPluginTagInfo;

/**
 * nsIPluginTagInfo2
 */
typedef struct vftable_nsIPluginTagInfo2
{
    VFTnsIPluginTagInfo  base;
    nsresult (*VFTCALL GetTagType)(void *pvThis, nsPluginTagType *aTagType);
    VFTDELTA_DECL(GetTagType)
    nsresult (*VFTCALL GetTagText)(void *pvThis, const char * *aTagText);
    VFTDELTA_DECL(GetTagText)
    nsresult (*VFTCALL GetParameters)(void *pvThis, PRUint16 & aCount, const char* const* & aNames, const char* const* & aValues);
    VFTDELTA_DECL(GetParameters)
    nsresult (*VFTCALL GetParameter)(void *pvThis, const char *aName, const char * *aResult);
    VFTDELTA_DECL(GetParameter)
    nsresult (*VFTCALL GetDocumentBase)(void *pvThis, const char * *aDocumentBase);
    VFTDELTA_DECL(GetDocumentBase)
    nsresult (*VFTCALL GetDocumentEncoding)(void *pvThis, const char * *aDocumentEncoding);
    VFTDELTA_DECL(GetDocumentEncoding)
    nsresult (*VFTCALL GetAlignment)(void *pvThis, const char * *aElignment);
    VFTDELTA_DECL(GetAlignment)
    nsresult (*VFTCALL GetWidth)(void *pvThis, PRUint32 *aWidth);
    VFTDELTA_DECL(GetWidth)
    nsresult (*VFTCALL GetHeight)(void *pvThis, PRUint32 *aHeight);
    VFTDELTA_DECL(GetHeight)
    nsresult (*VFTCALL GetBorderVertSpace)(void *pvThis, PRUint32 *aBorderVertSpace);
    VFTDELTA_DECL(GetBorderVertSpace)
    nsresult (*VFTCALL GetBorderHorizSpace)(void *pvThis, PRUint32 *aBorderHorizSpace);
    VFTDELTA_DECL(GetBorderHorizSpace)
    nsresult (*VFTCALL GetUniqueID)(void *pvThis, PRUint32 *aUniqueID);
    VFTDELTA_DECL(GetUniqueID)
    nsresult (*VFTCALL GetDOMElement)(void *pvThis, nsIDOMElement * *aDOMElement);
    VFTDELTA_DECL(GetDOMElement)
} VFTnsIPluginTagInfo2;


/**
 * nsIJVMWindow
 */
typedef struct vftable_nsIJVMWindow
{
    VFTnsISupports  base;
    nsresult (*VFTCALL Show)(void *pvThis);
    VFTDELTA_DECL(Show)
    nsresult (*VFTCALL Hide)(void *pvThis);
    VFTDELTA_DECL(Hide)
    nsresult (*VFTCALL IsVisible)(void *pvThis, PRBool *result);
    VFTDELTA_DECL(IsVisible)
} VFTnsIJVMWindow;


/**
 * nsIJVMConsole
 */
typedef struct vftable_nsIJVMConsole
{
    VFTnsIJVMWindow  base;
    /* This these are not duplicated even if duplicated in the interface.
    nsresult (*VFTCALL Show)(void *pvThis);
    nsresult (*VFTCALL Hide)(void *pvThis);
    nsresult (*VFTCALL IsVisible)(void *pvThis, PRBool *result);
    */
    nsresult (*VFTCALL Print)(void *pvThis, const char* msg, const char* encodingName);
    VFTDELTA_DECL(Print)
} VFTnsIJVMConsole;


/**
 * nsIJVMPluginInstance
 */
typedef struct vftable_nsIJVMPluginInstance
{
    VFTnsISupports  base;
    nsresult (*VFTCALL GetJavaObject)(void *pvThis, jobject *result);
    VFTDELTA_DECL(GetJavaObject)
    nsresult (*VFTCALL GetText)(void *pvThis, const char ** result);
    VFTDELTA_DECL(GetText)
} VFTnsIJVMPluginInstance;


/**
 * nsIPluginManager2
 */
typedef struct vtable_nsIPluginManager2
{
    VFTnsIPluginManager base;
    nsresult (*VFTCALL BeginWaitCursor)(void *pvThis);
    VFTDELTA_DECL(BeginWaitCursor)
    nsresult (*VFTCALL EndWaitCursor)(void *pvThis);
    VFTDELTA_DECL(EndWaitCursor)
    nsresult (*VFTCALL SupportsURLProtocol)(void *pvThis, const char *aProtocol, PRBool *aResult);
    VFTDELTA_DECL(SupportsURLProtocol)
    nsresult (*VFTCALL NotifyStatusChange)(void *pvThis, nsIPlugin *aPlugin, nsresult aStatus);
    VFTDELTA_DECL(NotifyStatusChange)
    nsresult (*VFTCALL FindProxyForURL)(void *pvThis, const char *aURL, char **aResult);
    VFTDELTA_DECL(FindProxyForURL)
    nsresult (*VFTCALL RegisterWindow)(void *pvThis, nsIEventHandler *aHandler, nsPluginPlatformWindowRef aWindow);
    VFTDELTA_DECL(RegisterWindow)
    nsresult (*VFTCALL UnregisterWindow)(void *pvThis, nsIEventHandler *aHandler, nsPluginPlatformWindowRef aWindow);
    VFTDELTA_DECL(UnregisterWindow)
    nsresult (*VFTCALL AllocateMenuID)(void *pvThis, nsIEventHandler *aHandler, PRBool aIsSubmenu, PRInt16 *aResult);
    VFTDELTA_DECL(AllocateMenuID)
    nsresult (*VFTCALL DeallocateMenuID)(void *pvThis, nsIEventHandler *aHandler, PRInt16 aMenuID);
    VFTDELTA_DECL(DeallocateMenuID)
    nsresult (*VFTCALL HasAllocatedMenuID)(void *pvThis, nsIEventHandler *aHandler, PRInt16 aMenuID, PRBool *aResult);
    VFTDELTA_DECL(HasAllocatedMenuID)
} VFTnsIPluginManager2;


/**
 * nsICookieStorage
 */
typedef struct vtable_nsICookieStorage
{
    VFTnsISupports base;
    nsresult (*VFTCALL GetCookie)(void *pvThis, const char *aCookieURL, void * aCookieBuffer, PRUint32 & aCookieSize);
    VFTDELTA_DECL(GetCookie)
    nsresult (*VFTCALL SetCookie)(void *pvThis, const char *aCookieURL, const void * aCookieBuffer, PRUint32 aCookieSize);
    VFTDELTA_DECL(SetCookie)
} VFTnsICookieStorage;


/**
 * nsIEventHandler
 */
typedef struct vtable_nsIEventHandler
{
    VFTnsISupports base;
    nsresult (*VFTCALL HandleEvent)(void *pvThis, nsPluginEvent * aEvent, PRBool *aHandled);
    VFTDELTA_DECL(HandleEvent)
} VFTnsIEventHandler;


/**
 * nsIJVMThreadManager
 */
typedef struct vtable_nsIJVMThreadManager
{
    VFTnsISupports base;
#ifdef IPLUGINW_OUTOFTREE
    nsresult (*VFTCALL GetCurrentThread)(void *pvThis, nsPluginThread* *threadID);
#else
    nsresult (*VFTCALL GetCurrentThread)(void *pvThis, PRThread **threadID);
#endif
    VFTDELTA_DECL(GetCurrentThread)
    nsresult (*VFTCALL Sleep)(void *pvThis, PRUint32 milli);
    VFTDELTA_DECL(Sleep)
    nsresult (*VFTCALL EnterMonitor)(void *pvThis, void* address);
    VFTDELTA_DECL(EnterMonitor)
    nsresult (*VFTCALL ExitMonitor)(void *pvThis, void* address);
    VFTDELTA_DECL(ExitMonitor)
    nsresult (*VFTCALL Wait)(void *pvThis, void* address, PRUint32 milli);
    VFTDELTA_DECL(Wait)
    nsresult (*VFTCALL Notify)(void *pvThis, void* address);
    VFTDELTA_DECL(Notify)
    nsresult (*VFTCALL NotifyAll)(void *pvThis, void* address);
    VFTDELTA_DECL(NotifyAll)
#ifdef IPLUGINW_OUTOFTREE
    nsresult (*VFTCALL CreateThread)(void *pvThis, PRUint32* threadID, nsIRunnable* runnable);
#else
    nsresult (*VFTCALL CreateThread)(void *pvThis, PRThread **threadID, nsIRunnable* runnable);
#endif
    VFTDELTA_DECL(CreateThread)
#ifdef IPLUGINW_OUTOFTREE
    nsresult (*VFTCALL PostEvent)(void *pvThis, PRUint32 threadID, nsIRunnable* runnable, PRBool async);
#else
    nsresult (*VFTCALL PostEvent)(void *pvThis, PRThread *threadID, nsIRunnable* runnable, PRBool async);
#endif
    VFTDELTA_DECL(PostEvent)
} VFTnsIJVMThreadManager;


/**
 * nsIJVMManager
 */
typedef struct vtable_nsIJVMManager
{
    VFTnsISupports base;
    nsresult (*VFTCALL CreateProxyJNI)(void *pvThis, nsISecureEnv *secureEnv, JNIEnv * *outProxyEnv);
    VFTDELTA_DECL(CreateProxyJNI)
    nsresult (*VFTCALL GetProxyJNI)(void *pvThis, JNIEnv * *outProxyEnv);
    VFTDELTA_DECL(GetProxyJNI)
    nsresult (*VFTCALL ShowJavaConsole)(void *pvThis);
    VFTDELTA_DECL(ShowJavaConsole)
    nsresult (*VFTCALL IsAllPermissionGranted)(void *pvThis, const char *lastFingerprint, const char *lastCommonName, const char *rootFingerprint, const char *rootCommonName, PRBool *_retval);
    VFTDELTA_DECL(IsAllPermissionGranted)
    nsresult (*VFTCALL IsAppletTrusted)(void *pvThis, const char *aRSABuf, PRUint32 aRSABufLen, const char *aPlaintext, PRUint32 aPlaintextLen, PRBool *isTrusted, nsIPrincipal **_retval);
    VFTDELTA_DECL(IsAppletTrusted)
    nsresult (*VFTCALL GetJavaEnabled)(void *pvThis, PRBool *aJavaEnabled);
    VFTDELTA_DECL(GetJavaEnabled)
} VFTnsIJVMManager;


/**
 * nsIRunnable
 */
typedef struct vtable_nsIRunnable
{
    VFTnsISupports base;
    nsresult (*VFTCALL Run)(void *pvThis);
    VFTDELTA_DECL(Run)
} VFTnsIRunnable;


/**
 * nsISecurityContext
 */
typedef struct vftable_nsISecurityContext
{
    VFTnsISupports  base;
    nsresult (*VFTCALL Implies)(void *pvThis, const char* target, const char* action, PRBool *bAllowedAccess);
    VFTDELTA_DECL(Implies)
    nsresult (*VFTCALL GetOrigin)(void *pvThis, char* buf, int len);
    VFTDELTA_DECL(GetOrigin)
    nsresult (*VFTCALL GetCertificateID)(void *pvThis, char* buf, int len);
    VFTDELTA_DECL(GetCertificateID)
} VFTnsISecurityContext;


/**
 * nsIRequest
 */
typedef struct vftable_nsIRequest
{
    VFTnsISupports  base;
    nsresult (*VFTCALL GetName)(void *pvThis, nsACString & aName);
    VFTDELTA_DECL(GetName)
    nsresult (*VFTCALL IsPending)(void *pvThis, PRBool *_retval);
    VFTDELTA_DECL(IsPending)
    nsresult (*VFTCALL GetStatus)(void *pvThis, nsresult *aStatus);
    VFTDELTA_DECL(GetStatus)
    nsresult (*VFTCALL Cancel)(void *pvThis, nsresult aStatus);
    VFTDELTA_DECL(Cancel)
    nsresult (*VFTCALL Suspend)(void *pvThis);
    VFTDELTA_DECL(Suspend)
    nsresult (*VFTCALL Resume)(void *pvThis);
    VFTDELTA_DECL(Resume)
    nsresult (*VFTCALL GetLoadGroup)(void *pvThis, nsILoadGroup * *aLoadGroup);
    VFTDELTA_DECL(GetLoadGroup)
    nsresult (*VFTCALL SetLoadGroup)(void *pvThis, nsILoadGroup * aLoadGroup);
    VFTDELTA_DECL(SetLoadGroup)
    nsresult (*VFTCALL GetLoadFlags)(void *pvThis, nsLoadFlags *aLoadFlags);
    VFTDELTA_DECL(GetLoadFlags)
    nsresult (*VFTCALL SetLoadFlags)(void *pvThis, nsLoadFlags aLoadFlags);
    VFTDELTA_DECL(SetLoadFlags)
} VFTnsIRequest;


/**
 * nsILoadGroup
 */
typedef struct vftable_nsILoadGroup
{
    VFTnsIRequest   base;
    nsresult (*VFTCALL GetGroupObserver)(void *pvThis, nsIRequestObserver * *aGroupObserver);
    VFTDELTA_DECL(GetGroupObserver)
    nsresult (*VFTCALL SetGroupObserver)(void *pvThis, nsIRequestObserver * aGroupObserver);
    VFTDELTA_DECL(SetGroupObserver)
    nsresult (*VFTCALL GetDefaultLoadRequest)(void *pvThis, nsIRequest * *aDefaultLoadRequest);
    VFTDELTA_DECL(GetDefaultLoadRequest)
    nsresult (*VFTCALL SetDefaultLoadRequest)(void *pvThis, nsIRequest * aDefaultLoadRequest);
    VFTDELTA_DECL(SetDefaultLoadRequest)
    nsresult (*VFTCALL AddRequest)(void *pvThis, nsIRequest *aRequest, nsISupports *aContext);
    VFTDELTA_DECL(AddRequest)
    nsresult (*VFTCALL RemoveRequest)(void *pvThis, nsIRequest *aRequest, nsISupports *aContext, nsresult aStatus);
    VFTDELTA_DECL(RemoveRequest)
    nsresult (*VFTCALL GetRequests)(void *pvThis, nsISimpleEnumerator * *aRequests);
    VFTDELTA_DECL(GetRequests)
    nsresult (*VFTCALL GetActiveCount)(void *pvThis, PRUint32 *aActiveCount);
    VFTDELTA_DECL(GetActiveCount)
    nsresult (*VFTCALL GetNotificationCallbacks)(void *pvThis, nsIInterfaceRequestor * *aNotificationCallbacks);
    VFTDELTA_DECL(GetNotificationCallbacks)
    nsresult (*VFTCALL SetNotificationCallbacks)(void *pvThis, nsIInterfaceRequestor * aNotificationCallbacks);
    VFTDELTA_DECL(SetNotificationCallbacks)
} VFTnsILoadGroup;


/**
 * nsIRequestObserver
 */
typedef struct vftable_nsIRequestObserver
{
    VFTnsISupports  base;
    nsresult (*VFTCALL OnStartRequest)(void *pvThis, nsIRequest *aRequest, nsISupports *aContext);
    VFTDELTA_DECL(OnStartRequest)
    nsresult (*VFTCALL OnStopRequest)(void *pvThis, nsIRequest *aRequest, nsISupports *aContext, nsresult aStatusCode);
    VFTDELTA_DECL(OnStopRequest)
} VFTnsIRequestObserver;


/**
 * nsIStreamListener
 */
typedef struct vftable_nsIStreamListener
{
    VFTnsIRequestObserver  base;
    nsresult (*VFTCALL OnDataAvailable)(void *pvThis, nsIRequest *aRequest, nsISupports *aContext, nsIInputStream *aInputStream, PRUint32 aOffset, PRUint32 aCount);
    VFTDELTA_DECL(OnDataAvailable)
} VFTnsIStreamListener;


/**
 * nsISimpleEnumerator
 */
typedef struct vftable_nsISimpleEnumerator
{
    VFTnsISupports  base;
    nsresult (*VFTCALL HasMoreElements)(void *pvThis, PRBool *_retval);
    VFTDELTA_DECL(HasMoreElements)
    nsresult (*VFTCALL GetNext)(void *pvThis, nsISupports **_retval);
    VFTDELTA_DECL(GetNext)
} VFTnsISimpleEnumerator;


/**
 * nsIInterfaceRequestor
 */
typedef struct vftable_nsIInterfaceRequestor
{
    VFTnsISupports  base;
    nsresult (*VFTCALL GetInterface)(void *pvThis, const nsIID & uuid, void * *result);
    VFTDELTA_DECL(GetInterface)
} VFTnsIInterfaceRequestor;


/**
 * nsIPluginStreamListener
 */
typedef struct vftable_nsIPluginStreamListener
{
    VFTnsISupports  base;
    nsresult (*VFTCALL OnStartBinding)(void *pvThis, nsIPluginStreamInfo *aPluginInfo);
    VFTDELTA_DECL(OnStartBinding)
    nsresult (*VFTCALL OnDataAvailable)(void *pvThis, nsIPluginStreamInfo *aPluginInfo, nsIInputStream *aInputStream, PRUint32 aLength);
    VFTDELTA_DECL(OnDataAvailable)
    nsresult (*VFTCALL OnFileAvailable)(void *pvThis, nsIPluginStreamInfo *aPluginInfo, const char *aFileName);
    VFTDELTA_DECL(OnFileAvailable)
    nsresult (*VFTCALL OnStopBinding)(void *pvThis, nsIPluginStreamInfo *aPluginInfo, nsresult aStatus);
    VFTDELTA_DECL(OnStopBinding)
    nsresult (*VFTCALL GetStreamType)(void *pvThis, nsPluginStreamType *aStreamType);
    VFTDELTA_DECL(GetStreamType)
} VFTnsIPluginStreamListener;



/**
 * nsIEnumerator
 */
typedef struct vftable_nsIEnumerator
{
    VFTnsISupports  base;
    nsresult (*VFTCALL First)(void *pvThis);
    VFTDELTA_DECL(First)
    nsresult (*VFTCALL Next)(void *pvThis);
    VFTDELTA_DECL(Next)
    nsresult (*VFTCALL CurrentItem)(void *pvThis, nsISupports **_retval);
    VFTDELTA_DECL(CurrentItem)
    nsresult (*VFTCALL IsDone)(void *pvThis);
    VFTDELTA_DECL(IsDone)
} VFTnsIEnumerator;


/**
 * nsIPluginStreamInfo
 */
typedef struct vftable_nsIPluginStreamInfo
{
    VFTnsISupports  base;
    nsresult (*VFTCALL GetContentType)(void *pvThis, nsMIMEType *aContentType);
    VFTDELTA_DECL(GetContentType)
    nsresult (*VFTCALL IsSeekable)(void *pvThis, PRBool *aSeekable);
    VFTDELTA_DECL(IsSeekable)
    nsresult (*VFTCALL GetLength)(void *pvThis, PRUint32 *aLength);
    VFTDELTA_DECL(GetLength)
    nsresult (*VFTCALL GetLastModified)(void *pvThis, PRUint32 *aLastModified);
    VFTDELTA_DECL(GetLastModified)
    nsresult (*VFTCALL GetURL)(void *pvThis, const char * *aURL);
    VFTDELTA_DECL(GetURL)
    nsresult (*VFTCALL RequestRead)(void *pvThis, nsByteRange * aRangeList);
    VFTDELTA_DECL(RequestRead)
    nsresult (*VFTCALL GetStreamOffset)(void *pvThis, PRInt32 *aStreamOffset);
    VFTDELTA_DECL(GetStreamOffset)
    nsresult (*VFTCALL SetStreamOffset)(void *pvThis, PRInt32 aStreamOffset);
    VFTDELTA_DECL(SetStreamOffset)
} VFTnsIPluginStreamInfo;


class FlashIObject7;
/**
 * FlashIObject (42b1d5a4-6c2b-11d6-8063-0005029bc257)
 * Flash version 7r14
 */
typedef struct vftable_FlashIObject7
{
    VFTnsISupports  base;
    nsresult (*VFTCALL Evaluate)(void *pvThis, const char *aString, FlashIObject7 ** aFlashObject);
    VFTDELTA_DECL(Evaluate)
} VFTFlashIObject7;


/**
 * FlashIScriptablePlugin (d458fe9c-518c-11d6-84cb-0005029bc257)
 * Flash version 7r14
 */
typedef struct vftable_FlashIScriptablePlugin7
{
    VFTnsISupports  base;
    nsresult (*VFTCALL IsPlaying)(void *pvThis, PRBool *aretval);
    VFTDELTA_DECL(IsPlaying)
    nsresult (*VFTCALL Play)(void *pvThis);
    VFTDELTA_DECL(Play)
    nsresult (*VFTCALL StopPlay)(void *pvThis);
    VFTDELTA_DECL(StopPlay)
    nsresult (*VFTCALL TotalFrames)(void *pvThis, PRInt32 *aretval);
    VFTDELTA_DECL(TotalFrames)
    nsresult (*VFTCALL CurrentFrame)(void *pvThis, PRInt32 *aretval);
    VFTDELTA_DECL(CurrentFrame)
    nsresult (*VFTCALL GotoFrame)(void *pvThis, PRInt32 aFrame);
    VFTDELTA_DECL(GotoFrame)
    nsresult (*VFTCALL Rewind)(void *pvThis);
    VFTDELTA_DECL(Rewind)
    nsresult (*VFTCALL Back)(void *pvThis);
    VFTDELTA_DECL(Back)
    nsresult (*VFTCALL Forward)(void *pvThis);
    VFTDELTA_DECL(Forward)
    nsresult (*VFTCALL Pan)(void *pvThis, PRInt32 aX, PRInt32 aY, PRInt32 aMode);
    VFTDELTA_DECL(Pan)
    nsresult (*VFTCALL PercentLoaded)(void *pvThis, PRInt32 *aretval);
    VFTDELTA_DECL(PercentLoaded)
    nsresult (*VFTCALL FrameLoaded)(void *pvThis, PRInt32 aFrame, PRBool *aretval);
    VFTDELTA_DECL(FrameLoaded)
    nsresult (*VFTCALL FlashVersion)(void *pvThis, PRInt32 *aretval);
    VFTDELTA_DECL(FlashVersion)
    nsresult (*VFTCALL Zoom)(void *pvThis, PRInt32 aZoom);
    VFTDELTA_DECL(Zoom)
    nsresult (*VFTCALL SetZoomRect)(void *pvThis, PRInt32 aLeft, PRInt32 aTop, PRInt32 aRight, PRInt32 aBottom);
    VFTDELTA_DECL(SetZoomRect)
    nsresult (*VFTCALL LoadMovie)(void *pvThis, PRInt32 aLayer, PRUnichar *aURL);
    VFTDELTA_DECL(LoadMovie)
    nsresult (*VFTCALL TGotoFrame)(void *pvThis, PRUnichar *aTarget, PRInt32 aFrameNumber);
    VFTDELTA_DECL(TGotoFrame)
    nsresult (*VFTCALL TGotoLabel)(void *pvThis, PRUnichar *aTarget, PRUnichar *aLabel);
    VFTDELTA_DECL(TGotoLabel)
    nsresult (*VFTCALL TCurrentFrame)(void *pvThis, PRUnichar *aTarget, PRInt32 *aretval);
    VFTDELTA_DECL(TCurrentFrame)
    /* ??? wonder if this is right! */
    nsresult (*VFTCALL TCurrentLabel)(void *pvThis, PRUnichar *aTarget, PRUnichar **aretval);
    VFTDELTA_DECL(TCurrentLabel)
    nsresult (*VFTCALL TPlay)(void *pvThis, PRUnichar *aTarget);
    VFTDELTA_DECL(TPlay)
    nsresult (*VFTCALL TStopPlay)(void *pvThis, PRUnichar *aTarget);
    VFTDELTA_DECL(TStopPlay)
    nsresult (*VFTCALL SetVariable)(void *pvThis, PRUnichar *aVariable, PRUnichar *aValue);
    VFTDELTA_DECL(SetVariable)
    nsresult (*VFTCALL GetVariable)(void *pvThis, PRUnichar *aVariable, PRUnichar **aretval);
    VFTDELTA_DECL(GetVariable)
    nsresult (*VFTCALL TSetProperty)(void *pvThis, PRUnichar *aTarget, PRInt32 aProperty, PRUnichar *aValue);
    VFTDELTA_DECL(TSetProperty)
    nsresult (*VFTCALL TGetProperty)(void *pvThis, PRUnichar *aTarget, PRInt32 aProperty, PRUnichar **aretval);
    VFTDELTA_DECL(TGetProperty)
    /* ??? check this!? */
    nsresult (*VFTCALL TGetPropertyAsNumber)(void *pvThis, PRUnichar *aTarget, PRInt32 aProperty, double **aretval);
    VFTDELTA_DECL(TGetPropertyAsNumber)
    nsresult (*VFTCALL TCallLabel)(void *pvThis, PRUnichar *aTarget, PRUnichar *aLabel);
    VFTDELTA_DECL(TCallLabel)
    nsresult (*VFTCALL TCallFrame)(void *pvThis, PRUnichar *aTarget, PRInt32 aFrameNumber);
    VFTDELTA_DECL(TCallFrame)
    nsresult (*VFTCALL SetWindow)(void *pvThis, FlashIObject7 *aFlashObject, PRInt32 a1);
    VFTDELTA_DECL(SetWindow)
} VFTFlashIScriptablePlugin7;


/**
 * nsIPlugin5 (d27cdb6e-ae6d-11cf-96b8-444553540000)
 * Flash 5 rXY for OS/2
 */
typedef struct vftable_nsIFlash5
{
    VFTnsISupports  base;
    nsresult (*VFTCALL IsPlaying)(void *pvThis, PRBool *aretval);
    VFTDELTA_DECL(IsPlaying)
    nsresult (*VFTCALL Play)(void *pvThis);
    VFTDELTA_DECL(Play)
    nsresult (*VFTCALL StopPlay)(void *pvThis);
    VFTDELTA_DECL(StopPlay)
    nsresult (*VFTCALL TotalFrames)(void *pvThis, PRInt32 *aretval);
    VFTDELTA_DECL(TotalFrames)
    nsresult (*VFTCALL CurrentFrame)(void *pvThis, PRInt32 *aretval);
    VFTDELTA_DECL(CurrentFrame)
    nsresult (*VFTCALL GotoFrame)(void *pvThis, PRInt32 aPosition);
    VFTDELTA_DECL(GotoFrame)
    nsresult (*VFTCALL Rewind)(void *pvThis);
    VFTDELTA_DECL(Rewind)
    nsresult (*VFTCALL Back)(void *pvThis);
    VFTDELTA_DECL(Back)
    nsresult (*VFTCALL Forward)(void *pvThis);
    VFTDELTA_DECL(Forward)
    nsresult (*VFTCALL PercentLoaded)(void *pvThis, PRInt32 *aretval);
    VFTDELTA_DECL(PercentLoaded)
    nsresult (*VFTCALL FrameLoaded)(void *pvThis, PRInt32 aFrame, PRBool *aretval);
    VFTDELTA_DECL(FrameLoaded)
    nsresult (*VFTCALL FlashVersion)(void *pvThis, PRInt32 *aretval);
    VFTDELTA_DECL(FlashVersion)
    nsresult (*VFTCALL Pan)(void *pvThis, PRInt32 aX, PRInt32 aY, PRInt32 aMode);
    VFTDELTA_DECL(Pan)
    nsresult (*VFTCALL Zoom)(void *pvThis, PRInt32 aPercent);
    VFTDELTA_DECL(Zoom)
    nsresult (*VFTCALL SetZoomRect)(void *pvThis, PRInt32 aLeft, PRInt32 aTop, PRInt32 aRight, PRInt32 aBottom);
    VFTDELTA_DECL(SetZoomRect)
    nsresult (*VFTCALL LoadMovie)(void *pvThis, PRInt32 aLayer, const char *aURL);
    VFTDELTA_DECL(LoadMovie)
    nsresult (*VFTCALL TGotoFrame)(void *pvThis, const char *aTarget, PRInt32 frameNum);
    VFTDELTA_DECL(TGotoFrame)
    nsresult (*VFTCALL TGotoLabel)(void *pvThis, const char *aTarget, const char *aLabel);
    VFTDELTA_DECL(TGotoLabel)
    nsresult (*VFTCALL TCurrentFrame)(void *pvThis, const char *aTarget, PRInt32 *aretval);
    VFTDELTA_DECL(TCurrentFrame)
    nsresult (*VFTCALL TCurrentLabel)(void *pvThis, const char *aTarget, char **aretval);
    VFTDELTA_DECL(TCurrentLabel)
    nsresult (*VFTCALL TPlay)(void *pvThis, const char *aTarget);
    VFTDELTA_DECL(TPlay)
    nsresult (*VFTCALL TStopPlay)(void *pvThis, const char *aTarget);
    VFTDELTA_DECL(TStopPlay)
    nsresult (*VFTCALL SetVariable)(void *pvThis, const char *aName, const char *aValue);
    VFTDELTA_DECL(SetVariable)
    nsresult (*VFTCALL GetVariable)(void *pvThis, const char *aName, char **aretval);
    VFTDELTA_DECL(GetVariable)
    nsresult (*VFTCALL TSetProperty)(void *pvThis, const char *aTarget, PRInt32 aProperty, const char *value);
    VFTDELTA_DECL(TSetProperty)
    nsresult (*VFTCALL TGetProperty)(void *pvThis, const char *aTarget, PRInt32 aProperty, char **aretval);
    VFTDELTA_DECL(TGetProperty)
    nsresult (*VFTCALL TCallFrame)(void *pvThis, const char *aTarget, PRInt32 aFrame);
    VFTDELTA_DECL(TCallFrame)
    nsresult (*VFTCALL TCallLabel)(void *pvThis, const char *aTarget, const char *aLabel);
    VFTDELTA_DECL(TCallLabel)
    nsresult (*VFTCALL TGetPropertyAsNumber)(void *pvThis, const char *aTarget, PRInt32 aProperty, double *aretval);
    VFTDELTA_DECL(TGetPropertyAsNumber)
    nsresult (*VFTCALL TSetPropertyAsNumber)(void *pvThis, const char *aTarget, PRInt32 aProperty, double aValue);
    VFTDELTA_DECL(TSetPropertyAsNumber)
} VFTnsIFlash5;



/**
 * nsIClassInfo
 */
typedef struct vftable_nsIClassInfo
{
    VFTnsISupports  base;
    nsresult (*VFTCALL GetInterfaces)(void *pvThis, PRUint32 *count, nsIID * **array);
    VFTDELTA_DECL(GetInterfaces)
    nsresult (*VFTCALL GetHelperForLanguage)(void *pvThis, PRUint32 language, nsISupports **_retval);
    VFTDELTA_DECL(GetHelperForLanguage)
    nsresult (*VFTCALL GetContractID)(void *pvThis, char * *aContractID);
    VFTDELTA_DECL(GetContractID)
    nsresult (*VFTCALL GetClassDescription)(void *pvThis, char * *aClassDescription);
    VFTDELTA_DECL(GetClassDescription)
    nsresult (*VFTCALL GetClassID)(void *pvThis, nsCID * *aClassID);
    VFTDELTA_DECL(GetClassID)
    nsresult (*VFTCALL GetImplementationLanguage)(void *pvThis, PRUint32 *aImplementationLanguage);
    VFTDELTA_DECL(GetImplementationLanguage)
    nsresult (*VFTCALL GetFlags)(void *pvThis, PRUint32 *aFlags);
    VFTDELTA_DECL(GetFlags)
    nsresult (*VFTCALL GetClassIDNoAlloc)(void *pvThis, nsCID *aClassIDNoAlloc);
    VFTDELTA_DECL(GetClassIDNoAlloc)
} VFTnsIClassInfo;


/**
 * nsIHTTPHeaderListener
 */
typedef struct vftable_nsIHTTPHeaderListener
{
    VFTnsISupports  base;
    nsresult (*VFTCALL NewResponseHeader)(void *pvThis, const char *headerName, const char *headerValue);
    VFTDELTA_DECL(NewResponseHeader)
} VFTnsIHTTPHeaderListener;


/**
 * nsIMemory
 */
typedef struct vftable_nsIMemory
{
    VFTnsISupports  base;
    void * (*VFTCALL Alloc)(void *pvThis, size_t size);
    VFTDELTA_DECL(Alloc)
    void * (*VFTCALL Realloc)(void *pvThis, void * ptr, size_t newSize);
    VFTDELTA_DECL(Realloc)
    void (*VFTCALL Free)(void *pvThis, void * ptr);
    VFTDELTA_DECL(Free)
    nsresult (*VFTCALL HeapMinimize)(void *pvThis, PRBool immediate);
    VFTDELTA_DECL(HeapMinimize)
    nsresult (*VFTCALL IsLowMemory)(void *pvThis, PRBool *_retval);
    VFTDELTA_DECL(IsLowMemory)
} VFTnsIMemory;

//@}
#endif
