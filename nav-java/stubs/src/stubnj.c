/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* From mozilla */
#include "lj.h"
#include "java.h"
#include "javasec.h"

/* From nspr */
#include "prthread.h"

/* From softupdt */
#include "softupdt.h"

/* From edtplug */
#include "edtplug.h"

/* for Mocha glue */
#include "jsapi.h"
#include "jsjava.h"

#if !defined (XP_MAC)
#include "netscape_plugin_composer_PluginManager.h"
#include "netscape_plugin_Plugin.h"
#include "netscape_plugin_composer_Composer.h"
#include "netscape_security_PrivilegeManager.h"
#include "netscape_security_PrivilegeTable.h"
#include "netscape_security_Privilege.h"
#include "netscape_security_Target.h"
#include "netscape_security_Zig.h"
#include "netscape_security_Principal.h"
#include "netscape_applet_EmbeddedObjectNatives.h"
#include "netscape_applet_MozillaAppletContext.h"
#include "netscape_javascript_JSObject.h"
#include "netscape_javascript_JSException.h"
#else
#include "n_p_composer_PluginManager.h"
#include "netscape_plugin_Plugin.h"
#include "n_plugin_composer_Composer.h"
#include "n_security_PrivilegeManager.h"
#include "n_security_PrivilegeTable.h"
#include "netscape_security_Privilege.h"
#include "netscape_security_Target.h"
#include "netscape_security_Zig.h"
#include "netscape_security_Principal.h"
#include "n_a_EmbeddedObjectNatives.h"
#include "n_applet_MozillaAppletContext.h"
#include "netscape_javascript_JSObject.h"
#include "n_javascript_JSException.h"
#endif

JRI_PUBLIC_API(int) LJ_JSJ_IsEnabled(void);

/*
static JSJCallbacks jsj_callbacks = {
    LJ_JSJ_IsEnabled,
    LJ_JSJ_CurrentEnv,
    LJ_JSJ_CurrentContext,
    LM_LockJS,
    LM_UnlockJS,
    LJ_JSJ_GetJSClassLoader,
    LJ_JSJ_GetJSPrincipalsFromJavaCaller,
    LJ_JSJ_GetDefaultObject
};
*/
/*
static JSJCallbacks jsj_callbacks = {
    LJ_JSJ_IsEnabled,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};
*/

/* ArrayAlloc                                                                       libjsj.so */
/* AwtRegisterXtAppVars                                                             mozilla.o */
/* CompiledFramePrev                                                                libjsj.so */
/* CreateNewJavaStack                                                               libjsj.so */

/* EDTPLUG_RegisterEditURLCallback                                                  editor.o */
/* ns/modules/edtplug/include/edtplug.h */
/* ns/modules/edtplug/src/edtplugStubs.c */
void
EDTPLUG_RegisterEditURLCallback(EditURLCallback callback)
{
  return;
}


/* ExecuteJava                                                                      libjsj.so */
/* FindClassFromClass                                                               libjsj.so */
/* FindLoadedClass                                                                  libjsj.so */
/* JMCException_Destroy                                                             xfe.o */
/* JRI_GetCurrentEnv                                                                libplug.so */


/* LJ_AddToClassPath                                                                libxfe2.so */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_init.c */
JRI_PUBLIC_API(void) 
LJ_AddToClassPath(char* dirPath) 
{
  return;
}

/* LJ_Applet_GetText                                                                liblay.so */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_run.c */
JRI_PUBLIC_API(char *) 
LJ_Applet_GetText(LJAppletData* ad) 
{ 
  return NULL; 
}

/* LJ_Applet_print                                                                  libxlate.so */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_run.c */
JRI_PUBLIC_API(void) 
LJ_Applet_print(LJAppletData *ad, void* printInfo) 
{
  return;
}


/* LJ_CreateApplet                                                                  liblay.so */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_embed.c */
JRI_PUBLIC_API(void)
LJ_CreateApplet(LO_JavaAppStruct *java, MWContext *cx,
		NET_ReloadMethod reloadMethod)
{ 
  return; 
}


