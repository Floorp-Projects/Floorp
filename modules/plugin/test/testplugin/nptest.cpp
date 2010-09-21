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
#include <iostream>
#include <string>
#include <sstream>

#ifdef XP_WIN
#include <process.h>
#include <float.h>
#include <windows.h>
#define getpid _getpid
#else
#include <unistd.h>
#include <pthread.h>
#endif

 using namespace std;

#define PLUGIN_NAME        "Test Plug-in"
#define PLUGIN_DESCRIPTION "Plug-in for testing purposes."
#define PLUGIN_VERSION     "1.0.0.0"

#define ARRAY_LENGTH(a) (sizeof(a)/sizeof(a[0]))
#define STATIC_ASSERT(condition)                                \
    extern void np_static_assert(int arg[(condition) ? 1 : -1])

static char sPluginName[] = PLUGIN_NAME; 
static char sPluginDescription[] = PLUGIN_DESCRIPTION;
static char sPluginVersion[] = PLUGIN_VERSION;

//
// Intentional crash
//

int gCrashCount = 0;

void
NoteIntentionalCrash()
{
  char* bloatLog = getenv("XPCOM_MEM_BLOAT_LOG");
  if (bloatLog) {
    char* logExt = strstr(bloatLog, ".log");
    if (logExt) {
      bloatLog[strlen(bloatLog) - strlen(logExt)] = '\0';
    }
    ostringstream bloatName;
    bloatName << bloatLog << "_plugin_pid" << getpid();
    if (logExt) {
      bloatName << ".log";
    }
    FILE* processfd = fopen(bloatName.str().c_str(), "a");
    fprintf(processfd, "==> process %d will purposefully crash\n", getpid());
    fclose(processfd);
  }
}

static void Crash()
{
  int *pi = NULL;
  *pi = 55; // Crash dereferencing null pointer
  ++gCrashCount;
}

static void
IntentionalCrash()
{
  NoteIntentionalCrash();
  Crash();
}

//
// static data
//

static NPNetscapeFuncs* sBrowserFuncs = NULL;
static NPClass sNPClass;

static void
testplugin_URLNotify(NPP instance, const char* url, NPReason reason,
                     void* notifyData);
void
asyncCallback(void* cookie);

//
// identifiers
//

typedef bool (* ScriptableFunction)
  (NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);

