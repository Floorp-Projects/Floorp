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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#ifndef __EPMANAGER_H__
#define __EPMANAGER_H__

#include "npapi.h"
#include "npupp.h"

typedef NPError (__stdcall * NP_GETENTRYPOINTS) (NPPluginFuncs* pFuncs);
typedef NPError (__stdcall * NP_INITIALIZE) (NPNetscapeFuncs* pFuncs);
typedef NPError (__stdcall * NP_SHUTDOWN) (void);

struct InstanceList
{
  InstanceList * next;
  NPP instance;

  InstanceList(NPP _instance);
  ~InstanceList();
};

struct PluginEntryPointList
{
  PluginEntryPointList * next;
  char mimetype[80];
  InstanceList * instances;
  NPPluginFuncs realNPPFuncs;
  NP_SHUTDOWN realShutdown;
  XP_HLIB hLib;

  PluginEntryPointList();
  ~PluginEntryPointList();
};

class NPPEntryPointManager
{
private:
  PluginEntryPointList * mEntryPoints;

public:
  void createEntryPointsForPlugin(char * mimetype, NPPluginFuncs * funcs, NP_SHUTDOWN shutdownproc, XP_HLIB hLib);
  void removeEntryPointsForPlugin(NPP instance, XP_HLIB * lib);
  NPPluginFuncs * findEntryPointsForPlugin(char * mimetype);

private:  
  NPPluginFuncs * findEntryPointsForInstance(NPP instance);

public:
  NPPEntryPointManager();
  ~NPPEntryPointManager();

  void callNP_Shutdown(NPP instance);
  void callNP_ShutdownAll();

  NPError callNPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved);
  NPError callNPP_Destroy(NPP instance, NPSavedData** save, BOOL * last);
  NPError callNPP_SetWindow(NPP instance, NPWindow* window);
  NPError callNPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype);
  NPError callNPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason);
  int32   callNPP_WriteReady(NPP instance, NPStream* stream);
  int32   callNPP_Write(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer);
  void    callNPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname);
  void    callNPP_Print(NPP instance, NPPrint* platformPrint);
  void    callNPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData);
  jref    callNPP_GetJavaClass(void);
  NPError callNPP_GetValue(NPP instance, NPPVariable variable, void *value);
  NPError callNPP_SetValue(NPP instance, NPNVariable variable, void *value);
  int16   callNPP_HandleEvent(NPP instance, void* event);
};

#endif
