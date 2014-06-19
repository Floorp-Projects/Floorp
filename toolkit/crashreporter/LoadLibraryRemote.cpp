/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __GNUC__
// disable warnings about pointer <-> DWORD conversions
#pragma warning( disable : 4311 4312 )
#endif

#ifdef _WIN64
#define POINTER_TYPE ULONGLONG
#else
#define POINTER_TYPE DWORD
#endif

#include <windows.h>
#include <winnt.h>
#include <stdlib.h>
#ifdef DEBUG_OUTPUT
#include <stdio.h>
#endif

#include "nsWindowsHelpers.h"

typedef const unsigned char* FileView;

template<>
class nsAutoRefTraits<FileView>
{
public:
  typedef FileView RawRef;
  static FileView Void()
  {
    return nullptr;
  }

  static void Release(RawRef aView)
  {
    if (nullptr != aView)
      UnmapViewOfFile(aView);
  }
};

#ifndef IMAGE_SIZEOF_BASE_RELOCATION
// Vista SDKs no longer define IMAGE_SIZEOF_BASE_RELOCATION!?
#define IMAGE_SIZEOF_BASE_RELOCATION (sizeof(IMAGE_BASE_RELOCATION))
#endif

#include "LoadLibraryRemote.h"

typedef struct {
  PIMAGE_NT_HEADERS headers;
  unsigned char *localCodeBase;
  unsigned char *remoteCodeBase;
  HMODULE *modules;
  int numModules;
} MEMORYMODULE, *PMEMORYMODULE;

typedef BOOL (WINAPI *DllEntryProc)(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);

#define GET_HEADER_DICTIONARY(module, idx)  &(module)->headers->OptionalHeader.DataDirectory[idx]

#ifdef DEBUG_OUTPUT
static void
OutputLastError(const char *msg)
{
  char* tmp;
  char *tmpmsg;
  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 nullptr, GetLastError(),
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 (LPSTR) &tmp, 0, nullptr);
  tmpmsg = (char *)LocalAlloc(LPTR, strlen(msg) + strlen(tmp) + 3);
  sprintf(tmpmsg, "%s: %s", msg, tmp);
  OutputDebugStringA(tmpmsg);
  LocalFree(tmpmsg);
  LocalFree(tmp);
}
#endif

static void
CopySections(const unsigned char *data, PIMAGE_NT_HEADERS old_headers, PMEMORYMODULE module)
{
  int i;
  unsigned char *codeBase = module->localCodeBase;
  unsigned char *dest;
  PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(module->headers);
  for (i=0; i<module->headers->FileHeader.NumberOfSections; i++, section++) {
    dest = codeBase + section->VirtualAddress;
    memset(dest, 0, section->Misc.VirtualSize);
    if (section->SizeOfRawData) {
      memcpy(dest, data + section->PointerToRawData, section->SizeOfRawData);
    }
    //     section->Misc.PhysicalAddress = (POINTER_TYPE) module->remoteCodeBase + section->VirtualAddress;
  }
}

// Protection flags for memory pages (Executable, Readable, Writeable)
static int ProtectionFlags[2][2][2] = {
  {
    // not executable
    {PAGE_NOACCESS, PAGE_WRITECOPY},
    {PAGE_READONLY, PAGE_READWRITE},
  }, {
    // executable
    {PAGE_EXECUTE, PAGE_EXECUTE_WRITECOPY},
    {PAGE_EXECUTE_READ, PAGE_EXECUTE_READWRITE},
  },
};

