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
#include "typedefs.h"
#include "oobj.h"
#include "javaThreads.h"

/* ArrayAlloc                                                                       libjsj.so */
/* ns/sun-java/include/interpreter.h */
/* ns/sun-java/md/gc_md.c */
JRI_PUBLIC_API(HObject *)
ArrayAlloc(int32_t t, int32_t l)
{
  return NULL;
}

#ifdef XP_UNIX
#include <X11/Intrinsic.h>

/* AwtRegisterXtAppVars                                                             mozilla.o */
/* ??? */
/* ns/sun-java/awt/x/awt_MToolkit.c */
void
AwtRegisterXtAppVars(Display *dpy, XtAppContext ac, char *class)
{
  return;
}
#endif

/* CompiledFramePrev                                                                libjsj.so */
/* ns/sun-java/include/interpreter.h */
/* ns/sun-java/runtime/compiler.c */
JRI_PUBLIC_API(JavaFrame *)
CompiledFramePrev(JavaFrame *frame, JavaFrame *buf)
{
  return NULL;
}

/* CreateNewJavaStack                                                               libjsj.so */
/* ns/sun-java/include/interpreter.h */
/* ns/sun-java/runtime/interpreter.c */
JRI_PUBLIC_API(JavaStack *)
CreateNewJavaStack(ExecEnv *ee, JavaStack *previous_stack)
{
  return NULL;
}

/* EDTPLUG_RegisterEditURLCallback                                                  editor.o */
/* nav-java */

/* ExecuteJava                                                                      libjsj.so */
/* ns/sun-java/include/interpreter.h */
/* ns/sun-java/runtime/executeJava.c */
JRI_PUBLIC_API(bool_t)
ExecuteJava(unsigned char *initial_pc, ExecEnv *ee)
{
  return FALSE;
}

/* FindClassFromClass                                                               libjsj.so */
/* ns/sun-java/include/interpreter.h */
/* ns/sun-java/runtime/classresolver.c */
JRI_PUBLIC_API(ClassClass *)
FindClassFromClass(struct execenv *ee, char *name, bool_t resolve,
		   ClassClass *from)
{
  return NULL;
}

/* FindLoadedClass                                                                  libjsj.so */
/* ns/sun-java/include/interpreter.h */
/* ns/sun-java/runtime/classresolver.c */
JRI_PUBLIC_API(ClassClass *)
FindLoadedClass(char *name, struct Hjava_lang_ClassLoader *loader)
{
  return NULL;
}

#include "jmc.h"
/* JMCException_Destroy                                                             xfe.o */
/* ns/sun-java/jtools/include/jmc.h */
/* ns/sun-java/jtools/src/jmc.c */
JRI_PUBLIC_API(void)
JMCException_Destroy(struct JMCException *self)
{
  return;
}

/* JRI_GetCurrentEnv                                                                libplug.so */
/* ns/sun-java/include/jritypes.h */
/* ns/sun-java/runtime/jrijdk.c */
JRI_PUBLIC_API(const JRIEnvInterface **)
JRI_GetCurrentEnv(void)
{
  return NULL;
}

/* LJ_AddToClassPath                                                                libxfe2.so */
/* nav-java */

/* LJ_Applet_GetText                                                                liblay.so */
/* nav-java */

/* LJ_Applet_print                                                                  libxlate.so */
/* nav-java */

/* LJ_CreateApplet                                                                  liblay.so */
/* nav-java */

/* LJ_DeleteSessionData                                                             liblay.so */
/* nav-java */

/* LJ_DiscardEventsForContext                                                       xfe.o */
/* nav-java */

/* LJ_DisplayJavaApp                                                                lay.o */
/* nav-java */

/* LJ_EnsureJavaEnv                                                                 libplug.so */
/* nav-java */

/* LJ_FreeJavaAppElement                                                            lay.o */
/* nav-java */

/* LJ_GetAppletScriptOrigin                                                         libmocha.so */
/* nav-java */

/* LJ_GetJavaAppSize                                                                lay.o */
/* nav-java */

/* LJ_GetJavaEnabled                                                                libxfe2.so */
/* nav-java */

/* LJ_GetMochaWindow                                                                liblay.so */
/* nav-java */

/* LJ_HandleEvent                                                                   liblay.so */
/* nav-java */

