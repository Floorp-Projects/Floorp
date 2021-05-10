/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <winternl.h>

#pragma warning(push)
#pragma warning(disable : 4275 4530)  // See msvc-stl-wrapper.template.h
#include <map>
#pragma warning(pop)

#include "Authenticode.h"
#include "BaseProfiler.h"
#include "nsWindowsDllInterceptor.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/StackWalk_windows.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "mozilla/WindowsProcessMitigations.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "nsWindowsHelpers.h"
#include "WindowsDllBlocklist.h"
#include "mozilla/AutoProfilerLabel.h"
#include "mozilla/glue/Debug.h"
#include "mozilla/glue/WindowsDllServices.h"
#include "mozilla/glue/WinUtils.h"

// Start new implementation
#include "LoaderObserver.h"
#include "ModuleLoadFrame.h"
#include "mozilla/glue/WindowsUnicode.h"

namespace mozilla {

glue::Win32SRWLock gDllServicesLock;
glue::detail::DllServicesBase* gDllServices;

}  // namespace mozilla

using namespace mozilla;

using CrashReporter::Annotation;
using CrashReporter::AnnotationWriter;

#define DLL_BLOCKLIST_ENTRY(name, ...) {name, __VA_ARGS__},
#define DLL_BLOCKLIST_STRING_TYPE const char*
#include "mozilla/WindowsDllBlocklistLegacyDefs.h"

// define this for very verbose dll load debug spew
#undef DEBUG_very_verbose

static uint32_t sInitFlags;
static bool sBlocklistInitAttempted;
static bool sBlocklistInitFailed;
static bool sUser32BeforeBlocklist;

typedef MOZ_NORETURN_PTR void(__fastcall* BaseThreadInitThunk_func)(
    BOOL aIsInitialThread, void* aStartAddress, void* aThreadParam);
static WindowsDllInterceptor::FuncHookType<BaseThreadInitThunk_func>
    stub_BaseThreadInitThunk;

typedef NTSTATUS(NTAPI* LdrLoadDll_func)(PWCHAR filePath, PULONG flags,
                                         PUNICODE_STRING moduleFileName,
                                         PHANDLE handle);
static WindowsDllInterceptor::FuncHookType<LdrLoadDll_func> stub_LdrLoadDll;

#ifdef _M_AMD64
typedef decltype(RtlInstallFunctionTableCallback)*
    RtlInstallFunctionTableCallback_func;
static WindowsDllInterceptor::FuncHookType<RtlInstallFunctionTableCallback_func>
    stub_RtlInstallFunctionTableCallback;

extern uint8_t* sMsMpegJitCodeRegionStart;
extern size_t sMsMpegJitCodeRegionSize;