/* LJ_DeleteSessionData                                                             liblay.so */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_embed.c */
JRI_PUBLIC_API(void) 
LJ_DeleteSessionData(MWContext* cx, void* sdata) 
{ 
  return; 
}


/* LJ_DiscardEventsForContext                                                       xfe.o */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_embed.c */
JRI_PUBLIC_API(void)
LJ_DiscardEventsForContext(MWContext* context) 
{ 
  return; 
}


/* LJ_DisplayJavaApp                                                                lay.o */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_embed.c */
JRI_PUBLIC_API(void)
LJ_DisplayJavaApp(MWContext *context, ...)
{ 
  return; 
}



/* LJ_EnsureJavaEnv                                                                 libplug.so */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_init.c */
JRI_PUBLIC_API(JRIEnv *)
LJ_EnsureJavaEnv(PRThread *thread) 
{ 
  return NULL; 
}

/* LJ_FreeJavaAppElement                                                            lay.o */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_embed.c */
JRI_PUBLIC_API(void)
LJ_FreeJavaAppElement(MWContext *context, ...)
{ 
  return; 
}


/* LJ_GetAppletScriptOrigin                                                         libmocha.so */
/* ns/modules/applet/src/lj_run.c */
JRI_PUBLIC_API(char *)
LJ_GetAppletScriptOrigin(JRIEnv *env)
{ 
  return NULL; 
}

/* LJ_GetJavaAppSize                                                                lay.o */
/* ns/modules/applet/src/java.h */
/* ns/modules/applet/src/lj_run.c */
JRI_PUBLIC_API(void)
LJ_GetJavaAppSize(MWContext *context, LO_JavaAppStruct *java_struct,
				  NET_ReloadMethod reloadMethod)
{ 
  return; 
}


/* LJ_GetJavaEnabled                                                                libxfe2.so */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_init.c */
JRI_PUBLIC_API(PRBool)
LJ_GetJavaEnabled(void)
{ 
  return PR_FALSE; 
}


/* LJ_GetMochaWindow                                                                liblay.so */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_run.c */
JRI_PUBLIC_API(struct netscape_javascript_JSObject*)
LJ_GetMochaWindow(MWContext *cx)
{ 
  return NULL; 
}


/* LJ_HandleEvent                                                                   liblay.so */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_embed.c */
JRI_PUBLIC_API(PRBool) 
LJ_HandleEvent(MWContext* context, LO_JavaAppStruct *pJava, void *event)
{ 
  return PR_FALSE; 
}


/* LJ_HideConsole                                                                   commands.o */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_cons.c */
JRI_PUBLIC_API(void)
LJ_HideConsole(void) 
{ 
  return; 
}

/* LJ_HideJavaAppElement                                                            lay.o */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_embed.c */
JRI_PUBLIC_API(void)
LJ_HideJavaAppElement(MWContext *context, ...)
{ 
  return; 
}

/* LJ_IconifyApplets                                                                xfe.o */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_run.c */
JRI_PUBLIC_API(void) 
LJ_IconifyApplets(MWContext* context)
{ 
  return; 
}

/* LJ_InitializeZig                                                                 libmocha.so */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/appletStubs.c */
PR_PUBLIC_API(void *)
LJ_InitializeZig(void *zipPtr)
{ 
  return NULL; 
}


/* LJ_InvokeMethod                                                                  libmocha.so */
/* ns/modules/applet/src/lj_run.c */
jref
LJ_InvokeMethod(jglobal clazzGlobal, ...)
{ 
  return NULL; 
}

/* LJ_JSJ_CurrentEnv                                                                libmocha.so */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_run.c */
JRI_PUBLIC_API(JRIEnv *)
LJ_JSJ_CurrentEnv(JSContext *cx)
{ 
  return NULL; 
}

/* LJ_JSJ_CurrentContext                                                            not referenced */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_run.c */
PR_PUBLIC_API(void *)
LJ_JSJ_CurrentContext(JRIEnv *env, char **errp)
{
  return NULL;
}