static bool npnEvaluateTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool npnInvokeTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool npnInvokeDefaultTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool setUndefinedValueTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool identifierToStringTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool timerTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool queryPrivateModeState(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool lastReportedPrivateModeState(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool hasWidget(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getEdge(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getClipRegionRectCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getClipRegionRectEdge(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool startWatchingInstanceCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getInstanceCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool stopWatchingInstanceCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getLastMouseX(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getLastMouseY(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getPaintCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getWidthAtLastPaint(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool setInvalidateDuringPaint(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getError(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool doInternalConsistencyCheck(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool setColor(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool throwExceptionNextInvoke(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool convertPointX(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool convertPointY(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool streamTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool crashPlugin(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool crashOnDestroy(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getObjectValue(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool checkObjectValue(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool enableFPExceptions(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool setCookie(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getCookie(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getAuthInfo(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool asyncCallbackTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool checkGCRace(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool hangPlugin(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getClipboardText(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool callOnDestroy(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool reinitWidget(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool crashPluginInNestedLoop(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool propertyAndMethod(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getTopLevelWindowActivationState(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getTopLevelWindowActivationEventCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getFocusState(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getFocusEventCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getEventModel(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool getReflector(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);

static const NPUTF8* sPluginMethodIdentifierNames[] = {
  "npnEvaluateTest",
  "npnInvokeTest",
  "npnInvokeDefaultTest",
  "setUndefinedValueTest",
  "identifierToStringTest",
  "timerTest",
  "queryPrivateModeState",
  "lastReportedPrivateModeState",
  "hasWidget",
  "getEdge",
  "getClipRegionRectCount",
  "getClipRegionRectEdge",
  "startWatchingInstanceCount",
  "getInstanceCount",
  "stopWatchingInstanceCount",
  "getLastMouseX",
  "getLastMouseY",
  "getPaintCount",
  "getWidthAtLastPaint",
  "setInvalidateDuringPaint",
  "getError",
  "doInternalConsistencyCheck",
  "setColor",
  "throwExceptionNextInvoke",
  "convertPointX",
  "convertPointY",
  "streamTest",
  "crash",
  "crashOnDestroy",
  "getObjectValue",
  "checkObjectValue",
  "enableFPExceptions",
  "setCookie",
  "getCookie",
  "getAuthInfo",
  "asyncCallbackTest",
  "checkGCRace",
  "hang",
  "getClipboardText",
  "callOnDestroy",
  "reinitWidget",
  "crashInNestedLoop",
  "propertyAndMethod",
  "getTopLevelWindowActivationState",
  "getTopLevelWindowActivationEventCount",
  "getFocusState",
  "getFocusEventCount",
  "getEventModel",
  "getReflector"
};
static NPIdentifier sPluginMethodIdentifiers[ARRAY_LENGTH(sPluginMethodIdentifierNames)];
static const ScriptableFunction sPluginMethodFunctions[] = {
  npnEvaluateTest,
  npnInvokeTest,
  npnInvokeDefaultTest,
  setUndefinedValueTest,
  identifierToStringTest,
  timerTest,
  queryPrivateModeState,
  lastReportedPrivateModeState,
  hasWidget,
  getEdge,
  getClipRegionRectCount,
  getClipRegionRectEdge,
  startWatchingInstanceCount,
  getInstanceCount,
  stopWatchingInstanceCount,
  getLastMouseX,
  getLastMouseY,
  getPaintCount,
  getWidthAtLastPaint,
  setInvalidateDuringPaint,
  getError,
  doInternalConsistencyCheck,
  setColor,
  throwExceptionNextInvoke,
  convertPointX,
  convertPointY,
  streamTest,
  crashPlugin,
  crashOnDestroy,
  getObjectValue,
  checkObjectValue,
  enableFPExceptions,
  setCookie,
  getCookie,
  getAuthInfo,
  asyncCallbackTest,
  checkGCRace,
  hangPlugin,
  getClipboardText,
  callOnDestroy,
  reinitWidget,
  crashPluginInNestedLoop,
  propertyAndMethod,
  getTopLevelWindowActivationState,
  getTopLevelWindowActivationEventCount,
  getFocusState,
  getFocusEventCount,
  getEventModel,
  getReflector
};

STATIC_ASSERT(ARRAY_LENGTH(sPluginMethodIdentifierNames) ==
              ARRAY_LENGTH(sPluginMethodFunctions));

static const NPUTF8* sPluginPropertyIdentifierNames[] = {
  "propertyAndMethod"
};
static NPIdentifier sPluginPropertyIdentifiers[ARRAY_LENGTH(sPluginPropertyIdentifierNames)];
static NPVariant sPluginPropertyValues[ARRAY_LENGTH(sPluginPropertyIdentifierNames)];

struct URLNotifyData
{
  const char* cookie;
  NPObject* callback;
  uint32_t size;
  char* data;
};

static URLNotifyData kNotifyData = {
  "static-cookie",
  NULL,
  0,
  NULL
};

static const char* SUCCESS_STRING = "pass";

static bool sIdentifiersInitialized = false;

static uint32_t timerEventCount = 0;

struct timerEvent {
  int32_t timerIdReceive;
  int32_t timerIdSchedule;
  uint32_t timerInterval;
  bool timerRepeat;
  int32_t timerIdUnschedule;
};
static timerEvent timerEvents[] = {
  {-1, 0, 200, false, -1},
  {0, 0, 400, false, -1},
  {0, 0, 200, true, -1},
  {0, 1, 100, true, -1},
  {1, -1, 0, false, -1},
  {0, -1, 0, false, -1},
  {1, -1, 0, false, -1},
  {1, -1, 0, false, -1},
  {0, -1, 0, false, 0},
  {1, 2, 600, false, 1},
  {2, -1, 0, false, 2},
};
static uint32_t totalTimerEvents = sizeof(timerEvents) / sizeof(timerEvent);

/**
 * Incremented for every startWatchingInstanceCount.
 */
static int32_t sCurrentInstanceCountWatchGeneration = 0;
/**
 * Tracks the number of instances created or destroyed since the last
 * startWatchingInstanceCount.
 */
static int32_t sInstanceCount = 0;
/**
 * True when we've had a startWatchingInstanceCount with no corresponding
 * stopWatchingInstanceCount.
 */
static bool sWatchingInstanceCount = false;

static void initializeIdentifiers()
{
  if (!sIdentifiersInitialized) {
    NPN_GetStringIdentifiers(sPluginMethodIdentifierNames,
        ARRAY_LENGTH(sPluginMethodIdentifierNames), sPluginMethodIdentifiers);
    NPN_GetStringIdentifiers(sPluginPropertyIdentifierNames,
        ARRAY_LENGTH(sPluginPropertyIdentifierNames), sPluginPropertyIdentifiers);

    sIdentifiersInitialized = true;    

    // Check whether NULL is handled in NPN_GetStringIdentifiers
    NPIdentifier IDList[2];
    static char const *const kIDNames[2] = { NULL, "setCookie" };
    NPN_GetStringIdentifiers(const_cast<const NPUTF8**>(kIDNames), 2, IDList);
  }
}

static void clearIdentifiers()
{
  memset(sPluginMethodIdentifiers, 0,
      ARRAY_LENGTH(sPluginMethodIdentifiers) * sizeof(NPIdentifier));
  memset(sPluginPropertyIdentifiers, 0,
      ARRAY_LENGTH(sPluginPropertyIdentifiers) * sizeof(NPIdentifier));

  sIdentifiersInitialized = false;
}

static void addRange(InstanceData* instanceData, const char* range)
{
  char rangestr[16];
  strncpy(rangestr, range, sizeof(rangestr));
  const char* str1 = strtok(rangestr, ",");
  const char* str2 = str1 ? strtok(NULL, ",") : NULL;
  if (str1 && str2) {
    TestRange* byterange = new TestRange;
    byterange->offset = atoi(str1);
    byterange->length = atoi(str2);
    byterange->waiting = true;
    byterange->next = instanceData->testrange;
    instanceData->testrange = byterange;
  }
}

static void sendBufferToFrame(NPP instance)
{
  InstanceData* instanceData = (InstanceData*)(instance->pdata);
  string outbuf;
  if (!instanceData->npnNewStream) outbuf = "data:text/html,";
  const char* buf = reinterpret_cast<char *>(instanceData->streamBuf);
  int32_t bufsize = instanceData->streamBufSize;
  if (instanceData->streamMode == NP_ASFILE || 
      instanceData->streamMode == NP_ASFILEONLY) {
    buf = reinterpret_cast<char *>(instanceData->fileBuf);
    bufsize = instanceData->fileBufSize;
  }
  if (instanceData->err.str().length() > 0) {
    outbuf.append(instanceData->err.str());
  }
  else if (bufsize > 0) {
    outbuf.append(buf);
  }
  else {
    outbuf.append("Error: no data in buffer");
  }
  
  if (instanceData->npnNewStream &&
      instanceData->err.str().length() == 0) {
    char typeHTML[] = "text/html";
    NPStream* stream;
    printf("calling NPN_NewStream...");
    NPError err = NPN_NewStream(instance, typeHTML, 
        instanceData->frame.c_str(), &stream);
    printf("return value %d\n", err);
    if (err != NPERR_NO_ERROR) {
      instanceData->err << "NPN_NewStream returned " << err;
      return;
    }
    
    int32_t bytesToWrite = outbuf.length();
    int32_t bytesWritten = 0;
    while ((bytesToWrite - bytesWritten) > 0) {
      int32_t numBytes = (bytesToWrite - bytesWritten) < 
          instanceData->streamChunkSize ?
          bytesToWrite - bytesWritten : instanceData->streamChunkSize;
      int32_t written = NPN_Write(instance, stream,
          numBytes, (void*)(outbuf.c_str() + bytesWritten));
      if (written <= 0) {
        instanceData->err << "NPN_Write returned " << written;
        break;
      }
      bytesWritten += numBytes;
      printf("%d bytes written, total %d\n", written, bytesWritten);
    }
    err = NPN_DestroyStream(instance, stream, NPRES_DONE);
    if (err != NPERR_NO_ERROR) {
      instanceData->err << "NPN_DestroyStream returned " << err;
    }
  }
  else {
    // Convert CRLF to LF, and escape most other non-alphanumeric chars.
    for (size_t i = 0; i < outbuf.length(); i++) {
      if (outbuf[i] == '\n') {
        outbuf.replace(i, 1, "%0a");
        i += 2;
      }
      else if (outbuf[i] == '\r') {
        outbuf.replace(i, 1, "");
        i -= 1;
      }
      else {
        int ascii = outbuf[i];
        if (!((ascii >= ',' && ascii <= ';') ||
              (ascii >= 'A' && ascii <= 'Z') ||
              (ascii >= 'a' && ascii <= 'z'))) {
          char hex[8];
          sprintf(hex, "%%%x", ascii);
          outbuf.replace(i, 1, hex);
          i += 2;
        }
      }
    }

    NPError err = NPN_GetURL(instance, outbuf.c_str(), 
                             instanceData->frame.c_str());
    if (err != NPERR_NO_ERROR) {
      instanceData->err << "NPN_GetURL returned " << err;
    }
  }
}

TestFunction
getFuncFromString(const char* funcname)
{
  FunctionTable funcTable[] = 
    {
      { FUNCTION_NPP_NEWSTREAM, "npp_newstream" },
      { FUNCTION_NPP_WRITEREADY, "npp_writeready" },
      { FUNCTION_NPP_WRITE, "npp_write" },
      { FUNCTION_NPP_DESTROYSTREAM, "npp_destroystream" },
      { FUNCTION_NPP_WRITE_RPC, "npp_write_rpc" },
      { FUNCTION_NONE, NULL }
    };
  int32_t i = 0;
  while(funcTable[i].funcName) {
    if (!strcmp(funcname, funcTable[i].funcName)) return funcTable[i].funcId;
    i++;
  }
  return FUNCTION_NONE;
}

static void
DuplicateNPVariant(NPVariant& aDest, const NPVariant& aSrc)
{
  if (NPVARIANT_IS_STRING(aSrc)) {
    NPString src = NPVARIANT_TO_STRING(aSrc);
    char* buf = new char[src.UTF8Length];
    strncpy(buf, src.UTF8Characters, src.UTF8Length);
    STRINGN_TO_NPVARIANT(buf, src.UTF8Length, aDest);
  }
  else if (NPVARIANT_IS_OBJECT(aSrc)) {
    NPObject* obj =
      NPN_RetainObject(NPVARIANT_TO_OBJECT(aSrc));
    OBJECT_TO_NPVARIANT(obj, aDest);
  }
  else {
    aDest = aSrc;
  }
}

//
// function signatures
//

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
  return sPluginVersion;
}
#endif

static char sMimeDescription[] = "application/x-test:tst:Test mimetype";

#if defined(XP_UNIX)
NP_EXPORT(char*) NP_GetMIMEDescription()
#elif defined(XP_WIN) || defined(XP_OS2)
char* NP_GetMIMEDescription()
#endif
{
  return sMimeDescription;
}

#ifdef XP_UNIX
NP_EXPORT(NPError)
NP_GetValue(void* future, NPPVariable aVariable, void* aValue) {
  switch (aVariable) {
    case NPPVpluginNameString:
      *((char**)aValue) = sPluginName;
      break;
    case NPPVpluginDescriptionString:
      *((char**)aValue) = sPluginDescription;
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
  pFuncs->urlnotify = testplugin_URLNotify;
  pFuncs->getvalue = NPP_GetValue;
  pFuncs->setvalue = NPP_SetValue;
}

#if defined(XP_MACOSX)
NP_EXPORT(NPError) NP_Initialize(NPNetscapeFuncs* bFuncs)
#elif defined(XP_WIN) || defined(XP_OS2)
NPError OSCALL NP_Initialize(NPNetscapeFuncs* bFuncs)
#elif defined(XP_UNIX)
NP_EXPORT(NPError) NP_Initialize(NPNetscapeFuncs* bFuncs, NPPluginFuncs* pFuncs)
#endif
{
  sBrowserFuncs = bFuncs;

  initializeIdentifiers();

  for (unsigned int i = 0; i < ARRAY_LENGTH(sPluginPropertyValues); i++) {
    VOID_TO_NPVARIANT(sPluginPropertyValues[i]);
  }

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
#elif defined(XP_WIN) || defined(XP_OS2)
NPError OSCALL NP_GetEntryPoints(NPPluginFuncs* pFuncs)
#endif
#if defined(XP_MACOSX) || defined(XP_WIN) || defined(XP_OS2)
{
  fillPluginFunctionTable(pFuncs);
  return NPERR_NO_ERROR;
}
#endif

#if defined(XP_UNIX)
NP_EXPORT(NPError) NP_Shutdown()
#elif defined(XP_WIN) || defined(XP_OS2)
NPError OSCALL NP_Shutdown()
#endif
{
  clearIdentifiers();

  for (unsigned int i = 0; i < ARRAY_LENGTH(sPluginPropertyValues); i++) {
    NPN_ReleaseVariantValue(&sPluginPropertyValues[i]);
  }

  return NPERR_NO_ERROR;
}

NPError
NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved)
{
  // Make sure our pdata field is NULL at this point. If it isn't, that
  // probably means the browser gave us uninitialized memory.
  if (instance->pdata) {
    printf("NPP_New called with non-NULL NPP->pdata pointer!\n");
    return NPERR_GENERIC_ERROR;
  }

  // Make sure we can render this plugin
  NPBool browserSupportsWindowless = false;
  NPN_GetValue(instance, NPNVSupportsWindowless, &browserSupportsWindowless);
  if (!browserSupportsWindowless && !pluginSupportsWindowMode()) {
    printf("Windowless mode not supported by the browser, windowed mode not supported by the plugin!\n");
    return NPERR_GENERIC_ERROR;
  }

  // set up our our instance data
  InstanceData* instanceData = new InstanceData;
  if (!instanceData)
    return NPERR_OUT_OF_MEMORY_ERROR;
  instanceData->npp = instance;
  instanceData->streamMode = NP_ASFILEONLY;
  instanceData->testFunction = FUNCTION_NONE;
  instanceData->functionToFail = FUNCTION_NONE;
  instanceData->failureCode = 0;
  instanceData->callOnDestroy = NULL;
  instanceData->streamChunkSize = 1024;
  instanceData->streamBuf = NULL;
  instanceData->streamBufSize = 0;
  instanceData->fileBuf = NULL;
  instanceData->fileBufSize = 0;
  instanceData->throwOnNextInvoke = false;
  instanceData->runScriptOnPaint = false;
  instanceData->testrange = NULL;
  instanceData->hasWidget = false;
  instanceData->npnNewStream = false;
  instanceData->invalidateDuringPaint = false;
  instanceData->writeCount = 0;
  instanceData->writeReadyCount = 0;
  memset(&instanceData->window, 0, sizeof(instanceData->window));
  instanceData->crashOnDestroy = false;
  instanceData->cleanupWidget = true; // only used by nptest_gtk
  instanceData->topLevelWindowActivationState = ACTIVATION_STATE_UNKNOWN;
  instanceData->topLevelWindowActivationEventCount = 0;
  instanceData->focusState = ACTIVATION_STATE_UNKNOWN;
  instanceData->focusEventCount = 0;
  instanceData->eventModel = 0;
  instance->pdata = instanceData;

  TestNPObject* scriptableObject = (TestNPObject*)NPN_CreateObject(instance, &sNPClass);
  if (!scriptableObject) {
    printf("NPN_CreateObject failed to create an object, can't create a plugin instance\n");
    free(instanceData);
    return NPERR_GENERIC_ERROR;
  }
  scriptableObject->npp = instance;
  scriptableObject->drawMode = DM_DEFAULT;
  scriptableObject->drawColor = 0;
  instanceData->scriptableObject = scriptableObject;

  instanceData->instanceCountWatchGeneration = sCurrentInstanceCountWatchGeneration;

  if (NP_FULL == mode) {
    instanceData->streamMode = NP_SEEK;
    instanceData->frame = "testframe";
    addRange(instanceData, "100,100");
  }

  bool requestWindow = false;
  // handle extra params
  for (int i = 0; i < argc; i++) {
    if (strcmp(argn[i], "drawmode") == 0) {
      if (strcmp(argv[i], "solid") == 0)
        scriptableObject->drawMode = DM_SOLID_COLOR;    
    }
    else if (strcmp(argn[i], "color") == 0) {
      scriptableObject->drawColor = parseHexColor(argv[i], strlen(argv[i]));
    }
    else if (strcmp(argn[i], "wmode") == 0) {
      if (strcmp(argv[i], "window") == 0) {
        requestWindow = true;
      }
    }
    if (strcmp(argn[i], "streammode") == 0) {
      if (strcmp(argv[i], "normal") == 0) {
        instanceData->streamMode = NP_NORMAL;
      }
      else if ((strcmp(argv[i], "asfile") == 0) &&
                strlen(argv[i]) == strlen("asfile")) {
        instanceData->streamMode = NP_ASFILE;
      }
      else if (strcmp(argv[i], "asfileonly") == 0) {
        instanceData->streamMode = NP_ASFILEONLY;
      }
      else if (strcmp(argv[i], "seek") == 0) {
        instanceData->streamMode = NP_SEEK;
      }
    }
    if (strcmp(argn[i], "streamchunksize") == 0) {
      instanceData->streamChunkSize = atoi(argv[i]);
    }
    if (strcmp(argn[i], "failurecode") == 0) {
      instanceData->failureCode = atoi(argv[i]);
    }
    if (strcmp(argn[i], "functiontofail") == 0) {
      instanceData->functionToFail = getFuncFromString(argv[i]);
    }
    if (strcmp(argn[i], "geturl") == 0) {
      instanceData->testUrl = argv[i];
      instanceData->testFunction = FUNCTION_NPP_GETURL;
    }
    if (strcmp(argn[i], "posturl") == 0) {
      instanceData->testUrl = argv[i];
      instanceData->testFunction = FUNCTION_NPP_POSTURL;
    }
    if (strcmp(argn[i], "geturlnotify") == 0) {
      instanceData->testUrl = argv[i];
      instanceData->testFunction = FUNCTION_NPP_GETURLNOTIFY;
    }
    if (strcmp(argn[i], "postmode") == 0) {
      if (strcmp(argv[i], "frame") == 0) {
        instanceData->postMode = POSTMODE_FRAME;
      }
      else if (strcmp(argv[i], "stream") == 0) {
        instanceData->postMode = POSTMODE_STREAM;
      }
    }
    if (strcmp(argn[i], "frame") == 0) {
      instanceData->frame = argv[i];
    }
    if (strcmp(argn[i], "range") == 0) {
      string range = argv[i];
      size_t semicolon = range.find(';');
      while (semicolon != string::npos) {
        addRange(instanceData, range.substr(0, semicolon).c_str());
        if (semicolon == range.length()) {
          range = "";
          break;
        }
        range = range.substr(semicolon + 1);
        semicolon = range.find(';');
      }
      if (range.length()) addRange(instanceData, range.c_str());
    }
    if (strcmp(argn[i], "newstream") == 0 &&
        strcmp(argv[i], "true") == 0) {
      instanceData->npnNewStream = true;
    }
    if (strcmp(argn[i], "newcrash") == 0) {
      IntentionalCrash();
    }
    if (strcmp(argn[i], "paintscript") == 0) {
      instanceData->runScriptOnPaint = true;
    }
    // "cleanupwidget" is only used with nptest_gtk, defaulting to true.  It
    // indicates whether the plugin should destroy its window in response to
    // NPP_Destroy (or let the platform destroy the widget when the parent
    // window gets destroyed).
    if (strcmp(argn[i], "cleanupwidget") == 0 &&
        strcmp(argv[i], "false") == 0) {
      instanceData->cleanupWidget = false;
    }
  }

  if (!browserSupportsWindowless || !pluginSupportsWindowlessMode()) {
    requestWindow = true;
  } else if (!pluginSupportsWindowMode()) {
    requestWindow = false;
  }
  if (requestWindow) {
    instanceData->hasWidget = true;
  } else {
    // NPPVpluginWindowBool should default to true, so we may as well
    // test that by not setting it in the window case
    NPN_SetValue(instance, NPPVpluginWindowBool, (void*)false);
  }

  if (scriptableObject->drawMode == DM_SOLID_COLOR &&
      (scriptableObject->drawColor & 0xFF000000) != 0xFF000000) {
    NPN_SetValue(instance, NPPVpluginTransparentBool, (void*)true);
  }

  instanceData->lastReportedPrivateModeState = false;
  instanceData->lastMouseX = instanceData->lastMouseY = -1;
  instanceData->widthAtLastPaint = -1;
  instanceData->paintCount = 0;

  // do platform-specific initialization
  NPError err = pluginInstanceInit(instanceData);
  if (err != NPERR_NO_ERROR) {
    NPN_ReleaseObject(scriptableObject);
    free(instanceData);
    return err;
  }

  NPVariant variantTrue;
  BOOLEAN_TO_NPVARIANT(true, variantTrue);

  // Set a property on NPNVPluginElementNPObject
  NPObject* o = NULL;
  err = NPN_GetValue(instance, NPNVPluginElementNPObject, &o);
  if (err == NPERR_NO_ERROR) {
    NPN_SetProperty(instance, o,
                    NPN_GetStringIdentifier("pluginFoundElement"), &variantTrue);
    NPN_ReleaseObject(o);
    o = NULL;
  }
  
  // Set a property on NPNVWindowNPObject
  err = NPN_GetValue(instance, NPNVWindowNPObject, &o);
  if (err == NPERR_NO_ERROR) {
    NPN_SetProperty(instance, o,
                    NPN_GetStringIdentifier("pluginFoundWindow"), &variantTrue);
    NPN_ReleaseObject(o);
    o = NULL;
  }

  ++sInstanceCount;

  if (instanceData->testFunction == FUNCTION_NPP_GETURL) {
    NPError err = NPN_GetURL(instance, instanceData->testUrl.c_str(), NULL);
    if (err != NPERR_NO_ERROR) {
      instanceData->err << "NPN_GetURL returned " << err;
    }
  }
  else if (instanceData->testFunction == FUNCTION_NPP_GETURLNOTIFY) {
    NPError err = NPN_GetURLNotify(instance, instanceData->testUrl.c_str(), 
                                   NULL, static_cast<void*>(&kNotifyData));
    if (err != NPERR_NO_ERROR) {
      instanceData->err << "NPN_GetURLNotify returned " << err;
    }
  }

  return NPERR_NO_ERROR;
}

NPError
NPP_Destroy(NPP instance, NPSavedData** save)
{
  printf("NPP_Destroy\n");
  InstanceData* instanceData = (InstanceData*)(instance->pdata);

  if (instanceData->crashOnDestroy)
    IntentionalCrash();

  if (instanceData->callOnDestroy) {
    NPVariant result;
    NPN_InvokeDefault(instance, instanceData->callOnDestroy, NULL, 0, &result);
    NPN_ReleaseVariantValue(&result);
    NPN_ReleaseObject(instanceData->callOnDestroy);
  }

  if (instanceData->streamBuf) {
    free(instanceData->streamBuf);
  }
  if (instanceData->fileBuf) {
    free(instanceData->fileBuf);
  }

  TestRange* currentrange = instanceData->testrange;
  TestRange* nextrange;
  while (currentrange != NULL) {
    nextrange = reinterpret_cast<TestRange*>(currentrange->next);
    delete currentrange;
    currentrange = nextrange;
  }

  pluginInstanceShutdown(instanceData);
  NPN_ReleaseObject(instanceData->scriptableObject);

  if (sCurrentInstanceCountWatchGeneration == instanceData->instanceCountWatchGeneration) {
    --sInstanceCount;
  }
  delete instanceData;

  return NPERR_NO_ERROR;
}

NPError
NPP_SetWindow(NPP instance, NPWindow* window)
{
  InstanceData* instanceData = (InstanceData*)(instance->pdata);

  if (instanceData->scriptableObject->drawMode == DM_DEFAULT &&
      (instanceData->window.width != window->width ||
       instanceData->window.height != window->height)) {
    NPRect r;
    r.left = r.top = 0;
    r.right = window->width;
    r.bottom = window->height;
    NPN_InvalidateRect(instance, &r);
  }

  void* oldWindow = instanceData->window.window;
  pluginDoSetWindow(instanceData, window);
  if (instanceData->hasWidget && oldWindow != instanceData->window.window) {
    pluginWidgetInit(instanceData, oldWindow);
  }
  return NPERR_NO_ERROR;
}

NPError
NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype)
{
  printf("NPP_NewStream\n");
  InstanceData* instanceData = (InstanceData*)(instance->pdata);
  
  if (instanceData->functionToFail == FUNCTION_NPP_NEWSTREAM &&
      instanceData->failureCode) {
    instanceData->err << SUCCESS_STRING;
    if (instanceData->frame.length() > 0) {
      sendBufferToFrame(instance);
    }
    return instanceData->failureCode;
  }

  if (stream->notifyData &&
      static_cast<URLNotifyData*>(stream->notifyData) != &kNotifyData) {
    // stream from streamTest
    *stype = NP_NORMAL;
  }
  else {
    *stype = instanceData->streamMode;

    if (instanceData->streamBufSize) {
      free(instanceData->streamBuf);
      instanceData->streamBufSize = 0;
      if (instanceData->testFunction == FUNCTION_NPP_POSTURL &&
          instanceData->postMode == POSTMODE_STREAM) {
        instanceData->testFunction = FUNCTION_NPP_GETURL;
      }
    }
  }
  return NPERR_NO_ERROR;
}

NPError
NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
  printf("NPP_DestroyStream\n");
  InstanceData* instanceData = (InstanceData*)(instance->pdata);

  if (instanceData->functionToFail == FUNCTION_NPP_NEWSTREAM) {
    instanceData->err << "NPP_DestroyStream called";
  }

  if (instanceData->functionToFail == FUNCTION_NPP_WRITE) {
    if (instanceData->writeCount == 1)
      instanceData->err << SUCCESS_STRING;
    else
      instanceData->err << "NPP_Write called after returning -1";
  }

  if (instanceData->functionToFail == FUNCTION_NPP_DESTROYSTREAM &&
      instanceData->failureCode) {
    instanceData->err << SUCCESS_STRING;
    if (instanceData->frame.length() > 0) {
      sendBufferToFrame(instance);
    }
    return instanceData->failureCode;
  }

  URLNotifyData* nd = static_cast<URLNotifyData*>(stream->notifyData);
  if (nd && nd != &kNotifyData) {
    return NPERR_NO_ERROR;
  }

  if (instanceData->streamMode == NP_ASFILE &&
      instanceData->functionToFail == FUNCTION_NONE) {
    if (!instanceData->streamBuf) {
      instanceData->err <<
        "Error: no data written with NPP_Write";
      return NPERR_GENERIC_ERROR;
    }

    if (!instanceData->fileBuf) {
      instanceData->err <<
        "Error: no data written with NPP_StreamAsFile";
      return NPERR_GENERIC_ERROR;
    }

    if (strcmp(reinterpret_cast<char *>(instanceData->fileBuf), 
               reinterpret_cast<char *>(instanceData->streamBuf))) {
      instanceData->err <<
        "Error: data passed to NPP_Write and NPP_StreamAsFile differed";
    }
  }
  if (instanceData->frame.length() > 0 && 
      instanceData->testFunction != FUNCTION_NPP_GETURLNOTIFY &&
      instanceData->testFunction != FUNCTION_NPP_POSTURL) {
    sendBufferToFrame(instance);
  }
  if (instanceData->testFunction == FUNCTION_NPP_POSTURL) {
    NPError err = NPN_PostURL(instance, instanceData->testUrl.c_str(), 
      instanceData->postMode == POSTMODE_FRAME ? instanceData->frame.c_str() : NULL, 
      instanceData->streamBufSize,
      reinterpret_cast<char *>(instanceData->streamBuf), false);
    if (err != NPERR_NO_ERROR)
      instanceData->err << "Error: NPN_PostURL returned error value " << err;
  }
  return NPERR_NO_ERROR;
}

int32_t
NPP_WriteReady(NPP instance, NPStream* stream)
{
  printf("NPP_WriteReady\n");
  InstanceData* instanceData = (InstanceData*)(instance->pdata);
  instanceData->writeReadyCount++;
  if (instanceData->functionToFail == FUNCTION_NPP_NEWSTREAM) {
    instanceData->err << "NPP_WriteReady called";
  }
  
  // temporarily disabled per bug 519870
  //if (instanceData->writeReadyCount == 1) {
  //  return 0;
  //}

  return instanceData->streamChunkSize;
}

int32_t
NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer)
{
  printf("NPP_Write, offset=%d, len=%d, end=%d\n", offset, len, stream->end);
  InstanceData* instanceData = (InstanceData*)(instance->pdata);
  instanceData->writeCount++;

  // temporarily disabled per bug 519870
  //if (instanceData->writeReadyCount == 1) {
  //  instanceData->err << "NPP_Write called even though NPP_WriteReady " <<
  //      "returned 0";
  //}

  if (instanceData->functionToFail == FUNCTION_NPP_WRITE_RPC) {
    // Make an RPC call and pretend to consume the data
    NPObject* windowObject = NULL;
    NPN_GetValue(instance, NPNVWindowNPObject, &windowObject);
    if (windowObject)
      NPN_ReleaseObject(windowObject);

    return len;
  }
  
  if (instanceData->functionToFail == FUNCTION_NPP_NEWSTREAM) {
    instanceData->err << "NPP_Write called";
  }

  if (instanceData->functionToFail == FUNCTION_NPP_WRITE) {
    return -1;
  }

  URLNotifyData* nd = static_cast<URLNotifyData*>(stream->notifyData);
  if (nd && nd != &kNotifyData) {
    uint32_t newsize = nd->size + len;
    nd->data = (char*) realloc(nd->data, newsize);
    memcpy(nd->data + nd->size, buffer, len);
    nd->size = newsize;
    return len;
  }

  // If the complete stream has been written, and we're doing a seek test,
  // then call NPN_RequestRead.
  if (instanceData->streamMode == NP_SEEK &&
      stream->end != 0 && 
      stream->end == ((uint32_t)instanceData->streamBufSize + len)) {
    // prevent recursion
    instanceData->streamMode = NP_NORMAL;

    if (instanceData->testrange != NULL) {
      NPError err = NPN_RequestRead(stream, instanceData->testrange);
      if (err != NPERR_NO_ERROR) {
        instanceData->err << "NPN_RequestRead returned error %d" << err;
      }
      printf("called NPN_RequestRead, return %d\n", err);
    }
  }

  char* streamBuf = reinterpret_cast<char *>(instanceData->streamBuf);
  if (offset + len <= instanceData->streamBufSize) {
    if (memcmp(buffer, streamBuf + offset, len)) {
      instanceData->err << 
          "Error: data written from NPN_RequestRead doesn't match";
    }
    else {
      printf("data matches!\n");
    }
    TestRange* range = instanceData->testrange;
    bool stillwaiting = false;
    while(range != NULL) {
      if (offset == range->offset &&
        (uint32_t)len == range->length) {
        range->waiting = false;
      }
      if (range->waiting) stillwaiting = true;
      range = reinterpret_cast<TestRange*>(range->next);
    }
    if (!stillwaiting) {
      NPError err = NPN_DestroyStream(instance, stream, NPRES_DONE);
      if (err != NPERR_NO_ERROR) {
        instanceData->err << "Error: NPN_DestroyStream returned " << err;
      }
    }
  }
  else {
    if (instanceData->streamBufSize == 0) {
      instanceData->streamBuf = malloc(len + 1);
      streamBuf = reinterpret_cast<char *>(instanceData->streamBuf);
    }
    else {
      instanceData->streamBuf = 
        realloc(reinterpret_cast<char *>(instanceData->streamBuf), 
        instanceData->streamBufSize + len + 1);
      streamBuf = reinterpret_cast<char *>(instanceData->streamBuf);
    }
    memcpy(streamBuf + instanceData->streamBufSize, buffer, len);
    instanceData->streamBufSize = instanceData->streamBufSize + len;
    streamBuf[instanceData->streamBufSize] = '\0';
  }
  return len;
}

void
NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
  printf("NPP_StreamAsFile, file=%s\n", fname);
  size_t size;

  InstanceData* instanceData = (InstanceData*)(instance->pdata);

  if (instanceData->functionToFail == FUNCTION_NPP_NEWSTREAM ||
      instanceData->functionToFail == FUNCTION_NPP_WRITE) {
    instanceData->err << "NPP_StreamAsFile called";
  }

  if (!fname)
    return;

  FILE *file = fopen(fname, "rb");
  if (file) {
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    instanceData->fileBuf = malloc((int32_t)size + 1);
    char* buf = reinterpret_cast<char *>(instanceData->fileBuf);
    fseek(file, 0, SEEK_SET);
    fread(instanceData->fileBuf, 1, size, file);
    fclose(file);
    buf[size] = '\0';
    instanceData->fileBufSize = (int32_t)size;
  }
  else {
    printf("Unable to open file\n");
    instanceData->err << "Unable to open file " << fname;
  }
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
testplugin_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{
  InstanceData* instanceData = (InstanceData*)(instance->pdata);
  URLNotifyData* ndata = static_cast<URLNotifyData*>(notifyData);

  printf("NPP_URLNotify called\n");
  if (&kNotifyData == ndata) {
    if (instanceData->frame.length() > 0) {
      sendBufferToFrame(instance);
    }
  }
  else if (!strcmp(ndata->cookie, "dynamic-cookie")) {
    NPVariant args[2];
    NPVariant result;
    INT32_TO_NPVARIANT(reason, args[0]);

    if (ndata->data)
      STRINGN_TO_NPVARIANT(ndata->data, ndata->size, args[1]);
    else
      STRINGN_TO_NPVARIANT("", 0, args[1]);

    NPN_InvokeDefault(instance, ndata->callback, args, 2, &result);
    NPN_ReleaseVariantValue(&result);

    // clean up the URLNotifyData
    NPN_ReleaseObject(ndata->callback);
    free(ndata->data);
    delete ndata;
  }
  else {
    printf("ERROR! NPP_URLNotify called with wrong cookie\n");
    instanceData->err << "Error: NPP_URLNotify called with wrong cookie";
  }
}

NPError
NPP_GetValue(NPP instance, NPPVariable variable, void* value)
{
  InstanceData* instanceData = (InstanceData*)instance->pdata;
  if (variable == NPPVpluginScriptableNPObject) {
    NPObject* object = instanceData->scriptableObject;
    NPN_RetainObject(object);
    *((NPObject**)value) = object;
    return NPERR_NO_ERROR;
  }
  if (variable == NPPVpluginNeedsXEmbed) {
    // Only relevant for X plugins
    *(NPBool*)value = instanceData->hasWidget;
    return NPERR_NO_ERROR;
  }

  return NPERR_GENERIC_ERROR;
}

NPError
NPP_SetValue(NPP instance, NPNVariable variable, void* value)
{
  if (variable == NPNVprivateModeBool) {
    InstanceData* instanceData = (InstanceData*)(instance->pdata);
    instanceData->lastReportedPrivateModeState = bool(*static_cast<NPBool*>(value));
    return NPERR_NO_ERROR;
  }
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

bool
NPN_IdentifierIsString(NPIdentifier identifier)
{
  return sBrowserFuncs->identifierisstring(identifier);
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

void
NPN_InvalidateRect(NPP instance, NPRect* rect)
{
  sBrowserFuncs->invalidaterect(instance, rect);
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

bool
NPN_Invoke(NPP npp, NPObject* obj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  return sBrowserFuncs->invoke(npp, obj, methodName, args, argCount, result);
}

bool
NPN_InvokeDefault(NPP npp, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  return sBrowserFuncs->invokeDefault(npp, obj, args, argCount, result);
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

char*
NPN_StrDup(const char* str)
{
  return strcpy((char*)sBrowserFuncs->memalloc(strlen(str) + 1), str);
}

void
NPN_MemFree(void* ptr)
{
  return sBrowserFuncs->memfree(ptr);
}

uint32_t
NPN_ScheduleTimer(NPP instance, uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID))
{
  return sBrowserFuncs->scheduletimer(instance, interval, repeat, timerFunc);
}

void
NPN_UnscheduleTimer(NPP instance, uint32_t timerID)
{
  return sBrowserFuncs->unscheduletimer(instance, timerID);
}

void
NPN_ReleaseVariantValue(NPVariant *variant)
{
  return sBrowserFuncs->releasevariantvalue(variant);
}

NPError
NPN_GetURLNotify(NPP instance, const char* url, const char* target, void* notifyData)
{
  return sBrowserFuncs->geturlnotify(instance, url, target, notifyData);
}

NPError
NPN_GetURL(NPP instance, const char* url, const char* target)
{
  return sBrowserFuncs->geturl(instance, url, target);
}

NPError
NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
  return sBrowserFuncs->requestread(stream, rangeList);
}

NPError
NPN_PostURLNotify(NPP instance, const char* url, 
                  const char* target, uint32_t len, 
                  const char* buf, NPBool file, void* notifyData)
{
  return sBrowserFuncs->posturlnotify(instance, url, target, len, buf, file, notifyData);
}

NPError 
NPN_PostURL(NPP instance, const char *url,
                    const char *target, uint32_t len,
                    const char *buf, NPBool file)
{
  return sBrowserFuncs->posturl(instance, url, target, len, buf, file);
}

NPError
NPN_DestroyStream(NPP instance, NPStream* stream, NPError reason)
{
  return sBrowserFuncs->destroystream(instance, stream, reason);
}

NPError
NPN_NewStream(NPP instance, 
              NPMIMEType  type, 
              const char* target,
              NPStream**  stream)
{
  return sBrowserFuncs->newstream(instance, type, target, stream);
}

int32_t
NPN_Write(NPP instance,
          NPStream* stream,
          int32_t len,
          void* buf)
{
  return sBrowserFuncs->write(instance, stream, len, buf);
}

bool
NPN_Enumerate(NPP instance,
              NPObject *npobj,
              NPIdentifier **identifiers,
              uint32_t *identifierCount)
{
  return sBrowserFuncs->enumerate(instance, npobj, identifiers, 
      identifierCount);
}

bool
NPN_GetProperty(NPP instance,
                NPObject *npobj,
                NPIdentifier propertyName,
                NPVariant *result)
{
  return sBrowserFuncs->getproperty(instance, npobj, propertyName, result);
}

bool
NPN_Evaluate(NPP instance, NPObject *npobj, NPString *script, NPVariant *result)
{
  return sBrowserFuncs->evaluate(instance, npobj, script, result);
}

void
NPN_SetException(NPObject *npobj, const NPUTF8 *message)
{
  return sBrowserFuncs->setexception(npobj, message);
}

NPBool
NPN_ConvertPoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace)
{
  return sBrowserFuncs->convertpoint(instance, sourceX, sourceY, sourceSpace, destX, destY, destSpace);
}

NPError
NPN_SetValueForURL(NPP instance, NPNURLVariable variable, const char *url, const char *value, uint32_t len)
{
  return sBrowserFuncs->setvalueforurl(instance, variable, url, value, len);
}

NPError
NPN_GetValueForURL(NPP instance, NPNURLVariable variable, const char *url, char **value, uint32_t *len)
{
  return sBrowserFuncs->getvalueforurl(instance, variable, url, value, len);
}

NPError
NPN_GetAuthenticationInfo(NPP instance,
                          const char *protocol,
                          const char *host, int32_t port,
                          const char *scheme,
                          const char *realm,
                          char **username, uint32_t *ulen,
                          char **password,
                          uint32_t *plen)
{
  return sBrowserFuncs->getauthenticationinfo(instance, protocol, host, port, scheme, realm,
      username, ulen, password, plen);
}

void
NPN_PluginThreadAsyncCall(NPP plugin, void (*func)(void*), void* userdata)
{
  return sBrowserFuncs->pluginthreadasynccall(plugin, func, userdata);
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
  for (int i = 0; i < int(ARRAY_LENGTH(sPluginMethodIdentifiers)); i++) {
    if (name == sPluginMethodIdentifiers[i])
      return true;
  }
  return false;
}

bool
scriptableInvoke(NPObject* npobj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  if (id->throwOnNextInvoke) {
    id->throwOnNextInvoke = false;
    if (argCount == 0) {
      NPN_SetException(npobj, NULL);
    }
    else {
      for (uint32_t i = 0; i < argCount; i++) {
        const NPString* argstr = &NPVARIANT_TO_STRING(args[i]);
        NPN_SetException(npobj, argstr->UTF8Characters);
      }
    }
    return false;
  }
  
  for (int i = 0; i < int(ARRAY_LENGTH(sPluginMethodIdentifiers)); i++) {
    if (name == sPluginMethodIdentifiers[i])
      return sPluginMethodFunctions[i](npobj, args, argCount, result);
  }
  return false;
}

bool
scriptableInvokeDefault(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  if (id->throwOnNextInvoke) {
    id->throwOnNextInvoke = false;
    if (argCount == 0) {
      NPN_SetException(npobj, NULL);
    }
    else {
      for (uint32_t i = 0; i < argCount; i++) {
        const NPString* argstr = &NPVARIANT_TO_STRING(args[i]);
        NPN_SetException(npobj, argstr->UTF8Characters);
      }
    }
    return false;
  }

  ostringstream value;
  value << PLUGIN_NAME;
  for (uint32_t i = 0; i < argCount; i++) {
    switch(args[i].type) {
      case NPVariantType_Int32:
        value << ";" << NPVARIANT_TO_INT32(args[i]);
        break;
      case NPVariantType_String: {
        const NPString* argstr = &NPVARIANT_TO_STRING(args[i]);
        value << ";" << argstr->UTF8Characters;
        break;
      }
      case NPVariantType_Void:
        value << ";undefined";
        break;
      case NPVariantType_Null:
        value << ";null";
        break;
      default:
        value << ";other";
    }
  }
  STRINGZ_TO_NPVARIANT(NPN_StrDup(value.str().c_str()), *result);
  return true;
}

bool
scriptableHasProperty(NPObject* npobj, NPIdentifier name)
{
  if (NPN_IdentifierIsString(name)) {
    if (NPN_GetStringIdentifier(NPN_UTF8FromIdentifier(name)) != name)
      Crash();
  }
  else {
    if (NPN_GetIntIdentifier(NPN_IntFromIdentifier(name)) != name)
      Crash();
  }
  for (int i = 0; i < int(ARRAY_LENGTH(sPluginPropertyIdentifiers)); i++) {
    if (name == sPluginPropertyIdentifiers[i])
      return true;
  }
  return false;
}

bool
scriptableGetProperty(NPObject* npobj, NPIdentifier name, NPVariant* result)
{
  for (int i = 0; i < int(ARRAY_LENGTH(sPluginPropertyIdentifiers)); i++) {
    if (name == sPluginPropertyIdentifiers[i]) {
      DuplicateNPVariant(*result, sPluginPropertyValues[i]);
      return true;
    }
  }
  return false;
}

bool
scriptableSetProperty(NPObject* npobj, NPIdentifier name, const NPVariant* value)
{
  for (int i = 0; i < int(ARRAY_LENGTH(sPluginPropertyIdentifiers)); i++) {
    if (name == sPluginPropertyIdentifiers[i]) {
      NPN_ReleaseVariantValue(&sPluginPropertyValues[i]);
      DuplicateNPVariant(sPluginPropertyValues[i], *value);
      return true;
    }
  }
  return false;
}

bool
scriptableRemoveProperty(NPObject* npobj, NPIdentifier name)
{
  for (int i = 0; i < int(ARRAY_LENGTH(sPluginPropertyIdentifiers)); i++) {
    if (name == sPluginPropertyIdentifiers[i]) {
      NPN_ReleaseVariantValue(&sPluginPropertyValues[i]);
      return true;
    }
  }
  return false;
}

bool
scriptableEnumerate(NPObject* npobj, NPIdentifier** identifier, uint32_t* count)
{
  const int bufsize = sizeof(NPIdentifier) * ARRAY_LENGTH(sPluginMethodIdentifierNames);
  NPIdentifier* ids = (NPIdentifier*) NPN_MemAlloc(bufsize);
  if (!ids)
    return false;

  memcpy(ids, sPluginMethodIdentifiers, bufsize);
  *identifier = ids;
  *count = ARRAY_LENGTH(sPluginMethodIdentifierNames);
  return true;
}

bool
scriptableConstruct(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  return false;
}

//
// test functions
//

static bool
compareVariants(NPP instance, const NPVariant* var1, const NPVariant* var2)
{
  bool success = true;
  InstanceData* id = static_cast<InstanceData*>(instance->pdata);
  if (var1->type != var2->type) {
    id->err << "Variant types don't match; got " << var1->type <<
        " expected " << var2->type;
    return false;
  }
  
  switch (var1->type) {
    case NPVariantType_Int32: {
        int32_t result = NPVARIANT_TO_INT32(*var1);
        int32_t expected = NPVARIANT_TO_INT32(*var2);
        if (result != expected) {
          id->err << "Variant values don't match; got " << result <<
              " expected " << expected;
          success = false;
        }
        break;
      }
    case NPVariantType_Double: {
        double result = NPVARIANT_TO_DOUBLE(*var1);
        double expected = NPVARIANT_TO_DOUBLE(*var2);
        if (result != expected) {
          id->err << "Variant values don't match (double)";
          success = false;
        }
        break;
      }
    case NPVariantType_Void: {
        // void values are always equivalent
        break;
      }
    case NPVariantType_Null: {
        // null values are always equivalent
        break;
      }
    case NPVariantType_Bool: {
        bool result = NPVARIANT_TO_BOOLEAN(*var1);
        bool expected = NPVARIANT_TO_BOOLEAN(*var2);
        if (result != expected) {
          id->err << "Variant values don't match (bool)";
          success = false;
        }
        break;
      }
    case NPVariantType_String: {
        const NPString* result = &NPVARIANT_TO_STRING(*var1);
        const NPString* expected = &NPVARIANT_TO_STRING(*var2);
        if (strcmp(result->UTF8Characters, expected->UTF8Characters) ||
            strlen(result->UTF8Characters) != strlen(expected->UTF8Characters)) {
          id->err << "Variant values don't match; got " << 
              result->UTF8Characters << " expected " << 
              expected->UTF8Characters;
          success = false;
        }
        break;
      }
    case NPVariantType_Object: {
        uint32_t i, identifierCount = 0;
        NPIdentifier* identifiers;
        NPObject* result = NPVARIANT_TO_OBJECT(*var1);
        NPObject* expected = NPVARIANT_TO_OBJECT(*var2);
        bool enumerate_result = NPN_Enumerate(instance, expected,
            &identifiers, &identifierCount);
        if (!enumerate_result) {
          id->err << "NPN_Enumerate failed";
          success = false;
        }
        for (i = 0; i < identifierCount; i++) {
          NPVariant resultVariant, expectedVariant;
          if (!NPN_GetProperty(instance, expected, identifiers[i],
              &expectedVariant)) {
            id->err << "NPN_GetProperty returned false";
            success = false;
          }
          else {
            if (!NPN_HasProperty(instance, result, identifiers[i])) {
              id->err << "NPN_HasProperty returned false";
              success = false;
            }
            else {
              if (!NPN_GetProperty(instance, result, identifiers[i],
              &resultVariant)) {
                id->err << "NPN_GetProperty 2 returned false";
                success = false;
              }
              else {
                success = compareVariants(instance, &resultVariant, 
                    &expectedVariant);
                NPN_ReleaseVariantValue(&expectedVariant);
              }
            }
            NPN_ReleaseVariantValue(&resultVariant);
          }
        }
        break;
      }
    default:
      id->err << "Unknown variant type";
      success = false;
  }
  
  return success;
}

static bool
throwExceptionNextInvoke(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  id->throwOnNextInvoke = true;
  BOOLEAN_TO_NPVARIANT(true, *result);
  return true;  
}

static bool
npnInvokeDefaultTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  bool success = false;
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
 
  NPObject* windowObject;
  NPN_GetValue(npp, NPNVWindowNPObject, &windowObject);
  if (!windowObject)
    return false;

  NPIdentifier objectIdentifier = variantToIdentifier(args[0]);
  if (!objectIdentifier)
    return false;

  NPVariant objectVariant;
  if (NPN_GetProperty(npp, windowObject, objectIdentifier,
      &objectVariant)) {
    if (NPVARIANT_IS_OBJECT(objectVariant)) {
      NPObject* selfObject = NPVARIANT_TO_OBJECT(objectVariant);
      if (selfObject != NULL) {
        NPVariant resultVariant;
        if (NPN_InvokeDefault(npp, selfObject, argCount > 1 ? &args[1] : NULL, 
            argCount - 1, &resultVariant)) {
          *result = resultVariant;
          success = true;
        }
      }
    }
    NPN_ReleaseVariantValue(&objectVariant);
  }

  NPN_ReleaseObject(windowObject);
  return success;
}

static bool
npnInvokeTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  id->err.str("");
  if (argCount < 2)
    return false;

  NPIdentifier function = variantToIdentifier(args[0]);
  if (!function)
    return false;
  
  NPObject* windowObject;
  NPN_GetValue(npp, NPNVWindowNPObject, &windowObject);
  if (!windowObject)
    return false;
  
  NPVariant invokeResult;
  bool invokeReturn = NPN_Invoke(npp, windowObject, function,
      argCount > 2 ? &args[2] : NULL, argCount - 2, &invokeResult);
      
  bool compareResult = compareVariants(npp, &invokeResult, &args[1]);
      
  NPN_ReleaseObject(windowObject);
  NPN_ReleaseVariantValue(&invokeResult);
  BOOLEAN_TO_NPVARIANT(invokeReturn && compareResult, *result);
  return true;
}

static bool
npnEvaluateTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  bool success = false;
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  
  if (argCount != 1)
    return false;
  
  if (!NPVARIANT_IS_STRING(args[0]))
    return false;

  NPObject* windowObject;
  NPN_GetValue(npp, NPNVWindowNPObject, &windowObject);
  if (!windowObject)
    return false;
  
  success = NPN_Evaluate(npp, windowObject, (NPString*)&NPVARIANT_TO_STRING(args[0]), result);
  
  NPN_ReleaseObject(windowObject);
  return success;
}

static bool
setUndefinedValueTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  NPError err = NPN_SetValue(npp, (NPPVariable)0x0, 0x0);
  BOOLEAN_TO_NPVARIANT((err == NPERR_NO_ERROR), *result);
  return true;
}

static bool
identifierToStringTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
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

static bool
queryPrivateModeState(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPBool pms = false;
  NPN_GetValue(static_cast<TestNPObject*>(npobj)->npp, NPNVprivateModeBool, &pms);
  BOOLEAN_TO_NPVARIANT(pms, *result);
  return true;
}

static bool
lastReportedPrivateModeState(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  InstanceData* id = static_cast<InstanceData*>(static_cast<TestNPObject*>(npobj)->npp->pdata);
  BOOLEAN_TO_NPVARIANT(id->lastReportedPrivateModeState, *result);
  return true;
}

static bool
hasWidget(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  InstanceData* id = static_cast<InstanceData*>(static_cast<TestNPObject*>(npobj)->npp->pdata);
  BOOLEAN_TO_NPVARIANT(id->hasWidget, *result);
  return true;
}

static bool
getEdge(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 1)
    return false;
  if (!NPVARIANT_IS_INT32(args[0]))
    return false;
  int32_t edge = NPVARIANT_TO_INT32(args[0]);
  if (edge < EDGE_LEFT || edge > EDGE_BOTTOM)
    return false;

  InstanceData* id = static_cast<InstanceData*>(static_cast<TestNPObject*>(npobj)->npp->pdata);
  int32_t r = pluginGetEdge(id, RectEdge(edge));
  if (r == NPTEST_INT32_ERROR)
    return false;
  INT32_TO_NPVARIANT(r, *result);
  return true;
}

static bool
getClipRegionRectCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  InstanceData* id = static_cast<InstanceData*>(static_cast<TestNPObject*>(npobj)->npp->pdata);
  int32_t r = pluginGetClipRegionRectCount(id);
  if (r == NPTEST_INT32_ERROR)
    return false;
  INT32_TO_NPVARIANT(r, *result);
  return true;
}

static bool
getClipRegionRectEdge(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 2)
    return false;
  if (!NPVARIANT_IS_INT32(args[0]))
    return false;
  int32_t rectIndex = NPVARIANT_TO_INT32(args[0]);
  if (rectIndex < 0)
    return false;
  if (!NPVARIANT_IS_INT32(args[1]))
    return false;
  int32_t edge = NPVARIANT_TO_INT32(args[1]);
  if (edge < EDGE_LEFT || edge > EDGE_BOTTOM)
    return false;

  InstanceData* id = static_cast<InstanceData*>(static_cast<TestNPObject*>(npobj)->npp->pdata);
  int32_t r = pluginGetClipRegionRectEdge(id, rectIndex, RectEdge(edge));
  if (r == NPTEST_INT32_ERROR)
    return false;
  INT32_TO_NPVARIANT(r, *result);
  return true;
}

static bool
startWatchingInstanceCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;
  if (sWatchingInstanceCount)
    return false;

  sWatchingInstanceCount = true;
  sInstanceCount = 0;
  ++sCurrentInstanceCountWatchGeneration;
  return true;
}

static bool
getInstanceCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;
  if (!sWatchingInstanceCount)
    return false;

  INT32_TO_NPVARIANT(sInstanceCount, *result);
  return true;
}

static bool
stopWatchingInstanceCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;
  if (!sWatchingInstanceCount)
    return false;

  sWatchingInstanceCount = false;
  return true;
}

static bool
getLastMouseX(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  INT32_TO_NPVARIANT(id->lastMouseX, *result);
  return true;
}

static bool
getLastMouseY(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  INT32_TO_NPVARIANT(id->lastMouseY, *result);
  return true;
}

static bool
getPaintCount(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  INT32_TO_NPVARIANT(id->paintCount, *result);
  return true;
}

static bool
getWidthAtLastPaint(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  INT32_TO_NPVARIANT(id->widthAtLastPaint, *result);
  return true;
}

static bool
setInvalidateDuringPaint(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 1)
    return false;

  if (!NPVARIANT_IS_BOOLEAN(args[0]))
    return false;
  bool doInvalidate = NPVARIANT_TO_BOOLEAN(args[0]);

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  id->invalidateDuringPaint = doInvalidate;
  return true;
}

static bool
getError(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  if (id->err.str().length() == 0)
    STRINGZ_TO_NPVARIANT(NPN_StrDup(SUCCESS_STRING), *result);
  else
    STRINGZ_TO_NPVARIANT(NPN_StrDup(id->err.str().c_str()), *result);
  return true;
}

static bool
doInternalConsistencyCheck(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  string error;
  pluginDoInternalConsistencyCheck(id, error);
  NPUTF8* utf8String = (NPUTF8*)NPN_MemAlloc(error.length() + 1);
  if (!utf8String) {
    return false;
  }
  memcpy(utf8String, error.c_str(), error.length() + 1);
  STRINGZ_TO_NPVARIANT(utf8String, *result);
  return true;
}

static bool
convertPointX(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 4)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;

  if (!NPVARIANT_IS_INT32(args[0]))
    return false;
  int32_t sourceSpace = NPVARIANT_TO_INT32(args[0]);

  if (!NPVARIANT_IS_INT32(args[1]))
    return false;
  double sourceX = static_cast<double>(NPVARIANT_TO_INT32(args[1]));

  if (!NPVARIANT_IS_INT32(args[2]))
    return false;
  double sourceY = static_cast<double>(NPVARIANT_TO_INT32(args[2]));

  if (!NPVARIANT_IS_INT32(args[3]))
    return false;
  int32_t destSpace = NPVARIANT_TO_INT32(args[3]);

  double resultX, resultY;
  NPN_ConvertPoint(npp, sourceX, sourceY, (NPCoordinateSpace)sourceSpace, &resultX, &resultY, (NPCoordinateSpace)destSpace);

  DOUBLE_TO_NPVARIANT(resultX, *result);
  return true;
}

static bool
convertPointY(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 4)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;

  if (!NPVARIANT_IS_INT32(args[0]))
    return false;
  int32_t sourceSpace = NPVARIANT_TO_INT32(args[0]);

  if (!NPVARIANT_IS_INT32(args[1]))
    return false;
  double sourceX = static_cast<double>(NPVARIANT_TO_INT32(args[1]));

  if (!NPVARIANT_IS_INT32(args[2]))
    return false;
  double sourceY = static_cast<double>(NPVARIANT_TO_INT32(args[2]));

  if (!NPVARIANT_IS_INT32(args[3]))
    return false;
  int32_t destSpace = NPVARIANT_TO_INT32(args[3]);

  double resultX, resultY;
  NPN_ConvertPoint(npp, sourceX, sourceY, (NPCoordinateSpace)sourceSpace, &resultX, &resultY, (NPCoordinateSpace)destSpace);
  
  DOUBLE_TO_NPVARIANT(resultY, *result);
  return true;
}

static bool
streamTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  // .streamTest(url, doPost, doNull, callback)
  if (4 != argCount)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;

  if (!NPVARIANT_IS_STRING(args[0]))
    return false;
  NPString url = NPVARIANT_TO_STRING(args[0]);

  if (!NPVARIANT_IS_BOOLEAN(args[1]))
    return false;
  bool doPost = NPVARIANT_TO_BOOLEAN(args[1]);

  NPString postData = { NULL, 0 };
  if (NPVARIANT_IS_NULL(args[2])) {
  }
  else if (NPVARIANT_IS_STRING(args[2])) {
    postData = NPVARIANT_TO_STRING(args[2]);
  }
  else {
    return false;
  }

  if (!NPVARIANT_IS_OBJECT(args[3]))
    return false;
  NPObject* callback = NPVARIANT_TO_OBJECT(args[3]);

  URLNotifyData* ndata = new URLNotifyData;
  ndata->cookie = "dynamic-cookie";
  ndata->callback = callback;
  ndata->size = 0;
  ndata->data = NULL;

  /* null-terminate "url" */
  char* urlstr = (char*) malloc(url.UTF8Length + 1);
  strncpy(urlstr, url.UTF8Characters, url.UTF8Length);
  urlstr[url.UTF8Length] = '\0';

  NPError err;
  if (doPost) {
    err = NPN_PostURLNotify(npp, urlstr, NULL,
                            postData.UTF8Length, postData.UTF8Characters,
                            false, ndata);
  }
  else {
    err = NPN_GetURLNotify(npp, urlstr, NULL, ndata);
  }

  free(urlstr);

  if (NPERR_NO_ERROR == err) {
    NPN_RetainObject(ndata->callback);
    BOOLEAN_TO_NPVARIANT(true, *result);
  }
  else {
    delete ndata;
    BOOLEAN_TO_NPVARIANT(false, *result);
  }

  return true;
}

static bool
crashPlugin(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  IntentionalCrash();
  VOID_TO_NPVARIANT(*result);
  return true;
}

static bool
crashOnDestroy(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  id->crashOnDestroy = true;
  VOID_TO_NPVARIANT(*result);
  return true;
}

static bool
setColor(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 1)
    return false;
  if (!NPVARIANT_IS_STRING(args[0]))
    return false;
  const NPString* str = &NPVARIANT_TO_STRING(args[0]);

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  id->scriptableObject->drawColor =
    parseHexColor(str->UTF8Characters, str->UTF8Length);

  NPRect r;
  r.left = 0;
  r.top = 0;
  r.right = id->window.width;
  r.bottom = id->window.height;
  NPN_InvalidateRect(npp, &r);

  VOID_TO_NPVARIANT(*result);
  return true;
}

void notifyDidPaint(InstanceData* instanceData)
{
  ++instanceData->paintCount;
  instanceData->widthAtLastPaint = instanceData->window.width;

  if (instanceData->invalidateDuringPaint) {
    NPRect r;
    r.left = 0;
    r.top = 0;
    r.right = instanceData->window.width;
    r.bottom = instanceData->window.height;
    NPN_InvalidateRect(instanceData->npp, &r);
  }

  if (instanceData->runScriptOnPaint) {
    NPObject* o = NULL;
    NPN_GetValue(instanceData->npp, NPNVPluginElementNPObject, &o);
    if (o) {
      NPVariant param;
      STRINGZ_TO_NPVARIANT("paintscript", param);
      NPVariant result;
      NPN_Invoke(instanceData->npp, o, NPN_GetStringIdentifier("getAttribute"),
                 &param, 1, &result);

      if (NPVARIANT_IS_STRING(result)) {
        NPObject* windowObject;
        NPN_GetValue(instanceData->npp, NPNVWindowNPObject, &windowObject);
        if (windowObject) {
          NPVariant evalResult;
          NPN_Evaluate(instanceData->npp, windowObject,
                       (NPString*)&NPVARIANT_TO_STRING(result), &evalResult);
          NPN_ReleaseVariantValue(&evalResult);
          NPN_ReleaseObject(windowObject);
        }
      }

      NPN_ReleaseVariantValue(&result);
      NPN_ReleaseObject(o);
    }
  }
}

static const NPClass kTestSharedNPClass = {
  NP_CLASS_STRUCT_VERSION,
  // Everything else is NULL
};

static bool getObjectValue(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;

  NPObject* o = NPN_CreateObject(npp,
                                 const_cast<NPClass*>(&kTestSharedNPClass));
  if (!o)
    return false;

  OBJECT_TO_NPVARIANT(o, *result);
  return true;
}

static bool checkObjectValue(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  VOID_TO_NPVARIANT(*result);

  if (1 != argCount)
    return false;

  if (!NPVARIANT_IS_OBJECT(args[0]))
    return false;

  NPObject* o = NPVARIANT_TO_OBJECT(args[0]);

  BOOLEAN_TO_NPVARIANT(o->_class == &kTestSharedNPClass, *result);
  return true;
}

static bool enableFPExceptions(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  VOID_TO_NPVARIANT(*result);

#if defined(XP_WIN) && defined(_M_IX86)
  _control87(0, _MCW_EM);
  return true;
#else
  return false;
#endif
}

// caller is responsible for freeing return buffer
static char* URLForInstanceWindow(NPP instance) {
  char *outString = NULL;
  
  NPObject* windowObject = NULL;
  NPError err = NPN_GetValue(instance, NPNVWindowNPObject, &windowObject);
  if (err != NPERR_NO_ERROR || !windowObject)
    return NULL;
  
  NPIdentifier locationIdentifier = NPN_GetStringIdentifier("location");
  NPVariant locationVariant;
  if (NPN_GetProperty(instance, windowObject, locationIdentifier, &locationVariant)) {
    NPObject *locationObject = locationVariant.value.objectValue;
    if (locationObject) {
      NPIdentifier hrefIdentifier = NPN_GetStringIdentifier("href");
      NPVariant hrefVariant;
      if (NPN_GetProperty(instance, locationObject, hrefIdentifier, &hrefVariant)) {
        const NPString* hrefString = &NPVARIANT_TO_STRING(hrefVariant);
        if (hrefString) {
          outString = (char *)malloc(hrefString->UTF8Length + 1);
          if (outString) {
            strcpy(outString, hrefString->UTF8Characters);
            outString[hrefString->UTF8Length] = '\0';
          }
        }
        NPN_ReleaseVariantValue(&hrefVariant);
      }      
    }
    NPN_ReleaseVariantValue(&locationVariant);
  }
  
  NPN_ReleaseObject(windowObject);
  
  return outString;
}

static bool
setCookie(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 1)
    return false;
  if (!NPVARIANT_IS_STRING(args[0]))
    return false;
  const NPString* cookie = &NPVARIANT_TO_STRING(args[0]);
  
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  
  char* url = URLForInstanceWindow(npp);
  if (!url)
    return false;
  NPError err = NPN_SetValueForURL(npp, NPNURLVCookie, url, cookie->UTF8Characters, cookie->UTF8Length);
  free(url);
  
  return (err == NPERR_NO_ERROR);
}

static bool
getCookie(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 0)
    return false;
  
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  
  char* url = URLForInstanceWindow(npp);
  if (!url)
    return false;
  char* cookie = NULL;
  unsigned int length = 0;
  NPError err = NPN_GetValueForURL(npp, NPNURLVCookie, url, &cookie, &length);
  free(url);
  if (err != NPERR_NO_ERROR || !cookie)
    return false;
  
  STRINGZ_TO_NPVARIANT(cookie, *result);
  return true;
}

static bool
getAuthInfo(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (argCount != 5)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;

  if (!NPVARIANT_IS_STRING(args[0]) || !NPVARIANT_IS_STRING(args[1]) ||
      !NPVARIANT_IS_INT32(args[2]) || !NPVARIANT_IS_STRING(args[3]) ||
      !NPVARIANT_IS_STRING(args[4]))
    return false;

  const NPString* protocol = &NPVARIANT_TO_STRING(args[0]);
  const NPString* host = &NPVARIANT_TO_STRING(args[1]);
  uint32_t port = NPVARIANT_TO_INT32(args[2]);
  const NPString* scheme = &NPVARIANT_TO_STRING(args[3]);
  const NPString* realm = &NPVARIANT_TO_STRING(args[4]);

  char* username = NULL;
  char* password = NULL;
  uint32_t ulen = 0, plen = 0;
  
  NPError err = NPN_GetAuthenticationInfo(npp, 
      protocol->UTF8Characters, 
      host->UTF8Characters, 
      port, 
      scheme->UTF8Characters, 
      realm->UTF8Characters,
      &username, 
      &ulen, 
      &password, 
      &plen);
  
  if (err != NPERR_NO_ERROR) {
    return false;
  }

  char* outstring = (char*)NPN_MemAlloc(ulen + plen + 2);
  memset(outstring, 0, ulen + plen + 2);
  strncpy(outstring, username, ulen);
  strcat(outstring, "|");
  strncat(outstring, password, plen);

  STRINGZ_TO_NPVARIANT(outstring, *result);

  NPN_MemFree(username);
  NPN_MemFree(password);
  
  return true;
}

static void timerCallback(NPP npp, uint32_t timerID)
{
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  timerEventCount++;
  timerEvent event = timerEvents[timerEventCount];

  NPObject* windowObject;
  NPN_GetValue(npp, NPNVWindowNPObject, &windowObject);
  if (!windowObject)
    return;

  NPVariant rval;
  if (timerID != id->timerID[event.timerIdReceive])
    id->timerTestResult = false;

  if (timerEventCount == totalTimerEvents - 1) {
    NPVariant arg;
    BOOLEAN_TO_NPVARIANT(id->timerTestResult, arg);
    NPN_Invoke(npp, windowObject, NPN_GetStringIdentifier(id->timerTestScriptCallback.c_str()), &arg, 1, &rval);
    NPN_ReleaseVariantValue(&arg);
  }

  NPN_ReleaseObject(windowObject);
  
  if (event.timerIdSchedule > -1) {
    id->timerID[event.timerIdSchedule] = NPN_ScheduleTimer(npp, event.timerInterval, event.timerRepeat, timerCallback);
  }
  if (event.timerIdUnschedule > -1) {
    NPN_UnscheduleTimer(npp, id->timerID[event.timerIdUnschedule]);
  }
}

static bool
timerTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  timerEventCount = 0;

  if (argCount < 1 || !NPVARIANT_IS_STRING(args[0]))
    return false;
  const NPString* argstr = &NPVARIANT_TO_STRING(args[0]);
  id->timerTestScriptCallback = argstr->UTF8Characters;

  id->timerTestResult = true;
  timerEvent event = timerEvents[timerEventCount];
    
  id->timerID[event.timerIdSchedule] = NPN_ScheduleTimer(npp, event.timerInterval, event.timerRepeat, timerCallback);
  
  return id->timerID[event.timerIdSchedule] != 0;
}

#ifdef XP_WIN
void
ThreadProc(void* cookie)
#else
void*
ThreadProc(void* cookie)
#endif
{
  NPObject* npobj = (NPObject*)cookie;
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  id->asyncTestPhase = 1;
  NPN_PluginThreadAsyncCall(npp, asyncCallback, (void*)npobj);
#ifndef XP_WIN
  return NULL;
#endif
}

void
asyncCallback(void* cookie)
{
  NPObject* npobj = (NPObject*)cookie;
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  switch (id->asyncTestPhase) {
    // async callback triggered from same thread
    case 0:
#ifdef XP_WIN
      if (_beginthread(ThreadProc, 0, (void*)npobj) == -1)
        id->asyncCallbackResult = false;
#else
      pthread_t tid;
      if (pthread_create(&tid, 0, ThreadProc, (void*)npobj))
        id->asyncCallbackResult = false;
#endif
      break;
    
    // async callback triggered from different thread
    default:
      NPObject* windowObject;
      NPN_GetValue(npp, NPNVWindowNPObject, &windowObject);
      if (!windowObject)
        return;
      NPVariant arg, rval;
      BOOLEAN_TO_NPVARIANT(id->asyncCallbackResult, arg);
      NPN_Invoke(npp, windowObject, NPN_GetStringIdentifier(id->asyncTestScriptCallback.c_str()), &arg, 1, &rval);
      NPN_ReleaseVariantValue(&arg);
      NPN_ReleaseObject(windowObject);
      break;
  }
}

static bool
asyncCallbackTest(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  if (argCount < 1 || !NPVARIANT_IS_STRING(args[0]))
    return false;
  const NPString* argstr = &NPVARIANT_TO_STRING(args[0]);
  id->asyncTestScriptCallback = argstr->UTF8Characters;
  
  id->asyncTestPhase = 0;
  id->asyncCallbackResult = true;
  NPN_PluginThreadAsyncCall(npp, asyncCallback, (void*)npobj);
  
  return true;
}

static bool
GCRaceInvoke(NPObject*, NPIdentifier, const NPVariant*, uint32_t, NPVariant*)
{
  return false;
}

static bool
GCRaceInvokeDefault(NPObject* o, const NPVariant* args, uint32_t argCount,
		    NPVariant* result)
{
  if (1 != argCount || !NPVARIANT_IS_INT32(args[0]) ||
      35 != NPVARIANT_TO_INT32(args[0]))
    return false;

  return true;
}

static const NPClass kGCRaceClass = {
  NP_CLASS_STRUCT_VERSION,
  NULL,
  NULL,
  NULL,
  NULL,
  GCRaceInvoke,
  GCRaceInvokeDefault,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

struct GCRaceData
{
  GCRaceData(NPP npp, NPObject* callback, NPObject* localFunc)
    : npp_(npp)
    , callback_(callback)
    , localFunc_(localFunc)
  {
    NPN_RetainObject(callback_);
    NPN_RetainObject(localFunc_);
  }

  ~GCRaceData()
  {
    NPN_ReleaseObject(callback_);
    NPN_ReleaseObject(localFunc_);
  }

  NPP npp_;
  NPObject* callback_;
  NPObject* localFunc_;
};

static void
FinishGCRace(void* closure)
{
  GCRaceData* rd = static_cast<GCRaceData*>(closure);

#ifdef XP_WIN
  Sleep(5000);
#else
  sleep(5);
#endif

  NPVariant arg;
  OBJECT_TO_NPVARIANT(rd->localFunc_, arg);

  NPVariant result;
  bool ok = NPN_InvokeDefault(rd->npp_, rd->callback_, &arg, 1, &result);
  if (!ok)
    return;

  NPN_ReleaseVariantValue(&result);
  delete rd;
}

bool
checkGCRace(NPObject* npobj, const NPVariant* args, uint32_t argCount,
	    NPVariant* result)
{
  if (1 != argCount || !NPVARIANT_IS_OBJECT(args[0]))
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  
  NPObject* localFunc =
    NPN_CreateObject(npp, const_cast<NPClass*>(&kGCRaceClass));

  GCRaceData* rd =
    new GCRaceData(npp, NPVARIANT_TO_OBJECT(args[0]), localFunc);
  NPN_PluginThreadAsyncCall(npp, FinishGCRace, rd);

  OBJECT_TO_NPVARIANT(localFunc, *result);
  return true;
}

bool
hangPlugin(NPObject* npobj, const NPVariant* args, uint32_t argCount,
           NPVariant* result)
{
  NoteIntentionalCrash();

#ifdef XP_WIN
  Sleep(100000000);
#else
  pause();
#endif
  // NB: returning true here means that we weren't terminated, and
  // thus the hang detection/handling didn't work correctly.  The
  // test harness will succeed in calling this function, and the
  // test will fail.
  return true;
}

#if defined(MOZ_WIDGET_GTK2)
bool
getClipboardText(NPObject* npobj, const NPVariant* args, uint32_t argCount,
                 NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  string sel = pluginGetClipboardText(id);

  uint32 len = sel.size();
  char* selCopy = static_cast<char*>(NPN_MemAlloc(1 + len));
  if (!selCopy)
    return false;

  memcpy(selCopy, sel.c_str(), len);
  selCopy[len] = '\0';

  STRINGN_TO_NPVARIANT(selCopy, len, *result);
  // *result owns str now

  return true;
}

bool
crashPluginInNestedLoop(NPObject* npobj, const NPVariant* args,
                        uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);
  return pluginCrashInNestedLoop(id);
}

#else
bool
getClipboardText(NPObject* npobj, const NPVariant* args, uint32_t argCount,
                 NPVariant* result)
{
  // XXX Not implemented!
  return false;
}

bool
crashPluginInNestedLoop(NPObject* npobj, const NPVariant* args,
                        uint32_t argCount, NPVariant* result)
{
  // XXX Not implemented!
  return false;
}
#endif

bool
callOnDestroy(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  if (id->callOnDestroy)
    return false;

  if (1 != argCount || !NPVARIANT_IS_OBJECT(args[0]))
    return false;

  id->callOnDestroy = NPVARIANT_TO_OBJECT(args[0]);
  NPN_RetainObject(id->callOnDestroy);

  return true;
}

// On Linux at least, a windowed plugin resize causes Flash Player to
// reconnect to the browser window.  This method simulates that.
bool
reinitWidget(NPObject* npobj, const NPVariant* args, uint32_t argCount,
             NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  if (!id->hasWidget)
    return false;

  pluginWidgetInit(id, id->window.window);
  return true;
}

bool
propertyAndMethod(NPObject* npobj, const NPVariant* args, uint32_t argCount,
                  NPVariant* result)
{
  INT32_TO_NPVARIANT(5, *result);
  return true;
}

// Returns top-level window activation state as indicated by Cocoa NPAPI's
// NPCocoaEventWindowFocusChanged events - 'true' if active, 'false' if not.
// Throws an exception if no events have been received and thus this state
// is unknown.
bool
getTopLevelWindowActivationState(NPObject* npobj, const NPVariant* args, uint32_t argCount,
                                 NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  // Throw an exception for unknown state.
  if (id->topLevelWindowActivationState == ACTIVATION_STATE_UNKNOWN) {
    return false;
  }

  if (id->topLevelWindowActivationState == ACTIVATION_STATE_ACTIVATED) {
    BOOLEAN_TO_NPVARIANT(true, *result);
  } else if (id->topLevelWindowActivationState == ACTIVATION_STATE_DEACTIVATED) {
    BOOLEAN_TO_NPVARIANT(false, *result);
  }

  return true;
}

bool
getTopLevelWindowActivationEventCount(NPObject* npobj, const NPVariant* args, uint32_t argCount,
                                      NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  INT32_TO_NPVARIANT(id->topLevelWindowActivationEventCount, *result);

  return true;
}

// Returns top-level window activation state as indicated by Cocoa NPAPI's
// NPCocoaEventWindowFocusChanged events - 'true' if active, 'false' if not.
// Throws an exception if no events have been received and thus this state
// is unknown.
bool
getFocusState(NPObject* npobj, const NPVariant* args, uint32_t argCount,
              NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  // Throw an exception for unknown state.
  if (id->focusState == ACTIVATION_STATE_UNKNOWN) {
    return false;
  }

  if (id->focusState == ACTIVATION_STATE_ACTIVATED) {
    BOOLEAN_TO_NPVARIANT(true, *result);
  } else if (id->focusState == ACTIVATION_STATE_DEACTIVATED) {
    BOOLEAN_TO_NPVARIANT(false, *result);
  }

  return true;
}

bool
getFocusEventCount(NPObject* npobj, const NPVariant* args, uint32_t argCount,
                   NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  INT32_TO_NPVARIANT(id->focusEventCount, *result);

  return true;
}

bool
getEventModel(NPObject* npobj, const NPVariant* args, uint32_t argCount,
              NPVariant* result)
{
  if (argCount != 0)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;
  InstanceData* id = static_cast<InstanceData*>(npp->pdata);

  INT32_TO_NPVARIANT(id->eventModel, *result);

  return true;
}

static bool
ReflectorHasMethod(NPObject* npobj, NPIdentifier name)
{
  return false;
}

static bool
ReflectorHasProperty(NPObject* npobj, NPIdentifier name)
{
  return true;
}

static bool
ReflectorGetProperty(NPObject* npobj, NPIdentifier name, NPVariant* result)
{
  if (NPN_IdentifierIsString(name)) {
    char* s = NPN_UTF8FromIdentifier(name);
    STRINGZ_TO_NPVARIANT(s, *result);
    return true;
  }

  INT32_TO_NPVARIANT(NPN_IntFromIdentifier(name), *result);
  return true;
}

static const NPClass kReflectorNPClass = {
  NP_CLASS_STRUCT_VERSION,
  NULL,
  NULL,
  NULL,
  ReflectorHasMethod,
  NULL,
  NULL,
  ReflectorHasProperty,
  ReflectorGetProperty,
  NULL,
  NULL,
  NULL,
  NULL
};

bool
getReflector(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if (0 != argCount)
    return false;

  NPP npp = static_cast<TestNPObject*>(npobj)->npp;

  NPObject* reflector =
    NPN_CreateObject(npp,
		     const_cast<NPClass*>(&kReflectorNPClass)); // retains
  OBJECT_TO_NPVARIANT(reflector, *result);
  return true;
}