/* LJ_HideConsole                                                                   commands.o */
/* nav-java */

/* LJ_HideJavaAppElement                                                            lay.o */
/* nav-java */

/* LJ_IconifyApplets                                                                xfe.o */
/* nav-java */

/* LJ_InitializeZig                                                                 libmocha.so */
/* nav-java */

/* LJ_InvokeMethod                                                                  libmocha.so */
/* nav-java */

/* LJ_JSJ_CurrentEnv                                                                libmocha.so */
/* nav-java */

/* LJ_JSJ_Init                                                                      libmocha.so */
/* nav-java */

/* LJ_LoadFromZipFile                                                               libmocha.so */
/* nav-java */

/* LJ_PrintZigError                                                                 libmocha.so */
/* nav-java */

/* LJ_SetConsoleShowCallback                                                        mozilla.o */
/* nav-java */

/* LJ_SetProgramName                                                                mozilla.o */
/* nav-java */

/* LJ_ShowConsole                                                                   commands.o */
/* nav-java */

/* LJ_ShutdownJava                                                                  libxfe2.so */
/* nav-java */

/* LJ_UniconifyApplets                                                              xfe.o */
/* nav-java */

/* MakeClassSticky                                                                  libjsj.so */
/* ns/sun-java/include/interpreter.h */
/* ns/sun-java/runtime/classloader.c */
JRI_PUBLIC_API(void)
MakeClassSticky(ClassClass *cb)
{
  return;
}

/* NR_ShutdownRegistry                                                              xfe.o */
/* nav-java */

/* NR_StartupRegistry                                                               mozilla.o */
/* nav-java */

/* NSN_JavaContextToRealContext                                                     xfe.o */
/* nav-java */

/* NSN_RegisterJavaConverter                                                        libnet.so */
/* nav-java */

/* PrintToConsole                                                                   libmocha.so */
/* nav-java */

/* SU_NewStream                                                                     libnet.so */
/* nav-java */

/* SU_StartSoftwareUpdate                                                           libmocha.so */
/* nav-java */

/* VR_GetPath                                                                       libxfe2.so */
/* nav-java */

/* VR_GetVersion                                                                    libmocha.so */
/* nav-java */

/* VR_ValidateComponent                                                             libxfe2.so */
/* nav-java */

/* VerifyClassAccess                                                                libjsj.so */
/* ns/sun-java/include/interpreter.h */
/* ns/sun-java/runtime/classinitialize.c */
JRI_PUBLIC_API(bool_t)
VerifyClassAccess(ClassClass *current_class, ClassClass *new_class, 
		  bool_t classloader_only) 
{
  return FALSE;
}    

/* VerifyFieldAccess                                                                libjsj.so */
/* ns/sun-java/include/interpreter.h */
/* ns/sun-java/runtime/classinitialize.c */
JRI_PUBLIC_API(bool_t)
VerifyFieldAccess(ClassClass *current_class, ClassClass *field_class, 
		  int access, bool_t classloader_only)
{
  return FALSE;
}

/* awt_MToolkit_dispatchToWidget                                                    mozilla.o */
/* ??? */
/* ns/sun-java/awt/x/awt_MTookit.c */
#ifdef XP_UNIX
int
awt_MToolkit_dispatchToWidget(XEvent *xev)
{
  return 0;
}
#endif

/* awt_MToolkit_finishModals                                                        mozilla.o */
/* ??? */
/* ns/sun-java/awt/x/awt_MTookit.c */
void
awt_MToolkit_finishModals(void)
{
  return;
}

/* classEmbeddedObjectNatives                                                       libmocha.so */
/* nav-java */

/* classMozillaAppletContext                                                        libmocha.so */
/* nav-java */

/* do_execute_java_method                                                           libjsj.so */
/* ns/sun-java/include/interpreter.h */
/* ns/sun-java/runtime/interpreter.c */
JRI_PUBLIC_API(long)
do_execute_java_method(ExecEnv *ee, void *obj,
		      char *method_name, char *signature,
		      struct methodblock *mb,
		      bool_t isStaticCall, ...)
{
  return 0;
}

