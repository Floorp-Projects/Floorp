/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_MEMORY
#define MOZ_MEMORY_IMPL
#include "mozmemory_wrap.h"
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC
// See mozmemory_wrap.h for more details. This file is part of libmozglue, so
// it needs to use _impl suffixes.
#define MALLOC_DECL(name, return_type, ...) \
  MOZ_MEMORY_API return_type name ## _impl(__VA_ARGS__);
#include "malloc_decls.h"
#endif

#include <windows.h>
#include <winternl.h>
#include <io.h>

#pragma warning( push )
#pragma warning( disable : 4275 4530 ) // See msvc-stl-wrapper.template.h
#include <map>
#pragma warning( pop )

#include "Authenticode.h"
#include "nsAutoPtr.h"
#include "nsWindowsDllInterceptor.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StackWalk_windows.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "mozilla/WindowsVersion.h"
#include "nsWindowsHelpers.h"
#include "WindowsDllBlocklist.h"
#include "mozilla/AutoProfilerLabel.h"
#include "mozilla/glue/WindowsDllServices.h"

using namespace mozilla;

#define DLL_BLOCKLIST_ENTRY(name, ...) \
  { name, __VA_ARGS__ },
#define DLL_BLOCKLIST_STRING_TYPE const char*
#include "mozilla/WindowsDllBlocklistDefs.h"

// define this for very verbose dll load debug spew
#undef DEBUG_very_verbose

static const char kBlockedDllsParameter[] = "BlockedDllList=";
static const int kBlockedDllsParameterLen =
  sizeof(kBlockedDllsParameter) - 1;

static const char kBlocklistInitFailedParameter[] = "BlocklistInitFailed=1\n";
static const int kBlocklistInitFailedParameterLen =
  sizeof(kBlocklistInitFailedParameter) - 1;

static const char kUser32BeforeBlocklistParameter[] = "User32BeforeBlocklist=1\n";
static const int kUser32BeforeBlocklistParameterLen =
  sizeof(kUser32BeforeBlocklistParameter) - 1;

static uint32_t sInitFlags;
static bool sBlocklistInitAttempted;
static bool sBlocklistInitFailed;
static bool sUser32BeforeBlocklist;

// Duplicated from xpcom glue. Ideally this should be shared.
void
printf_stderr(const char *fmt, ...)
{
  if (IsDebuggerPresent()) {
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    VsprintfLiteral(buf, fmt, args);
    va_end(args);
    OutputDebugStringA(buf);
  }

  FILE *fp = _fdopen(_dup(2), "a");
  if (!fp)
      return;

  va_list args;
  va_start(args, fmt);
  vfprintf(fp, fmt, args);
  va_end(args);

  fclose(fp);
}


typedef MOZ_NORETURN_PTR void (__fastcall* BaseThreadInitThunk_func)(BOOL aIsInitialThread, void* aStartAddress, void* aThreadParam);
static WindowsDllInterceptor::FuncHookType<BaseThreadInitThunk_func> stub_BaseThreadInitThunk;

typedef NTSTATUS (NTAPI *LdrLoadDll_func) (PWCHAR filePath, PULONG flags, PUNICODE_STRING moduleFileName, PHANDLE handle);
static WindowsDllInterceptor::FuncHookType<LdrLoadDll_func> stub_LdrLoadDll;

#ifdef _M_AMD64
typedef decltype(RtlInstallFunctionTableCallback)* RtlInstallFunctionTableCallback_func;
static WindowsDllInterceptor::FuncHookType<RtlInstallFunctionTableCallback_func> stub_RtlInstallFunctionTableCallback;

extern uint8_t* sMsMpegJitCodeRegionStart;
extern size_t sMsMpegJitCodeRegionSize;

