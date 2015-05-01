/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXPCOM.h"
#include "nsXPCOMPrivate.h"
#include "nsXPCOMStrings.h"
#include "xptcall.h"

#include <string.h>

/**
 * Private Method to register an exit routine.  This method
 * used to allow you to setup a callback that will be called from
 * the NS_ShutdownXPCOM function after all services and
 * components have gone away. It was fatally flawed in that the component
 * DLL could be released before the exit function was called; it is now a
 * stub implementation that does nothing.
 */
XPCOM_API(nsresult)
NS_RegisterXPCOMExitRoutine(XPCOMExitRoutine aExitRoutine, uint32_t aPriority);

XPCOM_API(nsresult)
NS_UnregisterXPCOMExitRoutine(XPCOMExitRoutine aExitRoutine);

static const XPCOMFunctions kFrozenFunctions = {
  XPCOM_GLUE_VERSION,
  sizeof(XPCOMFunctions),
  &NS_InitXPCOM2,
  &NS_ShutdownXPCOM,
  &NS_GetServiceManager,
  &NS_GetComponentManager,
  &NS_GetComponentRegistrar,
  &NS_GetMemoryManager,
  &NS_NewLocalFile,
  &NS_NewNativeLocalFile,
  &NS_RegisterXPCOMExitRoutine,
  &NS_UnregisterXPCOMExitRoutine,

  // these functions were added post 1.4
  &NS_GetDebug,
  nullptr,

  // these functions were added post 1.6
  &NS_StringContainerInit,
  &NS_StringContainerFinish,
  &NS_StringGetData,
  &NS_StringSetData,
  &NS_StringSetDataRange,
  &NS_StringCopy,
  &NS_CStringContainerInit,
  &NS_CStringContainerFinish,
  &NS_CStringGetData,
  &NS_CStringSetData,
  &NS_CStringSetDataRange,
  &NS_CStringCopy,
  &NS_CStringToUTF16,
  &NS_UTF16ToCString,
  &NS_StringCloneData,
  &NS_CStringCloneData,

  // these functions were added post 1.7 (post Firefox 1.0)
  &moz_xmalloc,
  &moz_xrealloc,
  &free,
  &NS_StringContainerInit2,
  &NS_CStringContainerInit2,
  &NS_StringGetMutableData,
  &NS_CStringGetMutableData,
  nullptr,

  // these functions were added post 1.8
  &NS_DebugBreak,
  &NS_LogInit,
  &NS_LogTerm,
  &NS_LogAddRef,
  &NS_LogRelease,
  &NS_LogCtor,
  &NS_LogDtor,
  &NS_LogCOMPtrAddRef,
  &NS_LogCOMPtrRelease,
  &NS_GetXPTCallStub,
  &NS_DestroyXPTCallStub,
  &NS_InvokeByIndex,
  nullptr,
  nullptr,
  &NS_StringSetIsVoid,
  &NS_StringGetIsVoid,
  &NS_CStringSetIsVoid,
  &NS_CStringGetIsVoid,

  // these functions were added post 1.9, but then made obsolete
  nullptr,
  nullptr,

  &NS_CycleCollectorSuspect3,
};

EXPORT_XPCOM_API(nsresult)
NS_GetFrozenFunctions(XPCOMFunctions* aFunctions, const char* /* aLibraryPath */)
{
  if (!aFunctions) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (aFunctions->version != XPCOM_GLUE_VERSION) {
    return NS_ERROR_FAILURE;
  }

  uint32_t size = aFunctions->size;
  if (size > sizeof(XPCOMFunctions)) {
    size = sizeof(XPCOMFunctions);
  }

  size -= offsetof(XPCOMFunctions, init);

  memcpy(&aFunctions->init, &kFrozenFunctions.init, size);

  return NS_OK;
}

/*
 * Stubs for nsXPCOMPrivate.h
 */

EXPORT_XPCOM_API(nsresult)
NS_RegisterXPCOMExitRoutine(XPCOMExitRoutine aExitRoutine, uint32_t aPriority)
{
  return NS_OK;
}

EXPORT_XPCOM_API(nsresult)
NS_UnregisterXPCOMExitRoutine(XPCOMExitRoutine aExitRoutine)
{
  return NS_OK;
}