/* do_execute_java_method_vararg                                                    libjsj.so */
/* ns/sun-java/include/interpreter.h */
/* ns/sun-java/runtime/interpreter.c */
JRI_PUBLIC_API(long)
do_execute_java_method_vararg(ExecEnv *ee, void *obj,
			      char *method_name, char *method_signature,
			      struct methodblock *mb,
			      bool_t isStaticCall, va_list args,
			      long *otherBits, bool_t shortFloats)
{
  return 0;
}

/* execute_java_constructor                                                         libjsj.so */
/* ns/sun-java/include/interpreter.h */
/* ns/sun-java/runtime/interpreter.c */
JRI_PUBLIC_API(HObject *)
execute_java_constructor(struct execenv *ee,
				  char *classname,
				  ClassClass *cb,
				  char *signature, ...)
{
  return NULL;
}

/* execute_java_constructor_vararg                                                  libjsj.so */
/* ns/sun-java/include/interpreter.h */
/* ns/sun-java/runtime/interpreter.c */
JRI_PUBLIC_API(HObject *)
execute_java_constructor_vararg(struct execenv *ee,
				  char *classname,
				  ClassClass *cb,
				  char *signature, va_list args)
{
  return NULL;
}

/* is_subclass_of                                                                   libjsj.so */
/* ns/sun-java/include/interpreter.h */
/* ns/sun-java/runtime/interpreter.c */
JRI_PUBLIC_API(bool_t)
is_subclass_of(ClassClass *cb, ClassClass *dcb, ExecEnv *ee)
{
  return FALSE;
}

/* java_netscape_security_getPrincipals                                             libsecnav.so */
/* nav-java */

/* java_netscape_security_getPrivilegeDescs                                         libsecnav.so */
/* nav-java */

/* java_netscape_security_getTargetDetails                                          libsecnav.so */
/* nav-java */

/* java_netscape_security_removePrincipal                                           libsecnav.so */
/* nav-java */

/* java_netscape_security_removePrivilege                                           libsecnav.so */
/* nav-java */

/* java_netscape_security_savePrivilege                                             libsecnav.so */
/* nav-java */

#include "javaString.h"
/* makeJavaString                                                                   libmocha.so */
/* ns/sun-java/include/interpreter.h */
/* ns/sun-java/runtime/string.c */
JRI_PUBLIC_API(Hjava_lang_String *)
makeJavaString(char *str, int len)
{
  return NULL;
}

/* methodID_netscape_applet_EmbeddedObjectNatives_reflectObject                     libmocha.so */
/* nav-java */

/* methodID_netscape_applet_MozillaAppletContext_reflectApplet_1                    libmocha.so */
/* nav-java */

/* native_netscape_security_PrivilegeManager_getClassPrincipalsFromStackUnsafe      libmocha.so */
/* nav-java */

/* netscape_plugin_Plugin_destroy                                                   libplug.so */
/* nav-java */

/* netscape_plugin_Plugin_init                                                      libplug.so */
/* nav-java */

/* netscape_plugin_Plugin_new                                                       libplug.so */
/* nav-java */

/* netscape_plugin_composer_Composer_new                                            liblay.so */
/* nav-java */

/* netscape_plugin_composer_PluginManager_getCategoryName                           liblay.so */
/* nav-java */

/* netscape_plugin_composer_PluginManager_getEncoderFileExtension                   liblay.so */
/* nav-java */

/* netscape_plugin_composer_PluginManager_getEncoderFileType                        liblay.so */
/* nav-java */

/* netscape_plugin_composer_PluginManager_getEncoderHint                            liblay.so */
/* nav-java */

/* netscape_plugin_composer_PluginManager_getEncoderName                            liblay.so */
/* nav-java */

/* netscape_plugin_composer_PluginManager_getEncoderNeedsQuantizedSource            liblay.so */
/* nav-java */

/* netscape_plugin_composer_PluginManager_getNumberOfCategories                     liblay.so */
/* nav-java */

/* netscape_plugin_composer_PluginManager_getNumberOfEncoders                       liblay.so */
/* nav-java */

/* netscape_plugin_composer_PluginManager_getNumberOfPlugins                        liblay.so */
/* nav-java */

/* netscape_plugin_composer_PluginManager_getPluginHint                             liblay.so */
/* nav-java */

/* netscape_plugin_composer_PluginManager_getPluginName                             liblay.so */
/* nav-java */

/* netscape_plugin_composer_PluginManager_new                                       liblay.so */
/* nav-java */