/* LJ_JSJ_GetJSClassLoader                                                          not referenced */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_run.c */
JRI_PUBLIC_API(void *)
LJ_JSJ_GetJSClassLoader(void *cx, const char *codebase)
{
  return NULL;
}

/* LJ_JSJ_GetJSPrincipalsFromJavaCaller                                             not referenced */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_run.c */
void * PR_CALLBACK
LJ_JSJ_GetJSPrincipalsFromJavaCaller(void *cx, int callerDepth)
{
  return NULL;
}

/* LJ_JSJ_IsEnabled                                                                 not referenced */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_run.c */
JRI_PUBLIC_API(int)
LJ_JSJ_IsEnabled(void)
{
  return 0;
}

/* LJ_JSJ_Init                                                                      libmocha.so */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_run.c */
JRI_PUBLIC_API(void)
LJ_JSJ_Init()
{ 
  /* JSJ_Init(&jsj_callbacks); */
  return; 
}

/* LJ_LoadFromZipFile                                                               libmocha.so */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/appletStubs.c */
PR_PUBLIC_API(char *)
LJ_LoadFromZipFile(void *zip, char *fn)
{ 
  return NULL; 
}


/* LJ_PrintZigError                                                                 libmocha.so */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/appletStubs.c */
PR_PUBLIC_API(int)
LJ_PrintZigError(int status, void *zigPtr, const char *metafile, char *pathname, 
		 char *errortext)
{ 
  return 0; 
}

/* LJ_SetConsoleShowCallback                                                        mozilla.o */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_cons.c */
JRI_PUBLIC_API(void)
LJ_SetConsoleShowCallback(void (*func)(int on, void* a), void *arg)
{ 
  return; 
}

/* LJ_SetProgramName                                                                mozilla.o */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_init.c */
void
LJ_SetProgramName(const char *name)
{ 
  return; 
}


/* LJ_ShowConsole                                                                   commands.o */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_cons.c */
JRI_PUBLIC_API(void)
LJ_ShowConsole(void) 
{ 
  return; 
}

/* LJ_ShutdownJava                                                                  libxfe2.so */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_init.c */
JRI_PUBLIC_API(LJJavaStatus)
LJ_ShutdownJava(void) 
{ 
  return LJJavaStatus_Disabled; 
}

/* LJ_UniconifyApplets                                                              xfe.o */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_run.c */
JRI_PUBLIC_API(void)
LJ_UniconifyApplets(MWContext* context)
{ 
  return; 
}

/* ??? Is it needed for windows ??? */
/* LJ_StartupJava used in ./cmd/winfe/woohoo.cpp */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_init.c */
JRI_PUBLIC_API(LJJavaStatus)
LJ_StartupJava(void) 
{ 
  return LJJavaStatus_Disabled; 
}


/* MakeClassSticky                                                                  libjsj.so */
/* sun-java */


/* NSN_JavaContextToRealContext                                                     xfe.o */
/* ns/modules/applet/include/java.h */
/* ns/nav-java/netscape/security/netStubs.c */
JRI_PUBLIC_API(MWContext *)
NSN_JavaContextToRealContext(MWContext *cx)
{
  return NULL;
}

/* NSN_RegisterJavaConverter                                                        libnet.so */
/* ns/modules/applet/include/java.h */
/* ns/nav-java/netscape/security/netStubs.c */
JRI_PUBLIC_API(void) 
NSN_RegisterJavaConverter(void)
{
  return;
}

/* PrintToConsole                                                                   libmocha.so */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/appletStubs.c */
JRI_PUBLIC_API(void) PrintToConsole(const char* bytes)
{
  return;
}

#ifdef XP_MAC
#pragma export on
#endif

/* SU_NewStream                                                                     libnet.so */
/* ns/modules/softupdt/include/softupdt.h */
/* ns/modules/softupdt/src/softupdt.c */
XP_Bool SU_StartSoftwareUpdate(MWContext * context, 
			       const char * url, 
			       const char * name, 
			       SoftUpdateCompletionFunction f,
			       void * completionClosure,
                               int32 flags) /* FORCE_INSTALL, SILENT_INSTALL */
{
  return SILENT_INSTALL;
}