BOOLEAN WINAPI patched_RtlInstallFunctionTableCallback(DWORD64 TableIdentifier,
  DWORD64 BaseAddress, DWORD Length, PGET_RUNTIME_FUNCTION_CALLBACK Callback,
  PVOID Context, PCWSTR OutOfProcessCallbackDll)
{
  // msmpeg2vdec.dll sets up a function table callback for their JIT code that
  // just terminates the process, because their JIT doesn't have unwind info.
  // If we see this callback being registered, record the region address, so
  // that StackWalk.cpp can avoid unwinding addresses in this region.
  //
  // To keep things simple I'm not tracking unloads of msmpeg2vdec.dll.
  // Worst case the stack walker will needlessly avoid a few pages of memory.

  // Tricky: GetModuleHandleExW adds a ref by default; GetModuleHandleW doesn't.
  HMODULE callbackModule = nullptr;
  DWORD moduleFlags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                      GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;

  // These GetModuleHandle calls enter a critical section on Win7.
  AutoSuppressStackWalking suppress;

  if (GetModuleHandleExW(moduleFlags, (LPWSTR)Callback, &callbackModule) &&
      GetModuleHandleW(L"msmpeg2vdec.dll") == callbackModule) {
      sMsMpegJitCodeRegionStart = (uint8_t*)BaseAddress;
      sMsMpegJitCodeRegionSize = Length;
  }

  return stub_RtlInstallFunctionTableCallback(TableIdentifier, BaseAddress,
                                              Length, Callback, Context,
                                              OutOfProcessCallbackDll);
}
#endif

template <class T>
struct RVAMap {
  RVAMap(HANDLE map, DWORD offset) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);

    DWORD alignedOffset = (offset / info.dwAllocationGranularity) *
                          info.dwAllocationGranularity;

    MOZ_ASSERT(offset - alignedOffset < info.dwAllocationGranularity, "Wtf");

    mRealView = ::MapViewOfFile(map, FILE_MAP_READ, 0, alignedOffset,
                                sizeof(T) + (offset - alignedOffset));

    mMappedView = mRealView ? reinterpret_cast<T*>((char*)mRealView + (offset - alignedOffset)) :
                              nullptr;
  }
  ~RVAMap() {
    if (mRealView) {
      ::UnmapViewOfFile(mRealView);
    }
  }
  operator const T*() const { return mMappedView; }
  const T* operator->() const { return mMappedView; }
private:
  const T* mMappedView;
  void* mRealView;
};

static DWORD
GetTimestamp(const wchar_t* path)
{
  DWORD timestamp = 0;

  HANDLE file = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
  if (file != INVALID_HANDLE_VALUE) {
    HANDLE map = ::CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0,
                                      nullptr);
    if (map) {
      RVAMap<IMAGE_DOS_HEADER> peHeader(map, 0);
      if (peHeader) {
        RVAMap<IMAGE_NT_HEADERS> ntHeader(map, peHeader->e_lfanew);
        if (ntHeader) {
          timestamp = ntHeader->FileHeader.TimeDateStamp;
        }
      }
      ::CloseHandle(map);
    }
    ::CloseHandle(file);
  }

  return timestamp;
}

// This lock protects both the reentrancy sentinel and the crash reporter
// data structures.
static CRITICAL_SECTION sLock;

/**
 * Some versions of Windows call LoadLibraryEx to get the version information
 * for a DLL, which causes our patched LdrLoadDll implementation to re-enter
 * itself and cause infinite recursion and a stack-exhaustion crash. We protect
 * against reentrancy by allowing recursive loads of the same DLL.
 *
 * Note that we don't use __declspec(thread) because that doesn't work in DLLs
 * loaded via LoadLibrary and there can be a limited number of TLS slots, so
 * we roll our own.
 */
class ReentrancySentinel
{
public:
  explicit ReentrancySentinel(const char* dllName)
  {
    DWORD currentThreadId = GetCurrentThreadId();
    AutoCriticalSection lock(&sLock);
    mPreviousDllName = (*sThreadMap)[currentThreadId];

    // If there is a DLL currently being loaded and it has the same name
    // as the current attempt, we're re-entering.
    mReentered = mPreviousDllName && !stricmp(mPreviousDllName, dllName);
    (*sThreadMap)[currentThreadId] = dllName;
  }

  ~ReentrancySentinel()
  {
    DWORD currentThreadId = GetCurrentThreadId();
    AutoCriticalSection lock(&sLock);
    (*sThreadMap)[currentThreadId] = mPreviousDllName;
  }

  bool BailOut() const
  {
    return mReentered;
  };

  static void InitializeStatics()
  {
    InitializeCriticalSection(&sLock);
    sThreadMap = new std::map<DWORD, const char*>;
  }

private:
  static std::map<DWORD, const char*>* sThreadMap;

