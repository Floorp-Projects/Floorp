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
#ifndef _NAVJAVA_H_
#define _NAVJAVA_H_

#include "lo_ele.h"
#ifndef NSPR20
#include "prevent.h"
#else
#include "plevent.h"
#endif
#include "jri.h"
#include "prthread.h"

/* for Mocha glue */
#include "jsapi.h"
#include "jsjava.h"

XP_BEGIN_PROTOS

struct java_applet_Applet;
struct netscape_applet_EmbeddedAppletFrame;
struct netscape_javascript_JSObject;
struct netscape_security_Zig;
struct Hjava_lang_Object;

/* Type of data that hangs off of LO_JavaStruct->FE_Data */
typedef struct LJAppletData LJAppletData;
struct LJAppletData {
    int16 generation;
};

typedef enum LJJavaStatus
{
  LJJavaStatus_Enabled,     /* but not Running */
  LJJavaStatus_Disabled,    /* explicitly disabled */
  LJJavaStatus_Running,     /* enabled and started */
  LJJavaStatus_Failed       /* enabled but failed to start */
} LJJavaStatus;

/*******************************************************************************
 * Mozilla Events
 ******************************************************************************/

/* Generic event struct for events coming from layer */
typedef struct _tagNAppletEvent {
    int id;
    void* data;
} NAppletEvent;


JRI_PUBLIC_API(void) 
LJ_AddToClassPath(char* dirPath);

JRI_PUBLIC_API(char *) 
LJ_Applet_GetText(LJAppletData* ad);

JRI_PUBLIC_API(void) 
LJ_Applet_print(LJAppletData *ad, void* printInfo);

JRI_PUBLIC_API(void)
LJ_CreateApplet(LO_JavaAppStruct *java, MWContext *cx,
		NET_ReloadMethod reloadMethod);

JRI_PUBLIC_API(void) 
LJ_DeleteSessionData(MWContext* cx, void* sdata);

JRI_PUBLIC_API(void)
LJ_DiscardEventsForContext(MWContext* context);

JRI_PUBLIC_API(void)
LJ_DisplayJavaApp(MWContext *context, ...);

JRI_PUBLIC_API(JRIEnv *)
LJ_EnsureJavaEnv(PRThread *thread);

JRI_PUBLIC_API(void)
LJ_FreeJavaAppElement(MWContext *context, ...);

JRI_PUBLIC_API(char *)
LJ_GetAppletScriptOrigin(JRIEnv *env);

JRI_PUBLIC_API(void)
LJ_GetJavaAppSize(MWContext *context, LO_JavaAppStruct *java_struct,
		  NET_ReloadMethod reloadMethod);

JRI_PUBLIC_API(PRBool)
LJ_GetJavaEnabled(void);

JRI_PUBLIC_API(struct netscape_javascript_JSObject*)
LJ_GetMochaWindow(MWContext *cx);

JRI_PUBLIC_API(PRBool) 
LJ_HandleEvent(MWContext* context, LO_JavaAppStruct *pJava, void *event);

JRI_PUBLIC_API(void)
LJ_HideConsole(void);

JRI_PUBLIC_API(void)
LJ_HideJavaAppElement(MWContext *context, ...);

JRI_PUBLIC_API(void) 
LJ_IconifyApplets(MWContext* context);

PR_PUBLIC_API(void *)
LJ_InitializeZig(void *zipPtr);

jref
LJ_InvokeMethod(jglobal clazzGlobal, ...);

JRI_PUBLIC_API(JRIEnv *)
LJ_JSJ_CurrentEnv(JSContext *cx);

JRI_PUBLIC_API(void)
LJ_JSJ_Init();

PR_PUBLIC_API(char *)
LJ_LoadFromZipFile(void *zip, char *fn);

PR_PUBLIC_API(int)
LJ_PrintZigError(int status, void *zigPtr, const char *metafile, char *pathname, 
		 char *errortext);

JRI_PUBLIC_API(void)
LJ_SetConsoleShowCallback(void (*func)(int on, void* a), void *arg);

void
LJ_SetProgramName(const char *name);

JRI_PUBLIC_API(void)
LJ_ShowConsole(void);

JRI_PUBLIC_API(LJJavaStatus)
LJ_ShutdownJava(void);

JRI_PUBLIC_API(void)
LJ_UniconifyApplets(MWContext* context);

/* ??? Is it needed for windows ??? */
/* LJ_StartupJava used in ./cmd/winfe/woohoo.cpp */
/* ns/modules/applet/include/java.h */
/* ns/modules/applet/src/lj_init.c */
JRI_PUBLIC_API(LJJavaStatus)
LJ_StartupJava(void);

JRI_PUBLIC_API(MWContext *)
NSN_JavaContextToRealContext(MWContext *javaContext);

JRI_PUBLIC_API(void) 
NSN_RegisterJavaConverter(void);

JRI_PUBLIC_API(void)
PrintToConsole(const char* bytes);

/******************************************************************************/

XP_END_PROTOS

#endif /* _NAVJAVA_H_ */