/* SU_StartSoftwareUpdate                                                           libmocha.so */
/* ns/modules/softupdt/include/softupdt.h */
/* ns/modules/softupdt/src/softupdt.c */
NET_StreamClass * SU_NewStream (int format_out, void * registration,
				URL_Struct * request, MWContext *context)
{
  return NULL;
}


#ifdef XP_MAC
#pragma export reset
#endif


/* VerifyClassAccess                                                                libjsj.so */
/* sun-java */

/* VerifyFieldAccess                                                                libjsj.so */
/* sun-java */

/* awt_MToolkit_dispatchToWidget                                                    mozilla.o */
/* sun-java */

/* awt_MToolkit_finishModals                                                        mozilla.o */
/* sun-java */

/* classEmbeddedObjectNatives                                                       libmocha.so */
/* ns/modules/applet/include/lj.h */
/* ns/modules/applet/src/lj_init.c */
JRI_PUBLIC_VAR(jglobal)
classEmbeddedObjectNatives = NULL;

/* classMozillaAppletContext                                                        libmocha.so */
/* ns/modules/applet/include/lj.h */
/* ns/modules/applet/src/lj_init.c */
JRI_PUBLIC_VAR(jglobal)
classMozillaAppletContext = NULL;

/* do_execute_java_method                                                           libjsj.so */
/* sun-java */

/* do_execute_java_method_vararg                                                    libjsj.so */
/* sun-java */

/* execute_java_constructor                                                         libjsj.so */
/* sun-java */

/* execute_java_constructor_vararg                                                  libjsj.so */
/* sun-java */

/* is_subclass_of                                                                   libjsj.so */
/* sun-java */

/* java_netscape_security_getPrincipals                                             libsecnav.so */
/* ns/nav-java/netscape/security/javasec.h */
/* ns/nav-java/netscape/security/secStubs.c */
JRI_PUBLIC_API(const char *)
java_netscape_security_getPrincipals(const char *charSetName)
{
  return NULL;
}

/* java_netscape_security_getPrivilegeDescs                                         libsecnav.so */
/* ns/nav-java/netscape/security/javasec.h */
/* ns/nav-java/netscape/security/secStubs.c */
JRI_PUBLIC_API(void)
java_netscape_security_getPrivilegeDescs(const char *charSetName,
                                         char  *prinName, 
                                         char **forever, 
                                         char **session, 
                                         char **denied)
{
  return;
}


/* java_netscape_security_getTargetDetails                                          libsecnav.so */
/* ns/nav-java/netscape/security/javasec.h */
/* ns/nav-java/netscape/security/secStubs.c */
JRI_PUBLIC_API(void)
java_netscape_security_getTargetDetails(const char *charSetName,
                                        char* targetName, 
                                        char** details, 
                                        char **risk)
{
  return;
}

/* java_netscape_security_removePrincipal                                           libsecnav.so */
/* ns/nav-java/netscape/security/javasec.h */
/* ns/nav-java/netscape/security/secStubs.c */
JRI_PUBLIC_API(jint)
java_netscape_security_removePrincipal(const char *charSetName, 
                                       char *prinName)
{
  return 0;
}

/* java_netscape_security_removePrivilege                                           libsecnav.so */
/* ns/nav-java/netscape/security/javasec.h */
/* ns/nav-java/netscape/security/secStubs.c */
JRI_PUBLIC_API(jint)
java_netscape_security_removePrivilege(const char *charSetName, 
                                       char *prinName,
                                       char *targetName)
{
  return 0;
}


/* java_netscape_security_savePrivilege                                             libsecnav.so */
/* ns/nav-java/netscape/security/javasec.h */
/* ns/nav-java/netscape/security/secStubs.c */
JRI_PUBLIC_API(void)
java_netscape_security_savePrivilege(int privilege)
{
  return;
}

/* makeJavaString                                                                   libmocha.so */
/* sun-java */

