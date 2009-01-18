/* ***** BEGIN LICENSE BLOCK *****
 * 
 * Copyright (c) 2008, Mozilla Corporation
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of the Mozilla Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Contributor(s):
 *   Dave Townsend <dtownsend@oxymoronical.com>
 *   Josh Aas <josh@mozilla.com>
 * 
 * ***** END LICENSE BLOCK ***** */

#include "nptest.h"
#include "nptest_utils.h"
#include "nptest_platform.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define PLUGIN_NAME        "Test Plug-in"
#define PLUGIN_DESCRIPTION "Plug-in for testing purposes."
#define PLUGIN_VERSION     "1.0.0.0"

//
// static data
//

static NPNetscapeFuncs* sBrowserFuncs = NULL;
static NPClass sNPClass;

//
// identifiers
//

#define IDENTIFIER_TO_STRING_TEST_METHOD 0
#define NUM_METHOD_IDENTIFIERS           1

static NPIdentifier sPluginMethodIdentifiers[NUM_METHOD_IDENTIFIERS];
static const NPUTF8 *sPluginMethodIdentifierNames[NUM_METHOD_IDENTIFIERS] = {
  "identifierToStringTest",
};

static bool sIdentifiersInitialized = false;

static void initializeIdentifiers()
{
  if (!sIdentifiersInitialized) {
    NPN_GetStringIdentifiers(sPluginMethodIdentifierNames, NUM_METHOD_IDENTIFIERS, sPluginMethodIdentifiers);
    sIdentifiersInitialized = true;    
  }
}

static void clearIdentifiers()
{
  memset(sPluginMethodIdentifierNames, 0, NUM_METHOD_IDENTIFIERS * sizeof(NPIdentifier));
  sIdentifiersInitialized = false;
}

//
// function signatures
//

bool identifierToStringTest(const NPVariant* args, uint32_t argCount, NPVariant* result);

