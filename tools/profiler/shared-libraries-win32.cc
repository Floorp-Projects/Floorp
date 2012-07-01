/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <tlhelp32.h>
#include <dbghelp.h>

#include "shared-libraries.h"
#include "nsWindowsHelpers.h"

#define CV_SIGNATURE 0x53445352 // 'SDSR'

struct CodeViewRecord70
{
  uint32_t signature;
  GUID pdbSignature;
  uint32_t pdbAge;
  char pdbFileName[1];
};

static bool GetPdbInfo(uintptr_t aStart, nsID& aSignature, uint32_t& aAge, char** aPdbName)
{
  if (!aStart) {
    return false;
  }

  PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(aStart);
  if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
    return false;
  }

  PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(
      aStart + dosHeader->e_lfanew);
  if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
    return false;
  }

  uint32_t relativeVirtualAddress =
    ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;
  if (!relativeVirtualAddress) {
    return false;
  }

  PIMAGE_DEBUG_DIRECTORY debugDirectory =
    reinterpret_cast<PIMAGE_DEBUG_DIRECTORY>(aStart + relativeVirtualAddress);
  if (!debugDirectory || debugDirectory->Type != IMAGE_DEBUG_TYPE_CODEVIEW) {
    return false;
  }

  CodeViewRecord70 *debugInfo = reinterpret_cast<CodeViewRecord70 *>(
      aStart + debugDirectory->AddressOfRawData);
  if (!debugInfo || debugInfo->signature != CV_SIGNATURE) {
    return false;
  }

  aAge = debugInfo->pdbAge;
  GUID& pdbSignature = debugInfo->pdbSignature;
  aSignature.m0 = pdbSignature.Data1;
  aSignature.m1 = pdbSignature.Data2;
  aSignature.m2 = pdbSignature.Data3;
  memcpy(aSignature.m3, pdbSignature.Data4, sizeof(pdbSignature.Data4));

  // The PDB file name could be different from module filename, so report both
  // e.g. The PDB for C:\Windows\SysWOW64\ntdll.dll is wntdll.pdb
  char * leafName = strrchr(debugInfo->pdbFileName, '\\');
  if (leafName) {
    // Only report the file portion of the path
    *aPdbName = leafName + 1;
  } else {
    *aPdbName = debugInfo->pdbFileName;
  }

  return true;
}

SharedLibraryInfo SharedLibraryInfo::GetInfoForSelf()
{
  SharedLibraryInfo sharedLibraryInfo;

  nsAutoHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId()));

  MODULEENTRY32 module = {0};
  module.dwSize = sizeof(MODULEENTRY32);
  if (Module32First(snap, &module)) {
    do {
      nsID pdbSig;
      uint32_t pdbAge;
      char *pdbName = NULL;

      // Load the module again to make sure that its handle will remain remain
      // valid as we attempt to read the PDB information from it.  We load the
      // DLL as a datafile so that if the module actually gets unloaded between
      // the call to Module32Next and the following LoadLibraryEx, we don't end
      // up running the now newly loaded module's DllMain function.  If the
      // module is already loaded, LoadLibraryEx just increments its refcount.
      //
      // Note that because of the race condition above, merely loading the DLL
      // again is not safe enough, therefore we also need to make sure that we
      // can read the memory mapped at the base address before we can safely
      // proceed to actually access those pages.
      HMODULE handleLock = LoadLibraryEx(module.szExePath, NULL, LOAD_LIBRARY_AS_DATAFILE);
      MEMORY_BASIC_INFORMATION vmemInfo = {0};
      if (handleLock &&
          sizeof(vmemInfo) == VirtualQuery(module.modBaseAddr, &vmemInfo, sizeof(vmemInfo)) &&
          vmemInfo.State == MEM_COMMIT &&
          GetPdbInfo((uintptr_t)module.modBaseAddr, pdbSig, pdbAge, &pdbName)) {
        SharedLibrary shlib((uintptr_t)module.modBaseAddr,
                            (uintptr_t)module.modBaseAddr+module.modBaseSize,
                            0, // DLLs are always mapped at offset 0 on Windows
                            pdbSig,
                            pdbAge,
                            pdbName,
                            module.szModule);
        sharedLibraryInfo.AddSharedLibrary(shlib);
      }
      FreeLibrary(handleLock); // ok to free null handles
    } while (Module32Next(snap, &module));
  }

  return sharedLibraryInfo;
}