/* methodID_netscape_applet_EmbeddedObjectNatives_reflectObject                     libmocha.so */
/* ns/modules/applet/src/_jri/netscape_applet_EmbeddedObjectNatives.h */
/* ns/modules/applet/src/_jri/netscape_applet_EmbeddedObjectNatives.c */
JRI_PUBLIC_VAR(JRIMethodID) FAR
methodID_netscape_applet_EmbeddedObjectNatives_reflectObject = JRIUninitialized;

/* methodID_netscape_applet_MozillaAppletContext_reflectApplet_1                    libmocha.so */
/* ns/modules/applet/src/_jri/netscape_applet_MozillaAppletContext.h */
/* ns/modules/applet/src/_jri/netscape_applet_MozillaAppletContext.c */
JRI_PUBLIC_VAR(JRIMethodID) FAR
methodID_netscape_applet_MozillaAppletContext_reflectApplet_1 = JRIUninitialized;

/* native_netscape_security_PrivilegeManager_getClassPrincipalsFromStackUnsafe      libmocha.so */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeManager.h */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeManager.c */
JRI_PUBLIC_API(jref)
native_netscape_security_PrivilegeManager_getClassPrincipalsFromStackUnsafe(JRIEnv* env, struct netscape_security_PrivilegeManager* self, jint a)
{
  return NULL;
}


/* netscape_plugin_Plugin_destroy                                                   libplug.so */
/* ns/modules/applet/src/_jri/netscape_plugin_Plugin.h */
/* ns/modules/applet/src/_jri/netscape_plugin_Plugin.c */
JRI_PUBLIC_API(void)
netscape_plugin_Plugin_destroy(JRIEnv* env, struct netscape_plugin_Plugin* self) 
{
  return;
}

/* netscape_plugin_Plugin_init                                                      libplug.so */
/* ns/modules/applet/src/_jri/netscape_plugin_Plugin.h */
/* ns/modules/applet/src/_jri/netscape_plugin_Plugin.c */
JRI_PUBLIC_API(void)
netscape_plugin_Plugin_init(JRIEnv* env, struct netscape_plugin_Plugin* self) 
{
  return;
}


/* netscape_plugin_Plugin_new                                                       libplug.so */
/* ns/modules/applet/src/_jri/netscape_plugin_Plugin.h */
/* ns/modules/applet/src/_jri/netscape_plugin_Plugin.c */
JRI_PUBLIC_API(struct netscape_plugin_Plugin*)
netscape_plugin_Plugin_new(JRIEnv* env, struct java_lang_Class* clazz) 
{
  return NULL;
}

/* netscape_plugin_composer_Composer_new                                            liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_Composer.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_Composer.c */
JRI_PUBLIC_API(struct netscape_plugin_composer_Composer*)
netscape_plugin_composer_Composer_new(JRIEnv* env, struct java_lang_Class* clazz, jint a, jint b, jint c) 
{
  return NULL;
}