NPObject* scriptableAllocate(NPP npp, NPClass* aClass);
void scriptableDeallocate(NPObject* npobj);
void scriptableInvalidate(NPObject* npobj);
bool scriptableHasMethod(NPObject* npobj, NPIdentifier name);
bool scriptableInvoke(NPObject* npobj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result);
bool scriptableInvokeDefault(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
bool scriptableHasProperty(NPObject* npobj, NPIdentifier name);
bool scriptableGetProperty(NPObject* npobj, NPIdentifier name, NPVariant* result);
bool scriptableSetProperty(NPObject* npobj, NPIdentifier name, const NPVariant* value);
bool scriptableRemoveProperty(NPObject* npobj, NPIdentifier name);
bool scriptableEnumerate(NPObject* npobj, NPIdentifier** identifier, uint32_t* count);
bool scriptableConstruct(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);

//
// npapi plugin functions
//

#ifdef XP_UNIX
NP_EXPORT(char*)
NP_GetPluginVersion()
{
  return PLUGIN_VERSION;
}
#endif

#if defined(XP_UNIX)
NP_EXPORT(char*) NP_GetMIMEDescription()
#elif defined(XP_WIN)
char* NP_GetMIMEDescription()
#endif
{
  return "application/x-test:tst:Test mimetype";
}

#ifdef XP_UNIX
NP_EXPORT(NPError)
NP_GetValue(void* future, NPPVariable aVariable, void* aValue) {
  switch (aVariable) {
    case NPPVpluginNameString:
      *((char**)aValue) = PLUGIN_NAME;
      break;
    case NPPVpluginDescriptionString:
      *((char**)aValue) = PLUGIN_DESCRIPTION;
      break;
    default:
      return NPERR_INVALID_PARAM;
      break;
  }
  return NPERR_NO_ERROR;
}
#endif

static void fillPluginFunctionTable(NPPluginFuncs* pFuncs)
{
  pFuncs->version = 11;
  pFuncs->size = sizeof(*pFuncs);
  pFuncs->newp = NPP_New;
  pFuncs->destroy = NPP_Destroy;
  pFuncs->setwindow = NPP_SetWindow;
  pFuncs->newstream = NPP_NewStream;
  pFuncs->destroystream = NPP_DestroyStream;
  pFuncs->asfile = NPP_StreamAsFile;
  pFuncs->writeready = NPP_WriteReady;
  pFuncs->write = NPP_Write;
  pFuncs->print = NPP_Print;
  pFuncs->event = NPP_HandleEvent;
  pFuncs->urlnotify = NPP_URLNotify;
  pFuncs->getvalue = NPP_GetValue;
  pFuncs->setvalue = NPP_SetValue;
}

#if defined(XP_MACOSX)
NP_EXPORT(NPError) NP_Initialize(NPNetscapeFuncs* bFuncs)
#elif defined(XP_WIN)
NPError OSCALL NP_Initialize(NPNetscapeFuncs* bFuncs)
#elif defined(XP_UNIX)
NP_EXPORT(NPError) NP_Initialize(NPNetscapeFuncs* bFuncs, NPPluginFuncs* pFuncs)
#endif
{
  sBrowserFuncs = bFuncs;

  initializeIdentifiers();

  memset(&sNPClass, 0, sizeof(NPClass));
  sNPClass.structVersion =  NP_CLASS_STRUCT_VERSION;
  sNPClass.allocate =       (NPAllocateFunctionPtr)scriptableAllocate;
  sNPClass.deallocate =     (NPDeallocateFunctionPtr)scriptableDeallocate;
  sNPClass.invalidate =     (NPInvalidateFunctionPtr)scriptableInvalidate;
  sNPClass.hasMethod =      (NPHasMethodFunctionPtr)scriptableHasMethod;
  sNPClass.invoke =         (NPInvokeFunctionPtr)scriptableInvoke;
  sNPClass.invokeDefault =  (NPInvokeDefaultFunctionPtr)scriptableInvokeDefault;
  sNPClass.hasProperty =    (NPHasPropertyFunctionPtr)scriptableHasProperty;
  sNPClass.getProperty =    (NPGetPropertyFunctionPtr)scriptableGetProperty;
  sNPClass.setProperty =    (NPSetPropertyFunctionPtr)scriptableSetProperty;
  sNPClass.removeProperty = (NPRemovePropertyFunctionPtr)scriptableRemoveProperty;
  sNPClass.enumerate =      (NPEnumerationFunctionPtr)scriptableEnumerate;
  sNPClass.construct =      (NPConstructFunctionPtr)scriptableConstruct;

#if defined(XP_UNIX) && !defined(XP_MACOSX)
  fillPluginFunctionTable(pFuncs);
#endif

  return NPERR_NO_ERROR;
}

#if defined(XP_MACOSX)
NP_EXPORT(NPError) NP_GetEntryPoints(NPPluginFuncs* pFuncs)
#elif defined(XP_WIN)
NPError OSCALL NP_GetEntryPoints(NPPluginFuncs* pFuncs)
#endif
#if defined(XP_MACOSX) || defined(XP_WIN)
{
  fillPluginFunctionTable(pFuncs);
  return NPERR_NO_ERROR;
}
#endif

#if defined(XP_UNIX)
NP_EXPORT(NPError) NP_Shutdown()
#elif defined(XP_WIN)
NPError OSCALL NP_Shutdown()
#endif
{
  clearIdentifiers();

  return NPERR_NO_ERROR;
}

NPError
NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved)
{
  NPN_SetValue(instance, NPPVpluginWindowBool, NULL);

  // set up our our instance data
  InstanceData* instanceData = (InstanceData*)malloc(sizeof(InstanceData));
  if (!instanceData)
    return NPERR_OUT_OF_MEMORY_ERROR;
  memset(instanceData, 0, sizeof(InstanceData));
  instanceData->npp = instance;
  instance->pdata = instanceData;

  TestNPObject* scriptableObject = (TestNPObject*)NPN_CreateObject(instance, &sNPClass);
  if (!scriptableObject) {
    printf("NPN_CreateObject failed to create an object, can't create a plugin instance\n");
    return NPERR_GENERIC_ERROR;
  }
  NPN_RetainObject(scriptableObject);
  scriptableObject->npp = instance;
  instanceData->scriptableObject = scriptableObject;

  scriptableObject->drawMode = DM_DEFAULT;
  scriptableObject->drawColor = 0;

  // handle extra params
  for (int i = 0; i < argc; i++) {
    if (strcmp(argn[i], "drawmode") == 0) {
      if (strcmp(argv[i], "solid") == 0)
        scriptableObject->drawMode = DM_SOLID_COLOR;    
    }
    else if (strcmp(argn[i], "color") == 0) {
      scriptableObject->drawColor = parseHexColor(argv[i]);
    }
  }

  // do platform-specific initialization
  NPError err = pluginInstanceInit(instanceData);
  if (err != NPERR_NO_ERROR)
    return err;

  return NPERR_NO_ERROR;
}

NPError
NPP_Destroy(NPP instance, NPSavedData** save)
{
  InstanceData* instanceData = (InstanceData*)(instance->pdata);
  NPN_ReleaseObject(instanceData->scriptableObject);
  free(instanceData);
  return NPERR_NO_ERROR;
}

NPError
NPP_SetWindow(NPP instance, NPWindow* window)
{
  InstanceData* instanceData = (InstanceData*)(instance->pdata);
  instanceData->window = *window;
  return NPERR_NO_ERROR;
}

NPError
NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype)
{
  *stype = NP_ASFILEONLY;
  return NPERR_NO_ERROR;
}

NPError
NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
  return NPERR_NO_ERROR;
}

int32_t
NPP_WriteReady(NPP instance, NPStream* stream)
{
  return 0;
}

int32_t
NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer)
{
  return 0;
}

void
NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
}

void
NPP_Print(NPP instance, NPPrint* platformPrint)
{
}

int16_t
NPP_HandleEvent(NPP instance, void* event)
{
  InstanceData* instanceData = (InstanceData*)(instance->pdata);
  return pluginHandleEvent(instanceData, event);
}