  const char* mPreviousDllName;
  bool mReentered;
};

std::map<DWORD, const char*>* ReentrancySentinel::sThreadMap;

/**
 * This is a linked list of DLLs that have been blocked. It doesn't use
 * mozilla::LinkedList because this is an append-only list and doesn't need
 * to be doubly linked.
 */
class DllBlockSet
{
public:
  static void Add(const char* name, unsigned long long version);

  // Write the list of blocked DLLs to a file HANDLE. This method is run after
  // a crash occurs and must therefore not use the heap, etc.
  static void Write(HANDLE file);

private:
  DllBlockSet(const char* name, unsigned long long version)
    : mName(name)
    , mVersion(version)
    , mNext(nullptr)
  {
  }

  const char* mName; // points into the gWindowsDllBlocklist string
  unsigned long long mVersion;
  DllBlockSet* mNext;

  static DllBlockSet* gFirst;
};

DllBlockSet* DllBlockSet::gFirst;

void
DllBlockSet::Add(const char* name, unsigned long long version)
{
  AutoCriticalSection lock(&sLock);
  for (DllBlockSet* b = gFirst; b; b = b->mNext) {
    if (0 == strcmp(b->mName, name) && b->mVersion == version) {
      return;
    }
  }
  // Not already present
  DllBlockSet* n = new DllBlockSet(name, version);
  n->mNext = gFirst;
  gFirst = n;
}

void
DllBlockSet::Write(HANDLE file)
{
  // It would be nicer to use AutoCriticalSection here. However, its destructor
  // might not run if an exception occurs, in which case we would never leave
  // the critical section. (MSVC warns about this possibility.) So we
  // enter and leave manually.
  ::EnterCriticalSection(&sLock);

  // Because this method is called after a crash occurs, and uses heap memory,
  // protect this entire block with a structured exception handler.
  MOZ_SEH_TRY {
    DWORD nBytes;
    for (DllBlockSet* b = gFirst; b; b = b->mNext) {
      // write name[,v.v.v.v];
      WriteFile(file, b->mName, strlen(b->mName), &nBytes, nullptr);
      if (b->mVersion != ALL_VERSIONS) {
        WriteFile(file, ",", 1, &nBytes, nullptr);
        uint16_t parts[4];
        parts[0] = b->mVersion >> 48;
        parts[1] = (b->mVersion >> 32) & 0xFFFF;
        parts[2] = (b->mVersion >> 16) & 0xFFFF;
        parts[3] = b->mVersion & 0xFFFF;
        for (int p = 0; p < 4; ++p) {
          char buf[32];
          ltoa(parts[p], buf, 10);
          WriteFile(file, buf, strlen(buf), &nBytes, nullptr);
          if (p != 3) {
            WriteFile(file, ".", 1, &nBytes, nullptr);
          }
        }
      }
      WriteFile(file, ";", 1, &nBytes, nullptr);
    }
  }
  MOZ_SEH_EXCEPT (EXCEPTION_EXECUTE_HANDLER) { }

  ::LeaveCriticalSection(&sLock);
}

static UniquePtr<wchar_t[]>
getFullPath (PWCHAR filePath, wchar_t* fname)
{
  // In Windows 8, the first parameter seems to be used for more than just the
  // path name.  For example, its numerical value can be 1.  Passing a non-valid
  // pointer to SearchPathW will cause a crash, so we need to check to see if we
  // are handed a valid pointer, and otherwise just pass nullptr to SearchPathW.
  PWCHAR sanitizedFilePath = nullptr;
  if ((uintptr_t(filePath) >= 65536) && ((uintptr_t(filePath) & 1) == 0)) {
    sanitizedFilePath = filePath;
  }

  // figure out the length of the string that we need
  DWORD pathlen = SearchPathW(sanitizedFilePath, fname, L".dll", 0, nullptr,
                              nullptr);
  if (pathlen == 0) {
    return nullptr;
  }

  auto full_fname = MakeUnique<wchar_t[]>(pathlen+1);
  if (!full_fname) {
    // couldn't allocate memory?
    return nullptr;
  }

  // now actually grab it
  SearchPathW(sanitizedFilePath, fname, L".dll", pathlen + 1, full_fname.get(),
              nullptr);
  return full_fname;
}