/* netscape_plugin_composer_PluginManager_getCategoryName                           liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(struct java_lang_String *)
netscape_plugin_composer_PluginManager_getCategoryName(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a) 
{
  return NULL;
}

/* netscape_plugin_composer_PluginManager_getEncoderFileExtension                   liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(struct java_lang_String *)
netscape_plugin_composer_PluginManager_getEncoderFileExtension(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a) 
{
  return NULL;
}

/* netscape_plugin_composer_PluginManager_getEncoderFileType                        liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(struct java_lang_String *)
netscape_plugin_composer_PluginManager_getEncoderFileType(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a) 
{
  return NULL;
}

/* netscape_plugin_composer_PluginManager_getEncoderHint                            liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(struct java_lang_String *)
netscape_plugin_composer_PluginManager_getEncoderHint(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a) 
{
  return NULL;
}

/* netscape_plugin_composer_PluginManager_getEncoderName                            liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(struct java_lang_String *)
netscape_plugin_composer_PluginManager_getEncoderName(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a) 
{
  return NULL;
}

/* netscape_plugin_composer_PluginManager_getEncoderNeedsQuantizedSource            liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(jbool)
netscape_plugin_composer_PluginManager_getEncoderNeedsQuantizedSource(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a) 
{
  return (jbool)FALSE;
}

/* netscape_plugin_composer_PluginManager_getNumberOfCategories                     liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(jint)
netscape_plugin_composer_PluginManager_getNumberOfCategories(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self) 
{
  return 0;
}

/* netscape_plugin_composer_PluginManager_getNumberOfEncoders                       liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(jint)
netscape_plugin_composer_PluginManager_getNumberOfEncoders(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self) 
{
  return 0;
}

/* netscape_plugin_composer_PluginManager_getNumberOfPlugins                        liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(jint)
netscape_plugin_composer_PluginManager_getNumberOfPlugins(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a) 
{
  return 0;
}

/* netscape_plugin_composer_PluginManager_getPluginHint                             liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(struct java_lang_String *)
netscape_plugin_composer_PluginManager_getPluginHint(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a, jint b) 
{
  return NULL;
}

/* netscape_plugin_composer_PluginManager_getPluginName                             liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(struct java_lang_String *)
netscape_plugin_composer_PluginManager_getPluginName(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a, jint b) 
{
  return NULL;
}

/* netscape_plugin_composer_PluginManager_new                                       liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(struct netscape_plugin_composer_PluginManager*)
netscape_plugin_composer_PluginManager_new(JRIEnv* env, struct java_lang_Class* clazz) 
{
  return NULL;
}

/* netscape_plugin_composer_PluginManager_performPlugin                             liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(jbool)
netscape_plugin_composer_PluginManager_performPlugin(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, struct netscape_plugin_composer_Composer *a, jint b, jint c, struct java_lang_String *d, struct java_lang_String *e, struct java_lang_String *f, struct java_lang_String *g, struct netscape_javascript_JSObject *h) 
{
  return (jbool)FALSE;
}

/* netscape_plugin_composer_PluginManager_performPluginByName                       liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(jbool)
netscape_plugin_composer_PluginManager_performPluginByName(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, struct netscape_plugin_composer_Composer *a, struct java_lang_String *b, struct java_lang_String *c, struct java_lang_String *d, struct java_lang_String *e, struct java_lang_String *f, struct netscape_javascript_JSObject *g) 
{
  return (jbool)FALSE;
}

/* netscape_plugin_composer_PluginManager_registerPlugin                            liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(void)
netscape_plugin_composer_PluginManager_registerPlugin(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, struct java_lang_String *a, struct java_lang_String *b) 
{
  return;
}

/* netscape_plugin_composer_PluginManager_startEncoder                              liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(jbool)
netscape_plugin_composer_PluginManager_startEncoder(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, struct netscape_plugin_composer_Composer *a, jint b, jint c, jint d, jarrayArray e, struct java_lang_String *f) 
{
  return (jbool)FALSE;
}

/* netscape_plugin_composer_PluginManager_stopPlugin                                liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(void)
netscape_plugin_composer_PluginManager_stopPlugin(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, struct netscape_plugin_composer_Composer *a) 
{
  return;
}

/* netscape_security_Principal_getVendor                                            libmocha.so */
/* ns/nav-java/netscape/security/_jri/netscape_security_Principal.h */
/* ns/nav-java/netscape/security/_jri/netscape_security_Principal.c */
JRI_PUBLIC_API(struct java_lang_String *)
netscape_security_Principal_getVendor(JRIEnv* env, struct netscape_security_Principal* self) 
{
  return NULL;
}

/* netscape_security_Principal_isCodebaseExact                                      libmocha.so */
/* ns/nav-java/netscape/security/_jri/netscape_security_Principal.h */
/* ns/nav-java/netscape/security/_jri/netscape_security_Principal.c */
JRI_PUBLIC_API(jbool)
netscape_security_Principal_isCodebaseExact(JRIEnv* env, struct netscape_security_Principal* self)
{
  return (jbool)FALSE;
}

