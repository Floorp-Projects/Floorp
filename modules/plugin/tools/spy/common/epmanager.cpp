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

#include "xp.h"

#include "epmanager.h"
#include "logger.h"

extern Logger * logger;

InstanceList::InstanceList(NPP _instance) : 
  next(NULL), 
  instance(_instance)
{
}

InstanceList::~InstanceList()
{
}

PluginEntryPointList::PluginEntryPointList() :
  next(NULL),
  instances(NULL)
{
  mimetype[0] = '\0';
  memset((void *)&realNPPFuncs, 0, sizeof(realNPPFuncs));
  realShutdown = NULL;
  hLib = NULL;
}

PluginEntryPointList::~PluginEntryPointList()
{
}

NPPEntryPointManager::NPPEntryPointManager() :
  mEntryPoints(NULL)
{
}

NPPEntryPointManager::~NPPEntryPointManager()
{
  for(PluginEntryPointList * eps = mEntryPoints; eps != NULL;)
  {
    for(InstanceList * instances = eps->instances; instances != NULL;)
    {
      InstanceList * next = instances->next;
      delete instances;
      instances = next;
    }

    PluginEntryPointList * next = eps->next;
    delete eps;
    eps = next;
  }
}

void NPPEntryPointManager::createEntryPointsForPlugin(char * mimetype, NPPluginFuncs * funcs, NP_SHUTDOWN shutdownproc, XP_HLIB hLib)
{
  PluginEntryPointList * eps = new PluginEntryPointList();

  if(eps == NULL)
    return;

  strcpy(eps->mimetype, mimetype);

  if(funcs)
  {
    eps->realNPPFuncs.size          = funcs->size;
    eps->realNPPFuncs.version       = funcs->version;
    eps->realNPPFuncs.newp          = funcs->newp;
    eps->realNPPFuncs.destroy       = funcs->destroy;
    eps->realNPPFuncs.setwindow     = funcs->setwindow;
    eps->realNPPFuncs.newstream     = funcs->newstream;
    eps->realNPPFuncs.destroystream = funcs->destroystream;
    eps->realNPPFuncs.asfile        = funcs->asfile;
    eps->realNPPFuncs.writeready    = funcs->writeready;
    eps->realNPPFuncs.write         = funcs->write;
    eps->realNPPFuncs.print         = funcs->print;
    eps->realNPPFuncs.event         = funcs->event;
    eps->realNPPFuncs.urlnotify     = funcs->urlnotify;
    eps->realNPPFuncs.javaClass     = funcs->javaClass;
    eps->realNPPFuncs.getvalue      = funcs->getvalue;
    eps->realNPPFuncs.setvalue      = funcs->setvalue;
  }

  eps->realShutdown = shutdownproc;
  eps->hLib = hLib;

  eps->next = mEntryPoints;
  mEntryPoints = eps;
}

void NPPEntryPointManager::removeEntryPointsForPlugin(NPP instance, XP_HLIB * lib)
{
  NPPluginFuncs * eptoremove = findEntryPointsForInstance(instance);

  PluginEntryPointList * prev = NULL;

  for(PluginEntryPointList * eps = mEntryPoints; eps != NULL; eps = eps->next)
  {
    if(&eps->realNPPFuncs == eptoremove)
    {
      if(prev)
        prev->next = eps->next;
      else
        mEntryPoints = eps->next;

      *lib = eps->hLib;
      delete eps;
      return;
    }

    prev = eps;
  }
}

NPPluginFuncs * NPPEntryPointManager::findEntryPointsForPlugin(char * mimetype)
{
  for(PluginEntryPointList * eps = mEntryPoints; eps != NULL; eps = eps->next)
  {
    if(0 == stricmp(eps->mimetype, mimetype))
      return &eps->realNPPFuncs;
  }

  return NULL;
}

NPPluginFuncs * NPPEntryPointManager::findEntryPointsForInstance(NPP instance)
{
  for(PluginEntryPointList * eps = mEntryPoints; eps != NULL; eps = eps->next)
  {
    for(InstanceList * instances = eps->instances; instances != NULL; instances = instances->next)
    {
      if(instances->instance == instance)
        return &eps->realNPPFuncs;
    }
  }

  return NULL;
}

void NPPEntryPointManager::callNP_ShutdownAll()
{
  for(PluginEntryPointList * eps = mEntryPoints; eps != NULL; eps = eps->next)
  {
    if(eps->realShutdown)
    {
      logger->logSPY_NP_Shutdown(eps->mimetype);
      eps->realShutdown();
      eps->realShutdown = NULL; // don't want to call it more than once
    }
  }
}

void NPPEntryPointManager::callNP_Shutdown(NPP instance)
{
  for(PluginEntryPointList * eps = mEntryPoints; eps != NULL; eps = eps->next)
  {
    for(InstanceList * instances = eps->instances; instances != NULL; instances = instances->next)
    {
      if(instances->instance == instance)
      {
        if(eps->realShutdown)
        {
          logger->logSPY_NP_Shutdown(eps->mimetype);
          eps->realShutdown();
          eps->realShutdown = NULL; // don't want to call it more than once
        }
      }
    }
  }
}