// No builtin function to find the last character matching a set
static wchar_t* lastslash(wchar_t* s, int len)
{
  for (wchar_t* c = s + len - 1; c >= s; --c) {
    if (*c == L'\\' || *c == L'/') {
      return c;
    }
  }
  return nullptr;
}


#ifdef ENABLE_TESTS
DllLoadHookType gDllLoadHook = nullptr;

void
DllBlocklist_SetDllLoadHook(DllLoadHookType aHook)
{
  gDllLoadHook = aHook;
}

void
CallDllLoadHook(bool aDllLoaded, NTSTATUS aStatus, HANDLE aDllBase, PUNICODE_STRING aDllName)
{
  if (gDllLoadHook) {
    gDllLoadHook(aDllLoaded, aStatus, aDllBase, aDllName);
  }
}

CreateThreadHookType gCreateThreadHook = nullptr;

void
DllBlocklist_SetCreateThreadHook(CreateThreadHookType aHook)
{
  gCreateThreadHook = aHook;
}

void
CallCreateThreadHook(bool aWasAllowed, void* aStartAddress)
{
  if (gCreateThreadHook) {
    gCreateThreadHook(aWasAllowed, aStartAddress);
  }
}

#else // ENABLE_TESTS
#define CallDllLoadHook(...)
#define CallCreateThreadHook(...)
#endif // ENABLE_TESTS