/* netscape_security_Principal_new_3                                                libmocha.so */
/* ns/nav-java/netscape/security/_jri/netscape_security_Principal.h */
/* ns/nav-java/netscape/security/_jri/netscape_security_Principal.c */
JRI_PUBLIC_API(struct netscape_security_Principal*)
netscape_security_Principal_new_3(JRIEnv* env, struct java_lang_Class* clazz, jint a, jbyteArray b)
{
  return NULL;
}

/* netscape_security_Principal_new_5                                                libmocha.so */
/* ns/nav-java/netscape/security/_jri/netscape_security_Principal.h */
/* ns/nav-java/netscape/security/_jri/netscape_security_Principal.c */
JRI_PUBLIC_API(struct netscape_security_Principal*)
netscape_security_Principal_new_5(JRIEnv* env, struct java_lang_Class* clazz, jint a, jbyteArray b, struct netscape_security_Zig *c)
{
  return NULL;
}

/* netscape_security_Principal_toString                                             libmocha.so */
/* ns/nav-java/netscape/security/_jri/netscape_security_Principal.h */
/* ns/nav-java/netscape/security/_jri/netscape_security_Principal.c */
JRI_PUBLIC_API(struct java_lang_String *)
netscape_security_Principal_toString(JRIEnv* env, struct netscape_security_Principal* self) 
{
  return NULL;
}

/* netscape_security_PrivilegeManager_canExtendTrust                                libmocha.so */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeManager.h */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeManager.c */
JRI_PUBLIC_API(jbool)
netscape_security_PrivilegeManager_canExtendTrust(JRIEnv* env, struct netscape_security_PrivilegeManager* self, jobjectArray a, jobjectArray b) 
{
  return (jbool)FALSE;
}


/* netscape_security_PrivilegeManager_comparePrincipalArray                         libmocha.so */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeManager.h */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeManager.c */
JRI_PUBLIC_API(jint)
netscape_security_PrivilegeManager_comparePrincipalArray(JRIEnv* env, struct netscape_security_PrivilegeManager* self, jobjectArray a, jobjectArray b)
{
  /* same as NO_SUBSET in PrivilegeManager.java */
  return 1;
}


/* netscape_security_PrivilegeManager_getClassPrincipalsFromStack                   libmocha.so */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeManager.h */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeManager.c */
JRI_PUBLIC_API(jref)
netscape_security_PrivilegeManager_getClassPrincipalsFromStack(JRIEnv* env, struct netscape_security_PrivilegeManager* self, jint a)
{
  return NULL;
}

/* netscape_security_PrivilegeManager_getPrivilegeManager                           libmocha.so */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeManager.h */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeManager.c */
JRI_PUBLIC_API(struct netscape_security_PrivilegeManager *)
netscape_security_PrivilegeManager_getPrivilegeManager(JRIEnv* env, struct java_lang_Class* clazz)
{
  return NULL;
}

/* netscape_security_PrivilegeManager_intersectPrincipalArray                       libmocha.so */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeManager.h */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeManager.c */
JRI_PUBLIC_API(jref)
netscape_security_PrivilegeManager_intersectPrincipalArray(JRIEnv* env, struct netscape_security_PrivilegeManager* self, jobjectArray a, jobjectArray b)
{
  return NULL;
}

/* netscape_security_PrivilegeManager_isPrivilegeEnabled                            libmocha.so */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeManager.h */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeManager.c */
JRI_PUBLIC_API(jbool)
netscape_security_PrivilegeManager_isPrivilegeEnabled(JRIEnv* env, struct netscape_security_PrivilegeManager* self, struct netscape_security_Target *a, jint b)
{
  return (jbool)FALSE;
}

/* netscape_security_PrivilegeManager_registerPrincipal                             libmocha.so */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeManager.h */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeManager.c */
JRI_PUBLIC_API(void)
netscape_security_PrivilegeManager_registerPrincipal(JRIEnv* env, struct netscape_security_PrivilegeManager* self, struct netscape_security_Principal *a)
{
  return;
}

