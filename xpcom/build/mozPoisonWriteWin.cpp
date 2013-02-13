/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <windows.h>
#include <winternl.h> // NTSTATUS
#include "nsWindowsDllInterceptor.h"
#include "mozilla/mozPoisonWrite.h"
#include "mozPoisonWriteBase.h"
#include "mozilla/Assertions.h"

namespace mozilla {

static WindowsDllInterceptor sNtDllInterceptor;

void AbortOnBadWrite();
bool ValidWriteAssert(bool ok);

typedef NTSTATUS (WINAPI* FlushBuffersFile_fn)(HANDLE, PIO_STATUS_BLOCK);
FlushBuffersFile_fn gOriginalFlushBuffersFile;

NTSTATUS WINAPI
patched_FlushBuffersFile(HANDLE aFileHandle, PIO_STATUS_BLOCK aIoStatusBlock)
{
  AbortOnBadWrite();
  return gOriginalFlushBuffersFile(aFileHandle, aIoStatusBlock);
}

void AbortOnBadWrite()
{
  if (!PoisonWriteEnabled())
    return;

  ValidWriteAssert(false);
}

void PoisonWrite() {
  // Quick sanity check that we don't poison twice.
  static bool WritesArePoisoned = false;
  MOZ_ASSERT(!WritesArePoisoned);
  if (WritesArePoisoned)
    return;
  WritesArePoisoned = true;

  if (!PoisonWriteEnabled())
    return;

  PoisonWriteBase();

  sNtDllInterceptor.Init("ntdll.dll");
  sNtDllInterceptor.AddHook("NtFlushBuffersFile", reinterpret_cast<intptr_t>(patched_FlushBuffersFile), reinterpret_cast<void**>(&gOriginalFlushBuffersFile));
}
}