static NTSTATUS NTAPI
patched_LdrLoadDll (PWCHAR filePath, PULONG flags, PUNICODE_STRING moduleFileName, PHANDLE handle)
{
  // We have UCS2 (UTF16?), we want ASCII, but we also just want the filename portion
#define DLLNAME_MAX 128
  char dllName[DLLNAME_MAX+1];
  wchar_t *dll_part;
  char *dot;

  int len = moduleFileName->Length / 2;
  wchar_t *fname = moduleFileName->Buffer;
  UniquePtr<wchar_t[]> full_fname;

  // The filename isn't guaranteed to be null terminated, but in practice
  // it always will be; ensure that this is so, and bail if not.
  // This is done instead of the more robust approach because of bug 527122,
  // where lots of weird things were happening when we tried to make a copy.
  if (moduleFileName->MaximumLength < moduleFileName->Length+2 ||
      fname[len] != 0)
  {
#ifdef DEBUG
    printf_stderr("LdrLoadDll: non-null terminated string found!\n");
#endif
    goto continue_loading;
  }

  dll_part = lastslash(fname, len);
  if (dll_part) {
    dll_part = dll_part + 1;
    len -= dll_part - fname;
  } else {
    dll_part = fname;
  }

#ifdef DEBUG_very_verbose
  printf_stderr("LdrLoadDll: dll_part '%S' %d\n", dll_part, len);
#endif

  // if it's too long, then, we assume we won't want to block it,
  // since DLLNAME_MAX should be at least long enough to hold the longest
  // entry in our blocklist.
  if (len > DLLNAME_MAX) {
#ifdef DEBUG
    printf_stderr("LdrLoadDll: len too long! %d\n", len);
#endif
    goto continue_loading;
  }

  // copy over to our char byte buffer, lowercasing ASCII as we go
  for (int i = 0; i < len; i++) {
    wchar_t c = dll_part[i];

    if (c > 0x7f) {
      // welp, it's not ascii; if we need to add non-ascii things to
      // our blocklist, we'll have to remove this limitation.
      goto continue_loading;
    }

    // ensure that dll name is all lowercase
    if (c >= 'A' && c <= 'Z')
      c += 'a' - 'A';

    dllName[i] = (char) c;
  }

  dllName[len] = 0;

#ifdef DEBUG_very_verbose
  printf_stderr("LdrLoadDll: dll name '%s'\n", dllName);
#endif

  if (!(sInitFlags & eDllBlocklistInitFlagWasBootstrapped)) {
    // Block a suspicious binary that uses various 12-digit hex strings
    // e.g. MovieMode.48CA2AEFA22D.dll (bug 973138)
    dot = strchr(dllName, '.');
    if (dot && (strchr(dot+1, '.') == dot+13)) {
      char * end = nullptr;
      _strtoui64(dot+1, &end, 16);
      if (end == dot+13) {
        CallDllLoadHook(false, STATUS_DLL_NOT_FOUND, 0, moduleFileName);
        return STATUS_DLL_NOT_FOUND;
      }
    }
    // Block binaries where the filename is at least 16 hex digits
    if (dot && ((dot - dllName) >= 16)) {
      char * current = dllName;
      while (current < dot && isxdigit(*current)) {
        current++;
      }
      if (current == dot) {
        CallDllLoadHook(false, STATUS_DLL_NOT_FOUND, 0, moduleFileName);
        return STATUS_DLL_NOT_FOUND;
      }
    }

    // then compare to everything on the blocklist
    DECLARE_POINTER_TO_FIRST_DLL_BLOCKLIST_ENTRY(info);
    while (info->name) {
      if (strcmp(info->name, dllName) == 0)
        break;

      info++;
    }

    if (info->name) {
      bool load_ok = false;

#ifdef DEBUG_very_verbose
      printf_stderr("LdrLoadDll: info->name: '%s'\n", info->name);
#endif

      if ((info->flags & DllBlockInfo::BLOCK_WIN8PLUS_ONLY) &&
          !IsWin8OrLater()) {
        goto continue_loading;
      }

      if ((info->flags & DllBlockInfo::BLOCK_WIN8_ONLY) &&
          (!IsWin8OrLater() || IsWin8Point1OrLater())) {
        goto continue_loading;
      }

      if ((info->flags & DllBlockInfo::CHILD_PROCESSES_ONLY) &&
          !(sInitFlags & eDllBlocklistInitFlagIsChildProcess)) {
        goto continue_loading;
      }

      unsigned long long fVersion = ALL_VERSIONS;

      if (info->maxVersion != ALL_VERSIONS) {
        ReentrancySentinel sentinel(dllName);
        if (sentinel.BailOut()) {
          goto continue_loading;
        }

        full_fname = getFullPath(filePath, fname);
        if (!full_fname) {
          // uh, we couldn't find the DLL at all, so...
          printf_stderr("LdrLoadDll: Blocking load of '%s' (SearchPathW didn't find it?)\n", dllName);
          CallDllLoadHook(false, STATUS_DLL_NOT_FOUND, 0, moduleFileName);
          return STATUS_DLL_NOT_FOUND;
        }

        if (info->flags & DllBlockInfo::USE_TIMESTAMP) {
          fVersion = GetTimestamp(full_fname.get());
          if (fVersion > info->maxVersion) {
            load_ok = true;
          }
        } else {
          DWORD zero;
          DWORD infoSize = GetFileVersionInfoSizeW(full_fname.get(), &zero);

          // If we failed to get the version information, we block.

          if (infoSize != 0) {
            auto infoData = MakeUnique<unsigned char[]>(infoSize);
            VS_FIXEDFILEINFO *vInfo;
            UINT vInfoLen;

            if (GetFileVersionInfoW(full_fname.get(), 0, infoSize, infoData.get()) &&
                VerQueryValueW(infoData.get(), L"\\", (LPVOID*) &vInfo, &vInfoLen))
            {
              fVersion =
                ((unsigned long long)vInfo->dwFileVersionMS) << 32 |
                ((unsigned long long)vInfo->dwFileVersionLS);

              // finally do the version check, and if it's greater than our block
              // version, keep loading
              if (fVersion > info->maxVersion)
                load_ok = true;
            }
          }
        }
      }

      if (!load_ok) {
        printf_stderr("LdrLoadDll: Blocking load of '%s' -- see http://www.mozilla.com/en-US/blocklist/\n", dllName);
        DllBlockSet::Add(info->name, fVersion);
        CallDllLoadHook(false, STATUS_DLL_NOT_FOUND, 0, moduleFileName);
        return STATUS_DLL_NOT_FOUND;
      }
    }
  }

continue_loading:
#ifdef DEBUG_very_verbose
    printf_stderr("LdrLoadDll: continuing load... ('%S')\n", moduleFileName->Buffer);
#endif

  // A few DLLs such as xul.dll and nss3.dll get loaded before mozglue's
  // AutoProfilerLabel is initialized, and this is a no-op in those cases. But
  // the vast majority of DLLs do get labelled here.
  AutoProfilerLabel label("WindowsDllBlocklist::patched_LdrLoadDll", dllName,
                          __LINE__);

#ifdef _M_AMD64
  // Prevent the stack walker from suspending this thread when LdrLoadDll
  // holds the RtlLookupFunctionEntry lock.
  AutoSuppressStackWalking suppress;
#endif

  NTSTATUS ret = stub_LdrLoadDll(filePath, flags, moduleFileName, handle);
  CallDllLoadHook(true, ret, handle ? *handle : 0, moduleFileName);
  return ret;
}