/* netscape_security_PrivilegeTable_get_1                                           libmocha.so */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeTable.h */
/* ns/nav-java/netscape/security/_jri/netscape_security_PrivilegeTable.c */
JRI_PUBLIC_API(struct netscape_security_Privilege *)
netscape_security_PrivilegeTable_get_1(JRIEnv* env, struct netscape_security_PrivilegeTable* self, struct netscape_security_Target *a)
{
  return NULL;
}

/* netscape_security_Privilege_getPermission                                        libmocha.so */
/* ns/nav-java/netscape/security/_jri/netscape_security_Privilege.h */
/* ns/nav-java/netscape/security/_jri/netscape_security_Privilege.c */
JRI_PUBLIC_API(jint)
netscape_security_Privilege_getPermission(JRIEnv* env, struct netscape_security_Privilege* self)
{
  /* Same as FORBIDDEN in Principal.java*/
  return 0;
}

/* netscape_security_Target_findTarget                                              libmocha.so */
/* ns/nav-java/netscape/security/_jri/netscape_security_Target.h */
/* ns/nav-java/netscape/security/_jri/netscape_security_Target.c */
JRI_PUBLIC_API(struct netscape_security_Target *)
netscape_security_Target_findTarget(JRIEnv* env, struct java_lang_Class* clazz, struct java_lang_String *a)
{
  return NULL;
}

/* newobject                                                                        libjsj.so */
/* sun-java */


/* ns_createZigObject                                                               libmocha.so */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/appletStubs.c */
JRI_PUBLIC_API(struct netscape_security_Zig *)
ns_createZigObject(JRIEnv* env, void *zig)
{
  return NULL;
}

/* set_netscape_plugin_Plugin_peer                                                  libplug.so */
/* ns/modules/applet/src/_jri/netscape_plugin_Plugin.h */
/* ns/modules/applet/src/_jri/netscape_plugin_Plugin.c */
JRI_PUBLIC_API(void)
set_netscape_plugin_Plugin_peer(JRIEnv* env, struct netscape_plugin_Plugin* obj, jint v) 
{
  return;
}

/* set_netscape_plugin_Plugin_window                                                libplug.so */
/* ns/modules/applet/src/_jri/netscape_plugin_Plugin.h */
/* ns/modules/applet/src/_jri/netscape_plugin_Plugin.c */
JRI_PUBLIC_API(void)
set_netscape_plugin_Plugin_window(JRIEnv* env, struct netscape_plugin_Plugin* obj, struct netscape_javascript_JSObject * v) 
{
  return;
}

/* sizearray                                                                        libjsj.so */
/* sun-java */

/* use_netscape_plugin_Plugin                                                       libplug.so */
/* ns/modules/applet/src/_jri/netscape_plugin_Plugin.h */
/* ns/modules/applet/src/_jri/netscape_plugin_Plugin.c */
JRI_PUBLIC_API(struct java_lang_Class*)
use_netscape_plugin_Plugin(JRIEnv* env)
{
  return NULL;
}

/* use_netscape_plugin_composer_Composer                                            liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_Composer.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_Composer.c */
JRI_PUBLIC_API(struct java_lang_Class*)
use_netscape_plugin_composer_Composer(JRIEnv* env)
{
  return NULL;
}

/* use_netscape_plugin_composer_MozillaCallback                                     liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_MozillaCallback.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_MozillaCallback.c */
JRI_PUBLIC_API(struct java_lang_Class*)
use_netscape_plugin_composer_MozillaCallback(JRIEnv* env)
{
  return NULL;
}

/* use_netscape_plugin_composer_PluginManager                                       liblay.so */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.h */
/* ns/modules/edtplug/src/_jri/netscape_plugin_composer_PluginManager.c */
JRI_PUBLIC_API(struct java_lang_Class*)
use_netscape_plugin_composer_PluginManager(JRIEnv* env)
{
  return NULL;
}


/* zip_close                                                                        liblay.so */
/* sun-java */

/* zip_get                                                                          liblay.so */
/* sun-java */

/* zip_open                                                                         liblay.so */
/* sun-java */

/* zip_stat                                                                         liblay.so */
/* sun-java */