static bool
FinalizeSections(PMEMORYMODULE module, HANDLE hRemoteProcess)
{
#ifdef DEBUG_OUTPUT
  fprintf(stderr, "Finalizing sections: local base %p, remote base %p\n",
          module->localCodeBase, module->remoteCodeBase);
#endif

  int i;
  PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(module->headers);
  
  // loop through all sections and change access flags
  for (i=0; i<module->headers->FileHeader.NumberOfSections; i++, section++) {
    DWORD protect, oldProtect, size;
    int executable = (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
    int readable =   (section->Characteristics & IMAGE_SCN_MEM_READ) != 0;
    int writeable =  (section->Characteristics & IMAGE_SCN_MEM_WRITE) != 0;

    // determine protection flags based on characteristics
    protect = ProtectionFlags[executable][readable][writeable];
    if (section->Characteristics & IMAGE_SCN_MEM_NOT_CACHED) {
      protect |= PAGE_NOCACHE;
    }

    // determine size of region
    size = section->Misc.VirtualSize;
    if (size > 0) {
      void* remoteAddress = module->remoteCodeBase + section->VirtualAddress;
      void* localAddress = module->localCodeBase + section->VirtualAddress;

#ifdef DEBUG_OUTPUT
      fprintf(stderr, "Copying section %s to %p, size %x, executable %i readable %i writeable %i\n",
              section->Name, remoteAddress, size, executable, readable, writeable);
#endif

      // Copy the data from local->remote and set the memory protection
      if (!VirtualAllocEx(hRemoteProcess, remoteAddress, size, MEM_COMMIT, PAGE_READWRITE))
        return false;

      if (!WriteProcessMemory(hRemoteProcess,
                              remoteAddress,
                              localAddress,
                              size,
                              nullptr)) {
#ifdef DEBUG_OUTPUT
        OutputLastError("Error writing remote memory.\n");
#endif
        return false;
      }

      if (VirtualProtectEx(hRemoteProcess, remoteAddress, size, protect, &oldProtect) == 0) {
#ifdef DEBUG_OUTPUT
        OutputLastError("Error protecting memory page");
#endif
        return false;
      }
    }
  }
  return true;
}

static void
PerformBaseRelocation(PMEMORYMODULE module, SIZE_T delta)
{
  DWORD i;
  unsigned char *codeBase = module->localCodeBase;

  PIMAGE_DATA_DIRECTORY directory = GET_HEADER_DICTIONARY(module, IMAGE_DIRECTORY_ENTRY_BASERELOC);
  if (directory->Size > 0) {
    PIMAGE_BASE_RELOCATION relocation = (PIMAGE_BASE_RELOCATION) (codeBase + directory->VirtualAddress);
    for (; relocation->VirtualAddress > 0; ) {
      unsigned char *dest = codeBase + relocation->VirtualAddress;
      unsigned short *relInfo = (unsigned short *)((unsigned char *)relocation + IMAGE_SIZEOF_BASE_RELOCATION);
      for (i=0; i<((relocation->SizeOfBlock-IMAGE_SIZEOF_BASE_RELOCATION) / 2); i++, relInfo++) {
        DWORD *patchAddrHL;
#ifdef _WIN64
        ULONGLONG *patchAddr64;
#endif
        int type, offset;

        // the upper 4 bits define the type of relocation
        type = *relInfo >> 12;
        // the lower 12 bits define the offset
        offset = *relInfo & 0xfff;
        
        switch (type)
        {
        case IMAGE_REL_BASED_ABSOLUTE:
          // skip relocation
          break;

        case IMAGE_REL_BASED_HIGHLOW:
          // change complete 32 bit address
          patchAddrHL = (DWORD *) (dest + offset);
          *patchAddrHL += delta;
          break;
        
#ifdef _WIN64
        case IMAGE_REL_BASED_DIR64:
          patchAddr64 = (ULONGLONG *) (dest + offset);
          *patchAddr64 += delta;
          break;
#endif

        default:
          //printf("Unknown relocation: %d\n", type);
          break;
        }
      }

      // advance to next relocation block
      relocation = (PIMAGE_BASE_RELOCATION) (((char *) relocation) + relocation->SizeOfBlock);
    }
  }
}

static int
BuildImportTable(PMEMORYMODULE module)
{
  int result=1;
  unsigned char *codeBase = module->localCodeBase;

  PIMAGE_DATA_DIRECTORY directory = GET_HEADER_DICTIONARY(module, IMAGE_DIRECTORY_ENTRY_IMPORT);
  if (directory->Size > 0) {
    PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR) (codeBase + directory->VirtualAddress);
    PIMAGE_IMPORT_DESCRIPTOR importEnd = (PIMAGE_IMPORT_DESCRIPTOR) (codeBase + directory->VirtualAddress + directory->Size);

    for (; importDesc < importEnd && importDesc->Name; importDesc++) {
      POINTER_TYPE *thunkRef;
      FARPROC *funcRef;
      HMODULE handle = GetModuleHandleA((LPCSTR) (codeBase + importDesc->Name));
      if (handle == nullptr) {
#if DEBUG_OUTPUT
        OutputLastError("Can't load library");
#endif
        result = 0;
        break;
      }

      module->modules = (HMODULE *)realloc(module->modules, (module->numModules+1)*(sizeof(HMODULE)));
      if (module->modules == nullptr) {
        result = 0;
        break;
      }

      module->modules[module->numModules++] = handle;
      if (importDesc->OriginalFirstThunk) {
        thunkRef = (POINTER_TYPE *) (codeBase + importDesc->OriginalFirstThunk);
        funcRef = (FARPROC *) (codeBase + importDesc->FirstThunk);
      } else {
        // no hint table
        thunkRef = (POINTER_TYPE *) (codeBase + importDesc->FirstThunk);
        funcRef = (FARPROC *) (codeBase + importDesc->FirstThunk);
      }
      for (; *thunkRef; thunkRef++, funcRef++) {
        if (IMAGE_SNAP_BY_ORDINAL(*thunkRef)) {
          *funcRef = (FARPROC)GetProcAddress(handle, (LPCSTR)IMAGE_ORDINAL(*thunkRef));
        } else {
          PIMAGE_IMPORT_BY_NAME thunkData = (PIMAGE_IMPORT_BY_NAME) (codeBase + (*thunkRef));
          *funcRef = (FARPROC)GetProcAddress(handle, (LPCSTR)&thunkData->Name);
        }
        if (*funcRef == 0) {
          result = 0;
          break;
        }
      }

      if (!result) {
        break;
      }
    }
  }

  return result;
}