#if defined(NIGHTLY_BUILD)
// Map of specific thread proc addresses we should block. In particular,
// LoadLibrary* APIs which indicate DLL injection
static mozilla::Vector<void*, 4>* gStartAddressesToBlock;
#endif

static bool
ShouldBlockThread(void* aStartAddress)
{
  // Allows crashfirefox.exe to continue to work. Also if your threadproc is null, this crash is intentional.
  if (aStartAddress == 0)
    return false;

#if defined(NIGHTLY_BUILD)
  for (auto p : *gStartAddressesToBlock) {
    if (p == aStartAddress) {
      return true;
    }
  }
#endif

  bool shouldBlock = false;
  MEMORY_BASIC_INFORMATION startAddressInfo = {0};
  if (VirtualQuery(aStartAddress, &startAddressInfo, sizeof(startAddressInfo))) {
    shouldBlock |= startAddressInfo.State != MEM_COMMIT;
    shouldBlock |= startAddressInfo.Protect != PAGE_EXECUTE_READ;
  }

  return shouldBlock;
}

// Allows blocked threads to still run normally through BaseThreadInitThunk, in case there's any magic there that we shouldn't skip.
static DWORD WINAPI
NopThreadProc(void* /* aThreadParam */)
{
  return 0;
}

static MOZ_NORETURN void __fastcall
patched_BaseThreadInitThunk(BOOL aIsInitialThread, void* aStartAddress,
                            void* aThreadParam)
{
  if (ShouldBlockThread(aStartAddress)) {
    CallCreateThreadHook(false, aStartAddress);
    aStartAddress = (void*)NopThreadProc;
  } else {
    CallCreateThreadHook(true, aStartAddress);
  }

  stub_BaseThreadInitThunk(aIsInitialThread, aStartAddress, aThreadParam);
}


static WindowsDllInterceptor NtDllIntercept;
static WindowsDllInterceptor Kernel32Intercept;

static void
GetNativeNtBlockSetWriter();