/* netscape_plugin_composer_PluginManager_performPlugin                             liblay.so */
/* nav-java */

/* netscape_plugin_composer_PluginManager_performPluginByName                       liblay.so */
/* nav-java */

/* netscape_plugin_composer_PluginManager_registerPlugin                            liblay.so */
/* nav-java */

/* netscape_plugin_composer_PluginManager_startEncoder                              liblay.so */
/* nav-java */

/* netscape_plugin_composer_PluginManager_stopPlugin                                liblay.so */
/* nav-java */

/* netscape_security_Principal_getVendor                                            libmocha.so */
/* nav-java */

/* netscape_security_Principal_isCodebaseExact                                      libmocha.so */
/* nav-java */

/* netscape_security_Principal_new_3                                                libmocha.so */
/* nav-java */

/* netscape_security_Principal_new_5                                                libmocha.so */
/* nav-java */

/* netscape_security_Principal_toString                                             libmocha.so */
/* nav-java */

/* netscape_security_PrivilegeManager_canExtendTrust                                libmocha.so */
/* nav-java */

/* netscape_security_PrivilegeManager_comparePrincipalArray                         libmocha.so */
/* nav-java */

/* netscape_security_PrivilegeManager_getClassPrincipalsFromStack                   libmocha.so */
/* nav-java */

/* netscape_security_PrivilegeManager_getPrivilegeManager                           libmocha.so */
/* nav-java */

/* netscape_security_PrivilegeManager_intersectPrincipalArray                       libmocha.so */
/* nav-java */

/* netscape_security_PrivilegeManager_isPrivilegeEnabled                            libmocha.so */
/* nav-java */

/* netscape_security_PrivilegeManager_registerPrincipal                             libmocha.so */
/* nav-java */

/* netscape_security_PrivilegeTable_get_1                                           libmocha.so */
/* nav-java */

/* netscape_security_Privilege_getPermission                                        libmocha.so */
/* nav-java */

/* netscape_security_Target_findTarget                                              libmocha.so */
/* nav-java */

/* newobject                                                                        libjsj.so */
/* ns/sun-java/include/interpreter.h */
/* ns/sun-java/runtime/string.c */
JRI_PUBLIC_API(HObject*)
newobject(ClassClass *cb, unsigned char *pc, struct execenv *ee)
{
  return NULL;
}

/* ns_createZigObject                                                               libmocha.so */
/* nav-java */

/* set_netscape_plugin_Plugin_peer                                                  libplug.so */
/* nav-java */

/* set_netscape_plugin_Plugin_window                                                libplug.so */
/* nav-java */

/* sizearray                                                                        libjsj.so */
/* ns/sun-java/include/interpreter.h */
/* ns/sun-java/runtime/gc.c */
int32_t
sizearray(int32_t t, int32_t l)
{
  return 0;
}

/* use_netscape_plugin_Plugin                                                       libplug.so */
/* nav-java */

/* use_netscape_plugin_composer_Composer                                            liblay.so */
/* nav-java */

/* use_netscape_plugin_composer_MozillaCallback                                     liblay.so */
/* nav-java */

/* use_netscape_plugin_composer_PluginManager                                       liblay.so */
/* nav-java */

#include "zip.h"
/* zip_close                                                                        liblay.so */
/* ns/sun-java/include/zip.h */
/* ns/sun-java/runtime/zip.c */
JRI_PUBLIC_API(void)
zip_close(zip_t *zip)
{
  return;
}

/* zip_get                                                                          liblay.so */
/* ns/sun-java/include/zip.h */
/* ns/sun-java/runtime/zip.c */
JRI_PUBLIC_API(bool_t)
zip_get(zip_t *zip, const char *fn, void HUGEP *buf, int32_t len)
{
  return FALSE;
}

/* zip_open                                                                         liblay.so */
/* ns/sun-java/include/zip.h */
/* ns/sun-java/runtime/zip.c */
JRI_PUBLIC_API(zip_t *)
zip_open(const char *fn)
{
  return NULL;
}

/* zip_stat                                                                         liblay.so */
/* ns/sun-java/include/zip.h */
/* ns/sun-java/runtime/zip.c */
JRI_PUBLIC_API(bool_t)
zip_stat(zip_t *zip, const char *fn, struct stat *sbuf)
{
  return FALSE;
}