static void* MemoryGetProcAddress(PMEMORYMODULE module, const char *name);

void* LoadRemoteLibraryAndGetAddress(HANDLE hRemoteProcess,
                                     const WCHAR* library,
                                     const char* symbol)
{
  // Map the DLL into memory
  nsAutoHandle hLibrary(
    CreateFile(library, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
               FILE_ATTRIBUTE_NORMAL, nullptr));
  if (INVALID_HANDLE_VALUE == hLibrary) {
#if DEBUG_OUTPUT
    OutputLastError("Couldn't CreateFile the library.\n");
#endif
    return nullptr;
  }

  nsAutoHandle hMapping(
    CreateFileMapping(hLibrary, nullptr, PAGE_READONLY, 0, 0, nullptr));
  if (!hMapping) {
#if DEBUG_OUTPUT
    OutputLastError("Couldn't CreateFileMapping.\n");
#endif
    return nullptr;
  }

  nsAutoRef<FileView> data(
    (const unsigned char*) MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0));
  if (!data) {
#if DEBUG_OUTPUT
    OutputLastError("Couldn't MapViewOfFile.\n");
#endif
    return nullptr;
  }

  SIZE_T locationDelta;

  PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)data.get();
  if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
#if DEBUG_OUTPUT
    OutputDebugStringA("Not a valid executable file.\n");