MFBT_API void
DllBlocklist_Initialize(uint32_t aInitFlags)
{
  if (sBlocklistInitAttempted) {
    return;
  }
  sInitFlags = aInitFlags;

  if (sInitFlags & eDllBlocklistInitFlagWasBootstrapped) {
    GetNativeNtBlockSetWriter();
  }

  sBlocklistInitAttempted = true;
#if defined(NIGHTLY_BUILD)
  gStartAddressesToBlock = new mozilla::Vector<void*, 4>;
#endif

  // In order to be effective against AppInit DLLs, the blocklist must be
  // initialized before user32.dll is loaded into the process (bug 932100).
  if (GetModuleHandleA("user32.dll")) {
    sUser32BeforeBlocklist = true;
#ifdef DEBUG
    printf_stderr("DLL blocklist was unable to intercept AppInit DLLs.\n");
#endif
  }

  NtDllIntercept.Init("ntdll.dll");

  ReentrancySentinel::InitializeStatics();

  // We specifically use a detour, because there are cases where external
  // code also tries to hook LdrLoadDll, and doesn't know how to relocate our
  // nop space patches. (Bug 951827)
  bool ok = stub_LdrLoadDll.SetDetour(NtDllIntercept, "LdrLoadDll",
                                      &patched_LdrLoadDll);

  if (!ok) {
    sBlocklistInitFailed = true;
#ifdef DEBUG
    printf_stderr("LdrLoadDll hook failed, no dll blocklisting active\n");
#endif
  }

  // If someone injects a thread early that causes user32.dll to load off the
  // main thread this causes issues, so load it as soon as we've initialized
  // the block-list. (See bug 1400637)
  if (!sUser32BeforeBlocklist) {
    ::LoadLibraryW(L"user32.dll");
  }

  Kernel32Intercept.Init("kernel32.dll");

#ifdef _M_AMD64
  if (!IsWin8OrLater()) {
    // The crash that this hook works around is only seen on Win7.
    stub_RtlInstallFunctionTableCallback.Set(Kernel32Intercept,
                                             "RtlInstallFunctionTableCallback",
                                             &patched_RtlInstallFunctionTableCallback);
  }
#endif

  // Bug 1361410: WRusr.dll will overwrite our hook and cause a crash.
  // Workaround: If we detect WRusr.dll, don't hook.
  if (!GetModuleHandleW(L"WRusr.dll")) {
    if (!stub_BaseThreadInitThunk.SetDetour(Kernel32Intercept,
                                            "BaseThreadInitThunk",
                                            &patched_BaseThreadInitThunk)) {
#ifdef DEBUG
    printf_stderr("BaseThreadInitThunk hook failed\n");
#endif
    }
  }

#if defined(NIGHTLY_BUILD)
  // Populate a list of thread start addresses to block.
  HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
  if (hKernel) {
    void* pProc;

    pProc = (void*)GetProcAddress(hKernel, "LoadLibraryA");
    if (pProc) {
      gStartAddressesToBlock->append(pProc);
    }

    pProc = (void*)GetProcAddress(hKernel, "LoadLibraryW");
    if (pProc) {
      gStartAddressesToBlock->append(pProc);
    }

    pProc = (void*)GetProcAddress(hKernel, "LoadLibraryExA");
    if (pProc) {
      gStartAddressesToBlock->append(pProc);
    }

    pProc = (void*)GetProcAddress(hKernel, "LoadLibraryExW");
    if (pProc) {
      gStartAddressesToBlock->append(pProc);
    }
  }
#endif
}

static void
InternalWriteNotes(HANDLE file)
{
  DWORD nBytes;

  WriteFile(file, kBlockedDllsParameter, kBlockedDllsParameterLen, &nBytes, nullptr);
  DllBlockSet::Write(file);
  WriteFile(file, "\n", 1, &nBytes, nullptr);

  if (sBlocklistInitFailed) {
    WriteFile(file, kBlocklistInitFailedParameter,
              kBlocklistInitFailedParameterLen, &nBytes, nullptr);
  }

  if (sUser32BeforeBlocklist) {
    WriteFile(file, kUser32BeforeBlocklistParameter,
              kUser32BeforeBlocklistParameterLen, &nBytes, nullptr);
  }
}

using WriterFn = void (*)(HANDLE);
static WriterFn gWriterFn = &InternalWriteNotes;

static void
GetNativeNtBlockSetWriter()
{
  auto nativeWriter = reinterpret_cast<WriterFn>(
    ::GetProcAddress(::GetModuleHandleW(nullptr), "NativeNtBlockSet_Write"));
  if (nativeWriter) {
    gWriterFn = nativeWriter;
  }
}

MFBT_API void
DllBlocklist_WriteNotes(HANDLE file)
{
  MOZ_ASSERT(gWriterFn);
  gWriterFn(file);
}

MFBT_API bool
DllBlocklist_CheckStatus()
{
  if (sBlocklistInitFailed || sUser32BeforeBlocklist)
    return false;
  return true;
}

// ============================================================================
// This section is for DLL Services
// ============================================================================


static SRWLOCK gDllServicesLock = SRWLOCK_INIT;
static mozilla::glue::detail::DllServicesBase* gDllServices;

class MOZ_RAII AutoSharedLock final
{
public:
  explicit AutoSharedLock(SRWLOCK& aLock)
    : mLock(aLock)
  {
    ::AcquireSRWLockShared(&aLock);
  }

  ~AutoSharedLock()
  {
    ::ReleaseSRWLockShared(&mLock);
  }

  AutoSharedLock(const AutoSharedLock&) = delete;
  AutoSharedLock(AutoSharedLock&&) = delete;
  AutoSharedLock& operator=(const AutoSharedLock&) = delete;
  AutoSharedLock& operator=(AutoSharedLock&&) = delete;

private:
  SRWLOCK& mLock;
};

