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

/*
 *  nppg.h $Revision: 1.1 $
 *  Prototypes for functions exported by the FEs and called by libplugin.
 *  Some (perhaps all) of these prototypes could be moved to fe_proto.h.
 *  Protypes for functions exported by libplugin are in np.h.
 */

#ifndef _NPPG_H
#define _NPPG_H

#include "npapi.h"
#include "npupp.h"
struct _np_handle;

XP_BEGIN_PROTOS

extern void 			FE_RegisterPlugins(void);
extern NPPluginFuncs*	FE_LoadPlugin(void *plugin, NPNetscapeFuncs *, struct _np_handle* handle);
extern void				FE_UnloadPlugin(void *plugin, struct _np_handle* handle);
extern void				FE_EmbedURLExit(URL_Struct *urls, int status, MWContext *cx);
extern void				FE_ShowScrollBars(MWContext *context, XP_Bool show);
#if defined (XP_WIN) || defined(XP_OS2)
extern void 			FE_FreeEmbedSessionData(MWContext *context, NPEmbeddedApp* pApp);
#endif

#ifdef XP_MAC
extern void 			FE_PluginProgress(MWContext *context, const char *message);
extern void 			FE_ResetRefreshURLTimer(MWContext *context);
extern void				FE_RegisterWindow(void* plugin, void* window);
extern void				FE_UnregisterWindow(void* plugin, void* window);
extern SInt16			FE_AllocateMenuID(void *plugin, XP_Bool isSubmenu);
#endif

#ifdef XP_UNIX
extern NPError			FE_PluginGetValue(void *pdesc, NPNVariable variable, void *r_value);
#else
extern NPError                  FE_PluginGetValue(MWContext *context, NPEmbeddedApp *app, NPNVariable variable, void *r_value);
#endif /* XP_UNIX */

#ifdef XP_WIN32
void*	WFE_BeginSetModuleState();
void	WFE_EndSetModuleState(void*);
#endif

XP_END_PROTOS

#endif /* _NPPG_H */