void
NPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{
}

NPError
NPP_GetValue(NPP instance, NPPVariable variable, void* value)
{
  if (variable == NPPVpluginScriptableNPObject) {
    NPObject* object = ((InstanceData*)instance->pdata)->scriptableObject;
    NPN_RetainObject(object);
    *((NPObject**)value) = object;
    return NPERR_NO_ERROR;
  }

  return NPERR_GENERIC_ERROR;
}

NPError
NPP_SetValue(NPP instance, NPNVariable variable, void* value)
{
  return NPERR_GENERIC_ERROR;
}

//
// npapi browser functions
//

bool
NPN_SetProperty(NPP instance, NPObject* obj, NPIdentifier propertyName, const NPVariant* value)
{
  return sBrowserFuncs->setproperty(instance, obj, propertyName, value);
}

NPIdentifier
NPN_GetIntIdentifier(int32_t intid)
{
  return sBrowserFuncs->getintidentifier(intid);
}

NPIdentifier
NPN_GetStringIdentifier(const NPUTF8* name)
{
  return sBrowserFuncs->getstringidentifier(name);
}

void
NPN_GetStringIdentifiers(const NPUTF8 **names, int32_t nameCount, NPIdentifier *identifiers)
{
  return sBrowserFuncs->getstringidentifiers(names, nameCount, identifiers);
}

NPUTF8*
NPN_UTF8FromIdentifier(NPIdentifier identifier)
{
  return sBrowserFuncs->utf8fromidentifier(identifier);
}

int32_t
NPN_IntFromIdentifier(NPIdentifier identifier)
{
  return sBrowserFuncs->intfromidentifier(identifier);
}

NPError
NPN_GetValue(NPP instance, NPNVariable variable, void* value)
{
  return sBrowserFuncs->getvalue(instance, variable, value);
}

NPError
NPN_SetValue(NPP instance, NPPVariable variable, void* value)
{
  return sBrowserFuncs->setvalue(instance, variable, value);
}

bool
NPN_HasProperty(NPP instance, NPObject* obj, NPIdentifier propertyName)
{
  return sBrowserFuncs->hasproperty(instance, obj, propertyName);
}

NPObject*
NPN_CreateObject(NPP instance, NPClass* aClass)
{
  return sBrowserFuncs->createobject(instance, aClass);
}

const char*
NPN_UserAgent(NPP instance)
{
  return sBrowserFuncs->uagent(instance);
}

NPObject*
NPN_RetainObject(NPObject* obj)
{
  return sBrowserFuncs->retainobject(obj);
}

void
NPN_ReleaseObject(NPObject* obj)
{
  return sBrowserFuncs->releaseobject(obj);
}

void*
NPN_MemAlloc(uint32_t size)
{
  return sBrowserFuncs->memalloc(size);
}

void
NPN_MemFree(void* ptr)
{
  return sBrowserFuncs->memfree(ptr);
}

//
// npruntime object functions
//

NPObject*
scriptableAllocate(NPP npp, NPClass* aClass)
{
  TestNPObject* object = (TestNPObject*)NPN_MemAlloc(sizeof(TestNPObject));
  if (!object)
    return NULL;
  memset(object, 0, sizeof(TestNPObject));
  return object;
}

void
scriptableDeallocate(NPObject* npobj)
{
  NPN_MemFree(npobj);
}

void
scriptableInvalidate(NPObject* npobj)
{
}

bool
scriptableHasMethod(NPObject* npobj, NPIdentifier name)
{
  for (int i = 0; i < NUM_METHOD_IDENTIFIERS; i++) {
    if (name == sPluginMethodIdentifiers[i])
      return true;
  }
  return false;
}

bool
scriptableInvoke(NPObject* npobj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (name == sPluginMethodIdentifiers[IDENTIFIER_TO_STRING_TEST_METHOD])
    return identifierToStringTest(args, argCount, result);
  return false;
}

bool
scriptableInvokeDefault(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  return false;
}

bool
scriptableHasProperty(NPObject* npobj, NPIdentifier name)
{
  return false;
}

bool
scriptableGetProperty(NPObject* npobj, NPIdentifier name, NPVariant* result)
{
  return false;
}

bool
scriptableSetProperty(NPObject* npobj, NPIdentifier name, const NPVariant* value)
{
  return false;
}

bool
scriptableRemoveProperty(NPObject* npobj, NPIdentifier name)
{
  return false;
}

bool
scriptableEnumerate(NPObject* npobj, NPIdentifier** identifier, uint32_t* count)
{
  return false;
}

bool
scriptableConstruct(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  return false;
}

//
// test functions
//

bool
identifierToStringTest(const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 1)
    return false;
  NPIdentifier identifier = variantToIdentifier(args[0]);
  if (!identifier)
    return false;
  NPUTF8* utf8String = NPN_UTF8FromIdentifier(identifier);
  if (!utf8String)
    return false;
  STRINGZ_TO_NPVARIANT(utf8String, *result);
  return true;
}