class MOZ_RAII AutoExclusiveLock final
{
public:
  explicit AutoExclusiveLock(SRWLOCK& aLock)
    : mLock(aLock)
  {
    ::AcquireSRWLockExclusive(&aLock);
  }

  ~AutoExclusiveLock()
  {
    ::ReleaseSRWLockExclusive(&mLock);
  }

  AutoExclusiveLock(const AutoExclusiveLock&) = delete;
  AutoExclusiveLock(AutoExclusiveLock&&) = delete;
  AutoExclusiveLock& operator=(const AutoExclusiveLock&) = delete;
  AutoExclusiveLock& operator=(AutoExclusiveLock&&) = delete;

private:
  SRWLOCK& mLock;
};

// These types are documented on MSDN but not provided in any SDK headers

enum DllNotificationReason
{
  LDR_DLL_NOTIFICATION_REASON_LOADED = 1,
  LDR_DLL_NOTIFICATION_REASON_UNLOADED = 2
};

typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA {
  ULONG Flags;                    //Reserved.
  PCUNICODE_STRING FullDllName;   //The full path name of the DLL module.
  PCUNICODE_STRING BaseDllName;   //The base file name of the DLL module.
  PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
  ULONG SizeOfImage;              //The size of the DLL image, in bytes.
} LDR_DLL_LOADED_NOTIFICATION_DATA, *PLDR_DLL_LOADED_NOTIFICATION_DATA;

typedef struct _LDR_DLL_UNLOADED_NOTIFICATION_DATA {
  ULONG Flags;                    //Reserved.
  PCUNICODE_STRING FullDllName;   //The full path name of the DLL module.
  PCUNICODE_STRING BaseDllName;   //The base file name of the DLL module.
  PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
  ULONG SizeOfImage;              //The size of the DLL image, in bytes.
} LDR_DLL_UNLOADED_NOTIFICATION_DATA, *PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

typedef union _LDR_DLL_NOTIFICATION_DATA {
  LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
  LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
} LDR_DLL_NOTIFICATION_DATA, *PLDR_DLL_NOTIFICATION_DATA;

typedef const LDR_DLL_NOTIFICATION_DATA* PCLDR_DLL_NOTIFICATION_DATA;

typedef VOID (CALLBACK* PLDR_DLL_NOTIFICATION_FUNCTION)(
          ULONG aReason,
          PCLDR_DLL_NOTIFICATION_DATA aNotificationData,
          PVOID aContext);

NTSTATUS NTAPI
LdrRegisterDllNotification(ULONG aFlags,
                           PLDR_DLL_NOTIFICATION_FUNCTION aCallback,
                           PVOID aContext, PVOID* aCookie);

static PVOID gNotificationCookie;

static VOID CALLBACK
DllLoadNotification(ULONG aReason, PCLDR_DLL_NOTIFICATION_DATA aNotificationData,
                    PVOID aContext)
{
  if (aReason != LDR_DLL_NOTIFICATION_REASON_LOADED) {
    // We don't care about unloads
    return;
  }

  AutoSharedLock lock(gDllServicesLock);
  if (!gDllServices) {
    return;
  }

  PCUNICODE_STRING fullDllName = aNotificationData->Loaded.FullDllName;
  gDllServices->DispatchDllLoadNotification(fullDllName);
}

namespace mozilla {
Authenticode* GetAuthenticode();
} // namespace mozilla

MFBT_API void
DllBlocklist_SetDllServices(mozilla::glue::detail::DllServicesBase* aSvc)
{
  AutoExclusiveLock lock(gDllServicesLock);

  if (aSvc) {
    aSvc->SetAuthenticodeImpl(GetAuthenticode());

    if (!gNotificationCookie) {
      auto pLdrRegisterDllNotification =
        reinterpret_cast<decltype(&::LdrRegisterDllNotification)>(
          ::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"),
                           "LdrRegisterDllNotification"));

      MOZ_DIAGNOSTIC_ASSERT(pLdrRegisterDllNotification);

      NTSTATUS ntStatus = pLdrRegisterDllNotification(0, &DllLoadNotification,
                                                      nullptr, &gNotificationCookie);
      MOZ_DIAGNOSTIC_ASSERT(NT_SUCCESS(ntStatus));
    }
  }

  gDllServices = aSvc;
}