BOOLEAN WINAPI patched_RtlInstallFunctionTableCallback(
    DWORD64 TableIdentifier, DWORD64 BaseAddress, DWORD Length,
    PGET_RUNTIME_FUNCTION_CALLBACK Callback, PVOID Context,
    PCWSTR OutOfProcessCallbackDll) {
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

    DWORD alignedOffset =
        (offset / info.dwAllocationGranularity) * info.dwAllocationGranularity;

    MOZ_ASSERT(offset - alignedOffset < info.dwAllocationGranularity, "Wtf");

    mRealView = ::MapViewOfFile(map, FILE_MAP_READ, 0, alignedOffset,
                                sizeof(T) + (offset - alignedOffset));

    mMappedView =
        mRealView
            ? reinterpret_cast<T*>((char*)mRealView + (offset - alignedOffset))
            : nullptr;
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

static DWORD GetTimestamp(const wchar_t* path) {
  DWORD timestamp = 0;

  HANDLE file = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file != INVALID_HANDLE_VALUE) {
    HANDLE map =
        ::CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
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
class ReentrancySentinel {
 public:
  explicit ReentrancySentinel(const char* dllName) {
    DWORD currentThreadId = GetCurrentThreadId();
    AutoCriticalSection lock(&sLock);
    mPreviousDllName = (*sThreadMap)[currentThreadId];

    // If there is a DLL currently being loaded and it has the same name
    // as the current attempt, we're re-entering.
    mReentered = mPreviousDllName && !stricmp(mPreviousDllName, dllName);
    (*sThreadMap)[currentThreadId] = dllName;
  }

  ~ReentrancySentinel() {
    DWORD currentThreadId = GetCurrentThreadId();
    AutoCriticalSection lock(&sLock);
    (*sThreadMap)[currentThreadId] = mPreviousDllName;
  }

  bool BailOut() const { return mReentered; };

  static void InitializeStatics() {
    InitializeCriticalSection(&sLock);
    sThreadMap = new std::map<DWORD, const char*>;
  }

 private:
  static std::map<DWORD, const char*>* sThreadMap;

  const char* mPreviousDllName;
  bool mReentered;
};

std::map<DWORD, const char*>* ReentrancySentinel::sThreadMap;

class WritableBuffer {
 public:
  WritableBuffer() : mBuffer{0}, mLen(0) {}

  void Write(const char* aData, size_t aLen) {
    size_t writable_len = std::min(aLen, Available());
    memcpy(mBuffer + mLen, aData, writable_len);
    mLen += writable_len;
  }

  size_t const Length() { return mLen; }
  const char* Data() { return mBuffer; }

 private:
  size_t const Available() { return sizeof(mBuffer) - mLen; }

  char mBuffer[1024];
  size_t mLen;
};

/**
 * This is a linked list of DLLs that have been blocked. It doesn't use
 * mozilla::LinkedList because this is an append-only list and doesn't need
 * to be doubly linked.
 */
class DllBlockSet {
 public:
  static void Add(const char* name, unsigned long long version);

  // Write the list of blocked DLLs to a WritableBuffer object. This method is
  // run after a crash occurs and must therefore not use the heap, etc.
  static void Write(WritableBuffer& buffer);

 private:
  DllBlockSet(const char* name, unsigned long long version)
      : mName(name), mVersion(version), mNext(nullptr) {}

  const char* mName;  // points into the gWindowsDllBlocklist string
  unsigned long long mVersion;
  DllBlockSet* mNext;

  static DllBlockSet* gFirst;
};

DllBlockSet* DllBlockSet::gFirst;

void DllBlockSet::Add(const char* name, unsigned long long version) {
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

void DllBlockSet::Write(WritableBuffer& buffer) {
  // It would be nicer to use AutoCriticalSection here. However, its destructor
  // might not run if an exception occurs, in which case we would never leave
  // the critical section. (MSVC warns about this possibility.) So we
  // enter and leave manually.
  ::EnterCriticalSection(&sLock);

  // Because this method is called after a crash occurs, and uses heap memory,
  // protect this entire block with a structured exception handler.
  MOZ_SEH_TRY {
    for (DllBlockSet* b = gFirst; b; b = b->mNext) {
      // write name[,v.v.v.v];
      buffer.Write(b->mName, strlen(b->mName));
      if (b->mVersion != DllBlockInfo::ALL_VERSIONS) {
        buffer.Write(",", 1);
        uint16_t parts[4];
        parts[0] = b->mVersion >> 48;
        parts[1] = (b->mVersion >> 32) & 0xFFFF;
        parts[2] = (b->mVersion >> 16) & 0xFFFF;
        parts[3] = b->mVersion & 0xFFFF;
        for (int p = 0; p < 4; ++p) {
          char buf[32];
          _ltoa_s(parts[p], buf, sizeof(buf), 10);
          buffer.Write(buf, strlen(buf));
          if (p != 3) {
            buffer.Write(".", 1);
          }
        }
      }
      buffer.Write(";", 1);
    }
  }
  MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) {}

  ::LeaveCriticalSection(&sLock);
}

static UniquePtr<wchar_t[]> getFullPath(PWCHAR filePath, wchar_t* fname) {
  // In Windows 8, the first parameter seems to be used for more than just the
  // path name.  For example, its numerical value can be 1.  Passing a non-valid
  // pointer to SearchPathW will cause a crash, so we need to check to see if we
  // are handed a valid pointer, and otherwise just pass nullptr to SearchPathW.
  PWCHAR sanitizedFilePath = nullptr;
  if ((uintptr_t(filePath) >= 65536) && ((uintptr_t(filePath) & 1) == 0)) {
    sanitizedFilePath = filePath;
  }

  // figure out the length of the string that we need
  DWORD pathlen =
      SearchPathW(sanitizedFilePath, fname, L".dll", 0, nullptr, nullptr);
  if (pathlen == 0) {
    return nullptr;
  }

  auto full_fname = MakeUnique<wchar_t[]>(pathlen + 1);
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
static wchar_t* lastslash(wchar_t* s, int len) {
  for (wchar_t* c = s + len - 1; c >= s; --c) {
    if (*c == L'\\' || *c == L'/') {
      return c;
    }
  }
  return nullptr;
}

static NTSTATUS NTAPI patched_LdrLoadDll(PWCHAR filePath, PULONG flags,
                                         PUNICODE_STRING moduleFileName,
                                         PHANDLE handle) {
  // We have UCS2 (UTF16?), we want ASCII, but we also just want the filename
  // portion
#define DLLNAME_MAX 128
  char dllName[DLLNAME_MAX + 1];
  wchar_t* dll_part;
  char* dot;

  int len = moduleFileName->Length / 2;
  wchar_t* fname = moduleFileName->Buffer;
  UniquePtr<wchar_t[]> full_fname;

  // The filename isn't guaranteed to be null terminated, but in practice
  // it always will be; ensure that this is so, and bail if not.
  // This is done instead of the more robust approach because of bug 527122,
  // where lots of weird things were happening when we tried to make a copy.
  if (moduleFileName->MaximumLength < moduleFileName->Length + 2 ||
      fname[len] != 0) {
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
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';

    dllName[i] = (char)c;
  }

  dllName[len] = 0;

#ifdef DEBUG_very_verbose
  printf_stderr("LdrLoadDll: dll name '%s'\n", dllName);
#endif

  if (!(sInitFlags & eDllBlocklistInitFlagWasBootstrapped)) {
    // Block a suspicious binary that uses various 12-digit hex strings
    // e.g. MovieMode.48CA2AEFA22D.dll (bug 973138)
    dot = strchr(dllName, '.');
    if (dot && (strchr(dot + 1, '.') == dot + 13)) {
      char* end = nullptr;
      _strtoui64(dot + 1, &end, 16);
      if (end == dot + 13) {
        return STATUS_DLL_NOT_FOUND;
      }
    }
    // Block binaries where the filename is at least 16 hex digits
    if (dot && ((dot - dllName) >= 16)) {
      char* current = dllName;
      while (current < dot && isxdigit(*current)) {
        current++;
      }
      if (current == dot) {
        return STATUS_DLL_NOT_FOUND;
      }
    }

    // then compare to everything on the blocklist
    DECLARE_POINTER_TO_FIRST_DLL_BLOCKLIST_ENTRY(info);
    while (info->mName) {
      if (strcmp(info->mName, dllName) == 0) break;

      info++;
    }

    if (info->mName) {
      bool load_ok = false;

#ifdef DEBUG_very_verbose
      printf_stderr("LdrLoadDll: info->mName: '%s'\n", info->mName);
#endif

      if (info->mFlags & DllBlockInfo::REDIRECT_TO_NOOP_ENTRYPOINT) {
        printf_stderr(
            "LdrLoadDll: "
            "Ignoring the REDIRECT_TO_NOOP_ENTRYPOINT flag\n");
      }

      if ((info->mFlags & DllBlockInfo::BLOCK_WIN8_AND_OLDER) &&
          IsWin8Point1OrLater()) {
        goto continue_loading;
      }

      if ((info->mFlags & DllBlockInfo::BLOCK_WIN7_AND_OLDER) &&
          IsWin8OrLater()) {
        goto continue_loading;
      }

      if ((info->mFlags & DllBlockInfo::CHILD_PROCESSES_ONLY) &&
          !(sInitFlags & eDllBlocklistInitFlagIsChildProcess)) {
        goto continue_loading;
      }

      if ((info->mFlags & DllBlockInfo::BROWSER_PROCESS_ONLY) &&
          (sInitFlags & eDllBlocklistInitFlagIsChildProcess)) {
        goto continue_loading;
      }

      unsigned long long fVersion = DllBlockInfo::ALL_VERSIONS;

      if (info->mMaxVersion != DllBlockInfo::ALL_VERSIONS) {
        ReentrancySentinel sentinel(dllName);
        if (sentinel.BailOut()) {
          goto continue_loading;
        }

        full_fname = getFullPath(filePath, fname);
        if (!full_fname) {
          // uh, we couldn't find the DLL at all, so...
          printf_stderr(
              "LdrLoadDll: Blocking load of '%s' (SearchPathW didn't find "
              "it?)\n",
              dllName);
          return STATUS_DLL_NOT_FOUND;
        }

        if (info->mFlags & DllBlockInfo::USE_TIMESTAMP) {
          fVersion = GetTimestamp(full_fname.get());
          if (fVersion > info->mMaxVersion) {
            load_ok = true;
          }
        } else {
          LauncherResult<ModuleVersion> version =
              GetModuleVersion(full_fname.get());
          // If we failed to get the version information, we block.
          if (version.isOk()) {
            load_ok = !info->IsVersionBlocked(version.unwrap());
          }
        }
      }

      if (!load_ok) {
        printf_stderr(
            "LdrLoadDll: Blocking load of '%s' -- see "
            "http://www.mozilla.com/en-US/blocklist/\n",
            dllName);
        DllBlockSet::Add(info->mName, fVersion);
        return STATUS_DLL_NOT_FOUND;
      }
    }
  }

continue_loading:
#ifdef DEBUG_very_verbose
  printf_stderr("LdrLoadDll: continuing load... ('%S')\n",
                moduleFileName->Buffer);
#endif

  glue::ModuleLoadFrame loadFrame(moduleFileName);

  NTSTATUS ret;
  HANDLE myHandle;

  ret = stub_LdrLoadDll(filePath, flags, moduleFileName, &myHandle);

  if (handle) {
    *handle = myHandle;
  }

  loadFrame.SetLoadStatus(ret, myHandle);

  return ret;
}

#if defined(NIGHTLY_BUILD)
// Map of specific thread proc addresses we should block. In particular,
// LoadLibrary* APIs which indicate DLL injection
static void* gStartAddressesToBlock[4];
#endif  // defined(NIGHTLY_BUILD)

static bool ShouldBlockThread(void* aStartAddress) {
  // Allows crashfirefox.exe to continue to work. Also if your threadproc is
  // null, this crash is intentional.
  if (aStartAddress == nullptr) return false;

#if defined(NIGHTLY_BUILD)
  for (auto p : gStartAddressesToBlock) {
    if (p == aStartAddress) {
      return true;
    }
  }
#endif

  bool shouldBlock = false;
  MEMORY_BASIC_INFORMATION startAddressInfo = {0};
  if (VirtualQuery(aStartAddress, &startAddressInfo,
                   sizeof(startAddressInfo))) {
    shouldBlock |= startAddressInfo.State != MEM_COMMIT;
    shouldBlock |= startAddressInfo.Protect != PAGE_EXECUTE_READ;
  }

  return shouldBlock;
}

// Allows blocked threads to still run normally through BaseThreadInitThunk, in
// case there's any magic there that we shouldn't skip.
static DWORD WINAPI NopThreadProc(void* /* aThreadParam */) { return 0; }

static MOZ_NORETURN void __fastcall patched_BaseThreadInitThunk(
    BOOL aIsInitialThread, void* aStartAddress, void* aThreadParam) {
  if (ShouldBlockThread(aStartAddress)) {
    aStartAddress = (void*)NopThreadProc;
  }

  stub_BaseThreadInitThunk(aIsInitialThread, aStartAddress, aThreadParam);
}

static WindowsDllInterceptor NtDllIntercept;
static WindowsDllInterceptor Kernel32Intercept;

static void GetNativeNtBlockSetWriter();

static glue::LoaderObserver gMozglueLoaderObserver;
static nt::WinLauncherFunctions gWinLauncherFunctions;

MFBT_API void DllBlocklist_Initialize(uint32_t aInitFlags) {
  if (sBlocklistInitAttempted) {
    return;
  }
  sBlocklistInitAttempted = true;

  sInitFlags = aInitFlags;

  glue::ModuleLoadFrame::StaticInit(&gMozglueLoaderObserver,
                                    &gWinLauncherFunctions);

#ifdef _M_AMD64
  if (!IsWin8OrLater()) {
    Kernel32Intercept.Init("kernel32.dll");

    // The crash that this hook works around is only seen on Win7.
    stub_RtlInstallFunctionTableCallback.Set(
        Kernel32Intercept, "RtlInstallFunctionTableCallback",
        &patched_RtlInstallFunctionTableCallback);
  }
#endif

  if (aInitFlags & eDllBlocklistInitFlagWasBootstrapped) {
    GetNativeNtBlockSetWriter();
    return;
  }

  // There are a couple of exceptional cases where we skip user32.dll check.
  // - If the the process was bootstrapped by the launcher process, AppInit
  //   DLLs will be intercepted by the new DllBlockList.  No need to check
  //   here.
  // - The code to initialize the base profiler loads winmm.dll which
  //   statically links user32.dll on an older Windows.  This means if the base
  //   profiler is active before coming here, we cannot fully intercept AppInit
  //   DLLs.  Given that the base profiler is used outside the typical use
  //   cases, it's ok not to check user32.dll in this scenario.
  const bool skipUser32Check =
      (sInitFlags & eDllBlocklistInitFlagWasBootstrapped)
#ifdef MOZ_GECKO_PROFILER
      ||
      (!IsWin10AnniversaryUpdateOrLater() && baseprofiler::profiler_is_active())
#endif
      ;

  // In order to be effective against AppInit DLLs, the blocklist must be
  // initialized before user32.dll is loaded into the process (bug 932100).
  if (!skipUser32Check && GetModuleHandleW(L"user32.dll")) {
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
  if (!sUser32BeforeBlocklist && !IsWin32kLockedDown()) {
    ::LoadLibraryW(L"user32.dll");
  }

  Kernel32Intercept.Init("kernel32.dll");

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
    gStartAddressesToBlock[0] = pProc;

    pProc = (void*)GetProcAddress(hKernel, "LoadLibraryW");
    gStartAddressesToBlock[1] = pProc;

    pProc = (void*)GetProcAddress(hKernel, "LoadLibraryExA");
    gStartAddressesToBlock[2] = pProc;

    pProc = (void*)GetProcAddress(hKernel, "LoadLibraryExW");
    gStartAddressesToBlock[3] = pProc;
  }
#endif
}

#ifdef DEBUG
MFBT_API void DllBlocklist_Shutdown() {}
#endif  // DEBUG

static void InternalWriteNotes(AnnotationWriter& aWriter) {
  WritableBuffer buffer;
  DllBlockSet::Write(buffer);

  aWriter.Write(Annotation::BlockedDllList, buffer.Data(), buffer.Length());

  if (sBlocklistInitFailed) {
    aWriter.Write(Annotation::BlocklistInitFailed, "1");
  }

  if (sUser32BeforeBlocklist) {
    aWriter.Write(Annotation::User32BeforeBlocklist, "1");
  }
}

using WriterFn = void (*)(AnnotationWriter&);
static WriterFn gWriterFn = &InternalWriteNotes;

static void GetNativeNtBlockSetWriter() {
  auto nativeWriter = reinterpret_cast<WriterFn>(
      ::GetProcAddress(::GetModuleHandleW(nullptr), "NativeNtBlockSet_Write"));
  if (nativeWriter) {
    gWriterFn = nativeWriter;
  }
}

MFBT_API void DllBlocklist_WriteNotes(AnnotationWriter& aWriter) {
  MOZ_ASSERT(gWriterFn);
  gWriterFn(aWriter);
}

MFBT_API bool DllBlocklist_CheckStatus() {
  if (sBlocklistInitFailed || sUser32BeforeBlocklist) return false;
  return true;
}

// ============================================================================
// This section is for DLL Services
// ============================================================================

namespace mozilla {
Authenticode* GetAuthenticode();
}  // namespace mozilla

/**
 * Please note that DllBlocklist_SetFullDllServices is called with
 * aSvc = nullptr to de-initialize the resources even though they
 * have been initialized via DllBlocklist_SetBasicDllServices.
 */
MFBT_API void DllBlocklist_SetFullDllServices(
    mozilla::glue::detail::DllServicesBase* aSvc) {
  glue::AutoExclusiveLock lock(gDllServicesLock);
  if (aSvc) {
    aSvc->SetAuthenticodeImpl(GetAuthenticode());
    aSvc->SetWinLauncherFunctions(gWinLauncherFunctions);
    gMozglueLoaderObserver.Forward(aSvc);
  }

  gDllServices = aSvc;
}

MFBT_API void DllBlocklist_SetBasicDllServices(
    mozilla::glue::detail::DllServicesBase* aSvc) {
  if (!aSvc) {
    return;
  }

  aSvc->SetAuthenticodeImpl(GetAuthenticode());
  gMozglueLoaderObserver.Disable();
}