#endif
    return nullptr;
  }

  PIMAGE_NT_HEADERS old_header = (PIMAGE_NT_HEADERS)(data + dos_header->e_lfanew);
  if (old_header->Signature != IMAGE_NT_SIGNATURE) {
#if DEBUG_OUTPUT
    OutputDebugStringA("No PE header found.\n");
#endif
    return nullptr;
  }

  // reserve memory for image of library in this process and the target process
  unsigned char* localCode = (unsigned char*) VirtualAlloc(nullptr,
    old_header->OptionalHeader.SizeOfImage,
    MEM_RESERVE | MEM_COMMIT,
    PAGE_READWRITE);
  if (!localCode) {
#if DEBUG_OUTPUT
    OutputLastError("Can't reserve local memory.");
#endif
  }

  unsigned char* remoteCode = (unsigned char*) VirtualAllocEx(hRemoteProcess, nullptr,
    old_header->OptionalHeader.SizeOfImage,
    MEM_RESERVE,
    PAGE_EXECUTE_READ);
  if (!remoteCode) {
#if DEBUG_OUTPUT
    OutputLastError("Can't reserve remote memory.");
#endif
  }

  MEMORYMODULE result;
  result.localCodeBase = localCode;
  result.remoteCodeBase = remoteCode;
  result.numModules = 0;
  result.modules = nullptr;

  // copy PE header to code
  memcpy(localCode, dos_header, dos_header->e_lfanew + old_header->OptionalHeader.SizeOfHeaders);
  result.headers = reinterpret_cast<PIMAGE_NT_HEADERS>(localCode + dos_header->e_lfanew);

  // update position
  result.headers->OptionalHeader.ImageBase = (POINTER_TYPE)remoteCode;

  // copy sections from DLL file block to new memory location
  CopySections(data, old_header, &result);

  // adjust base address of imported data
  locationDelta = (SIZE_T)(remoteCode - old_header->OptionalHeader.ImageBase);
  if (locationDelta != 0) {
    PerformBaseRelocation(&result, locationDelta);
  }

  // load required dlls and adjust function table of imports
  if (!BuildImportTable(&result)) {
    return nullptr;
  }

  // mark memory pages depending on section headers and release
  // sections that are marked as "discardable"
  if (!FinalizeSections(&result, hRemoteProcess)) {
    return nullptr;
  }

  return MemoryGetProcAddress(&result, symbol);
}

static void* MemoryGetProcAddress(PMEMORYMODULE module, const char *name)
{
  unsigned char *localCodeBase = module->localCodeBase;
  int idx=-1;
  DWORD i, *nameRef;
  WORD *ordinal;
  PIMAGE_EXPORT_DIRECTORY exports;
  PIMAGE_DATA_DIRECTORY directory = GET_HEADER_DICTIONARY(module, IMAGE_DIRECTORY_ENTRY_EXPORT);
  if (directory->Size == 0) {
    // no export table found
    return nullptr;
  }

  exports = (PIMAGE_EXPORT_DIRECTORY) (localCodeBase + directory->VirtualAddress);
  if (exports->NumberOfNames == 0 || exports->NumberOfFunctions == 0) {
    // DLL doesn't export anything
    return nullptr;
  }

  // search function name in list of exported names
  nameRef = (DWORD *) (localCodeBase + exports->AddressOfNames);
  ordinal = (WORD *) (localCodeBase + exports->AddressOfNameOrdinals);
  for (i=0; i<exports->NumberOfNames; i++, nameRef++, ordinal++) {
    if (stricmp(name, (const char *) (localCodeBase + (*nameRef))) == 0) {
      idx = *ordinal;
      break;
    }
  }

  if (idx == -1) {
    // exported symbol not found
    return nullptr;
  }

  if ((DWORD)idx > exports->NumberOfFunctions) {
    // name <-> ordinal number don't match
    return nullptr;
  }

  // AddressOfFunctions contains the RVAs to the "real" functions
  return module->remoteCodeBase + (*(DWORD *) (localCodeBase + exports->AddressOfFunctions + (idx*4)));
}