NPError NPPEntryPointManager::callNPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved)
{
  NPPluginFuncs * nppfuncs = NULL;

  for(PluginEntryPointList * eps = mEntryPoints; eps != NULL; eps = eps->next)
  {
    if(0 == stricmp(eps->mimetype, pluginType))
    {
      nppfuncs = &eps->realNPPFuncs;

      // now we should associate this plugin instance with plugin entry points
      // so that later we could find entry points by instance rather than by mimetype
      InstanceList * inst = new InstanceList(instance);
      inst->next = eps->instances;
      eps->instances = inst;

      break;
    }
  }

  if(!nppfuncs || !nppfuncs->newp)
    return NPERR_GENERIC_ERROR;

  NPError rv = CallNPP_NewProc(nppfuncs->newp, pluginType, instance, mode, argc, argn, argv, saved);

  return rv;
}

NPError NPPEntryPointManager::callNPP_Destroy(NPP instance, NPSavedData** save, BOOL * last)
{
  NPPluginFuncs * nppfuncs = NULL;

  BOOL done = FALSE;

  for(PluginEntryPointList * eps = mEntryPoints; eps != NULL; eps = eps->next)
  {
    InstanceList * prev = NULL;
    for(InstanceList * instances = eps->instances; instances != NULL; instances = instances->next)
    {
      if(instances->instance == instance)
      {
        nppfuncs = &eps->realNPPFuncs;
        done = TRUE;

        // check if this is the last one
        if(eps->instances->next == NULL) 
          *last = TRUE;
        else
        {
          // deassociate instance if this is not the last one
          // last instance will be needed to find corresponding shutdown proc
          if(prev)
            prev->next = instances->next;
          else
            eps->instances = instances->next;

          delete instances;
        }

        break;
      }
      prev = instances;
    }
    if(done)
      break;
  }

  if(!nppfuncs || !nppfuncs->destroy)
    return NPERR_GENERIC_ERROR;

  return CallNPP_DestroyProc(nppfuncs->destroy, instance, save);
}

NPError NPPEntryPointManager::callNPP_SetWindow(NPP instance, NPWindow* window)
{
  NPPluginFuncs * nppfuncs = findEntryPointsForInstance(instance);
  if(!nppfuncs || !nppfuncs->setwindow)
    return NPERR_GENERIC_ERROR;

  return CallNPP_SetWindowProc(nppfuncs->setwindow, instance, window);
}

NPError NPPEntryPointManager::callNPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype)
{
  NPPluginFuncs * nppfuncs = findEntryPointsForInstance(instance);
  if(!nppfuncs || !nppfuncs->newstream)
    return NPERR_GENERIC_ERROR;

  return CallNPP_NewStreamProc(nppfuncs->newstream, instance, type, stream, seekable, stype);
}

NPError NPPEntryPointManager::callNPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
  NPPluginFuncs * nppfuncs = findEntryPointsForInstance(instance);
  if(!nppfuncs || !nppfuncs->destroystream)
    return NPERR_GENERIC_ERROR;

  return CallNPP_DestroyStreamProc(nppfuncs->destroystream, instance, stream, reason);
}

int32 NPPEntryPointManager::callNPP_WriteReady(NPP instance, NPStream* stream)
{
  NPPluginFuncs * nppfuncs = findEntryPointsForInstance(instance);
  if(!nppfuncs || !nppfuncs->writeready)
    return NPERR_GENERIC_ERROR;

  return CallNPP_WriteReadyProc(nppfuncs->writeready, instance, stream);
}

int32 NPPEntryPointManager::callNPP_Write(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer)
{
  NPPluginFuncs * nppfuncs = findEntryPointsForInstance(instance);
  if(!nppfuncs || !nppfuncs->write)
    return NPERR_GENERIC_ERROR;

  return CallNPP_WriteProc(nppfuncs->write, instance, stream, offset, len, buffer);
}

void NPPEntryPointManager::callNPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
  NPPluginFuncs * nppfuncs = findEntryPointsForInstance(instance);
  if(!nppfuncs || !nppfuncs->asfile)
    return;

  CallNPP_StreamAsFileProc(nppfuncs->asfile, instance, stream, fname);
}

void NPPEntryPointManager::callNPP_Print(NPP instance, NPPrint* platformPrint)
{
  NPPluginFuncs * nppfuncs = findEntryPointsForInstance(instance);
  if(!nppfuncs || !nppfuncs->print)
    return;

  CallNPP_PrintProc(nppfuncs->print, instance, platformPrint);
}

void NPPEntryPointManager::callNPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{
  NPPluginFuncs * nppfuncs = findEntryPointsForInstance(instance);
  if(!nppfuncs || !nppfuncs->urlnotify)
    return;

  CallNPP_URLNotifyProc(nppfuncs->urlnotify, instance, url, reason, notifyData);
}

NPError NPPEntryPointManager::callNPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
  NPPluginFuncs * nppfuncs = findEntryPointsForInstance(instance);
  if(!nppfuncs || !nppfuncs->getvalue)
    return NPERR_GENERIC_ERROR;

  return CallNPP_GetValueProc(nppfuncs->getvalue, instance, variable, value);
}

NPError NPPEntryPointManager::callNPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
  NPPluginFuncs * nppfuncs = findEntryPointsForInstance(instance);
  if(!nppfuncs || !nppfuncs->setvalue)
    return NPERR_GENERIC_ERROR;

  return CallNPP_SetValueProc(nppfuncs->setvalue, instance, variable, value);
}

int16 NPPEntryPointManager::callNPP_HandleEvent(NPP instance, void* event)
{
  NPPluginFuncs * nppfuncs = findEntryPointsForInstance(instance);
  if(!nppfuncs || !nppfuncs->event)
    return NPERR_GENERIC_ERROR;

  return CallNPP_HandleEventProc(nppfuncs->event, instance, event);
}
