/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXPCOM.h"
#include "nsMemory.h"
#include "nsIMemory.h"
#include "nsXPCOMPrivate.h"
#include "nsDebug.h"
#include "nsISupportsUtils.h"
#include "nsCOMPtr.h"

////////////////////////////////////////////////////////////////////////////////
// nsMemory static helper routines

NS_COM_GLUE nsresult
nsMemory::HeapMinimize(bool aImmediate)
{
  nsCOMPtr<nsIMemory> mem;
  nsresult rv = NS_GetMemoryManager(getter_AddRefs(mem));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return mem->HeapMinimize(aImmediate);
}

NS_COM_GLUE void*
nsMemory::Clone(const void* aPtr, size_t aSize)
{
  void* newPtr = NS_Alloc(aSize);
  if (newPtr) {
    memcpy(newPtr, aPtr, aSize);
  }
  return newPtr;
}

NS_COM_GLUE nsIMemory*
nsMemory::GetGlobalMemoryService()
{
  nsIMemory* mem;
  nsresult rv = NS_GetMemoryManager(&mem);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return mem;
}

//----------------------------------------------------------------------

