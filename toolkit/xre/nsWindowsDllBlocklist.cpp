/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <winternl.h>

#include <stdio.h>
#include <string.h>

#include <map>

#include "nsXULAppAPI.h"

#include "nsAutoPtr.h"
#include "nsThreadUtils.h"

#include "prlog.h"

#include "nsWindowsDllInterceptor.h"
#include "nsWindowsHelpers.h"

using namespace mozilla;

#if defined(MOZ_CRASHREPORTER) && !defined(NO_BLOCKLIST_CRASHREPORTER)
#include "nsExceptionHandler.h"
#endif

#define ALL_VERSIONS   ((unsigned long long)-1LL)

// DLLs sometimes ship without a version number, particularly early
// releases. Blocking "version <= 0" has the effect of blocking unversioned
// DLLs (since the call to get version info fails), but not blocking
// any versioned instance.
#define UNVERSIONED    ((unsigned long long)0LL)

// Convert the 4 (decimal) components of a DLL version number into a
// single unsigned long long, as needed by the blocklist
#define MAKE_VERSION(a,b,c,d)\
  ((a##ULL << 48) + (b##ULL << 32) + (c##ULL << 16) + d##ULL)

struct DllBlockInfo {
  // The name of the DLL -- in LOWERCASE!  It will be compared to
  // a lowercase version of the DLL name only.
  const char *name;

  // If maxVersion is ALL_VERSIONS, we'll block all versions of this
  // dll.  Otherwise, we'll block all versions less than or equal to
  // the given version, as queried by GetFileVersionInfo and
  // VS_FIXEDFILEINFO's dwFileVersionMS and dwFileVersionLS fields.
  //
  // Note that the version is usually 4 components, which is A.B.C.D
  // encoded as 0x AAAA BBBB CCCC DDDD ULL (spaces added for clarity),
  // but it's not required to be of that format.
  unsigned long long maxVersion;

  enum {
    FLAGS_DEFAULT = 0,
    BLOCK_WIN8PLUS_ONLY = 1
  } flags;
};

static DllBlockInfo sWindowsDllBlocklist[] = {
  // EXAMPLE:
  // { "uxtheme.dll", ALL_VERSIONS },
  // { "uxtheme.dll", 0x0000123400000000ULL },
  // The DLL name must be in lowercase!
  
  // NPFFAddon - Known malware
  { "npffaddon.dll", ALL_VERSIONS},

  // AVG 8 - Antivirus vendor AVG, old version, plugin already blocklisted
  {"avgrsstx.dll", MAKE_VERSION(8,5,0,401)},
  
  // calc.dll - Suspected malware
  {"calc.dll", MAKE_VERSION(1,0,0,1)},

  // hook.dll - Suspected malware
  {"hook.dll", ALL_VERSIONS},
  
  // GoogleDesktopNetwork3.dll - Extremely old, unversioned instances
  // of this DLL cause crashes
  {"googledesktopnetwork3.dll", UNVERSIONED},

  // rdolib.dll - Suspected malware
  {"rdolib.dll", MAKE_VERSION(6,0,88,4)},

  // fgjk4wvb.dll - Suspected malware
  {"fgjk4wvb.dll", MAKE_VERSION(8,8,8,8)},
  
  // radhslib.dll - Naomi internet filter - unmaintained since 2006
  {"radhslib.dll", UNVERSIONED},

  // Music download filter for vkontakte.ru - old instances
  // of this DLL cause crashes
  {"vksaver.dll", MAKE_VERSION(2,2,2,0)},

  // Topcrash in Firefox 4.0b1
  {"rlxf.dll", MAKE_VERSION(1,2,323,1)},

  // psicon.dll - Topcrashes in Thunderbird, and some crashes in Firefox
  // Adobe photoshop library, now redundant in later installations
  {"psicon.dll", ALL_VERSIONS},

  // Topcrash in Firefox 4 betas (bug 618899)
  {"accelerator.dll", MAKE_VERSION(3,2,1,6)},

  // Topcrash with Roboform in Firefox 8 (bug 699134)
  {"rf-firefox.dll", MAKE_VERSION(7,6,1,0)},
  {"roboform.dll", MAKE_VERSION(7,6,1,0)},

  // Topcrash with Babylon Toolbar on FF16+ (bug 721264)
  {"babyfox.dll", ALL_VERSIONS},

  {"sprotector.dll", ALL_VERSIONS, DllBlockInfo::BLOCK_WIN8PLUS_ONLY },

  // Topcrash with Websense Endpoint, bug 828184
  {"qipcap.dll", MAKE_VERSION(7, 6, 815, 1)},

  // leave these two in always for tests
  { "mozdllblockingtest.dll", ALL_VERSIONS },
  { "mozdllblockingtest_versioned.dll", 0x0000000400000000ULL },

  // Windows Media Foundation FLAC decoder and type sniffer (bug 839031).
  { "mfflac.dll", ALL_VERSIONS },

  // Older Relevant Knowledge DLLs cause us to crash (bug 904001).
  { "rlnx.dll", MAKE_VERSION(1, 3, 334, 9) },
  { "pmnx.dll", MAKE_VERSION(1, 3, 334, 9) },
  { "opnx.dll", MAKE_VERSION(1, 3, 334, 9) },
  { "prnx.dll", MAKE_VERSION(1, 3, 334, 9) },

  // Older belgian ID card software causes Firefox to crash or hang on
  // shutdown, bug 831285 and 918399.
  { "beid35cardlayer.dll", MAKE_VERSION(3, 5, 6, 6968) },

  { nullptr, 0 }
};

#ifndef STATUS_DLL_NOT_FOUND
#define STATUS_DLL_NOT_FOUND ((DWORD)0xC0000135L)
#endif

// define this for very verbose dll load debug spew
#undef DEBUG_very_verbose

extern bool gInXPCOMLoadOnMainThread;

namespace {

typedef NTSTATUS (NTAPI *LdrLoadDll_func) (PWCHAR filePath, PULONG flags, PUNICODE_STRING moduleFileName, PHANDLE handle);

static LdrLoadDll_func stub_LdrLoadDll = 0;

template <class T>
struct RVAMap {
  RVAMap(HANDLE map, DWORD offset) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);

    DWORD alignedOffset = (offset / info.dwAllocationGranularity) *
                          info.dwAllocationGranularity;

    NS_ASSERTION(offset - alignedOffset < info.dwAllocationGranularity, "Wtf");

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

bool
CheckASLR(const wchar_t* path)
{
  bool retval = false;

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
          // If the DLL has no code, permit it regardless of ASLR status.
          if (ntHeader->OptionalHeader.SizeOfCode == 0) {
            retval = true;
          }
          // Check to see if the DLL supports ASLR
          else if ((ntHeader->OptionalHeader.DllCharacteristics &
                    IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE) != 0) {
            retval = true;
          }
        }
      }
      ::CloseHandle(map);
    }
    ::CloseHandle(file);
  }

  return retval;
}

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
    EnterCriticalSection(&sLock);
    mPreviousDllName = (*sThreadMap)[currentThreadId];

    // If there is a DLL currently being loaded and it has the same name
    // as the current attempt, we're re-entering.
    mReentered = mPreviousDllName && !stricmp(mPreviousDllName, dllName);
    (*sThreadMap)[currentThreadId] = dllName;
    LeaveCriticalSection(&sLock);
  }
    
  ~ReentrancySentinel()
  {
    DWORD currentThreadId = GetCurrentThreadId();
    EnterCriticalSection(&sLock);
    (*sThreadMap)[currentThreadId] = mPreviousDllName;
    LeaveCriticalSection(&sLock);
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
  static CRITICAL_SECTION sLock;
  static std::map<DWORD, const char*>* sThreadMap;

  const char* mPreviousDllName;
  bool mReentered;
};

CRITICAL_SECTION ReentrancySentinel::sLock;
std::map<DWORD, const char*>* ReentrancySentinel::sThreadMap;

static
wchar_t* getFullPath (PWCHAR filePath, wchar_t* fname)
{
  // In Windows 8, the first parameter seems to be used for more than just the
  // path name.  For example, its numerical value can be 1.  Passing a non-valid
  // pointer to SearchPathW will cause a crash, so we need to check to see if we
  // are handed a valid pointer, and otherwise just pass nullptr to SearchPathW.
  PWCHAR sanitizedFilePath = (intptr_t(filePath) < 1024) ? nullptr : filePath;

  // figure out the length of the string that we need
  DWORD pathlen = SearchPathW(sanitizedFilePath, fname, L".dll", 0, nullptr,
                              nullptr);
  if (pathlen == 0) {
    return nullptr;
  }

  wchar_t* full_fname = new wchar_t[pathlen+1];
  if (!full_fname) {
    // couldn't allocate memory?
    return nullptr;
  }

  // now actually grab it
  SearchPathW(sanitizedFilePath, fname, L".dll", pathlen + 1, full_fname,
              nullptr);
  return full_fname;
}

static bool
IsWin8OrLater()
{
  OSVERSIONINFOW osInfo;
  osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
  GetVersionExW(&osInfo);
  return (osInfo.dwMajorVersion > 6) ||
    (osInfo.dwMajorVersion >= 6 && osInfo.dwMinorVersion >= 2);
}

static NTSTATUS NTAPI
patched_LdrLoadDll (PWCHAR filePath, PULONG flags, PUNICODE_STRING moduleFileName, PHANDLE handle)
{
  // We have UCS2 (UTF16?), we want ASCII, but we also just want the filename portion
#define DLLNAME_MAX 128
  char dllName[DLLNAME_MAX+1];
  wchar_t *dll_part;
  DllBlockInfo *info;

  int len = moduleFileName->Length / 2;
  wchar_t *fname = moduleFileName->Buffer;
  nsAutoArrayPtr<wchar_t> full_fname;

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

  dll_part = wcsrchr(fname, L'\\');
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

  // then compare to everything on the blocklist
  info = &sWindowsDllBlocklist[0];
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

    if ((info->flags == DllBlockInfo::BLOCK_WIN8PLUS_ONLY) &&
        !IsWin8OrLater()) {
      goto continue_loading;
    }

    if (info->maxVersion != ALL_VERSIONS) {
      ReentrancySentinel sentinel(dllName);
      if (sentinel.BailOut()) {
        goto continue_loading;
      }

      full_fname = getFullPath(filePath, fname);
      if (!full_fname) {
        // uh, we couldn't find the DLL at all, so...
        printf_stderr("LdrLoadDll: Blocking load of '%s' (SearchPathW didn't find it?)\n", dllName);
        return STATUS_DLL_NOT_FOUND;
      }

      DWORD zero;
      DWORD infoSize = GetFileVersionInfoSizeW(full_fname, &zero);

      // If we failed to get the version information, we block.

      if (infoSize != 0) {
        nsAutoArrayPtr<unsigned char> infoData(new unsigned char[infoSize]);
        VS_FIXEDFILEINFO *vInfo;
        UINT vInfoLen;

        if (GetFileVersionInfoW(full_fname, 0, infoSize, infoData) &&
            VerQueryValueW(infoData, L"\\", (LPVOID*) &vInfo, &vInfoLen))
        {
          unsigned long long fVersion =
            ((unsigned long long)vInfo->dwFileVersionMS) << 32 |
            ((unsigned long long)vInfo->dwFileVersionLS);

          // finally do the version check, and if it's greater than our block
          // version, keep loading
          if (fVersion > info->maxVersion)
            load_ok = true;
        }
      }
    }

    if (!load_ok) {
      printf_stderr("LdrLoadDll: Blocking load of '%s' -- see http://www.mozilla.com/en-US/blocklist/\n", dllName);
      return STATUS_DLL_NOT_FOUND;
    }
  }

continue_loading:
#ifdef DEBUG_very_verbose
  printf_stderr("LdrLoadDll: continuing load... ('%S')\n", moduleFileName->Buffer);
#endif

  if (gInXPCOMLoadOnMainThread && NS_IsMainThread()) {
    // Check to ensure that the DLL has ASLR.
    full_fname = getFullPath(filePath, fname);
    if (!full_fname) {
      // uh, we couldn't find the DLL at all, so...
      printf_stderr("LdrLoadDll: Blocking load of '%s' (SearchPathW didn't find it?)\n", dllName);
      return STATUS_DLL_NOT_FOUND;
    }

    if (IsVistaOrLater() && !CheckASLR(full_fname)) {
      printf_stderr("LdrLoadDll: Blocking load of '%s'.  XPCOM components must support ASLR.\n", dllName);
      return STATUS_DLL_NOT_FOUND;
    }
  }

  return stub_LdrLoadDll(filePath, flags, moduleFileName, handle);
}

WindowsDllInterceptor NtDllIntercept;

} // anonymous namespace

void
XRE_SetupDllBlocklist()
{
  NtDllIntercept.Init("ntdll.dll");

  ReentrancySentinel::InitializeStatics();

  bool ok = NtDllIntercept.AddHook("LdrLoadDll", reinterpret_cast<intptr_t>(patched_LdrLoadDll), (void**) &stub_LdrLoadDll);

#ifdef DEBUG
  if (!ok)
    printf_stderr ("LdrLoadDll hook failed, no dll blocklisting active\n");
#endif

#if defined(MOZ_CRASHREPORTER) && !defined(NO_BLOCKLIST_CRASHREPORTER)
  if (!ok) {
    CrashReporter::AppendAppNotesToCrashReport(NS_LITERAL_CSTRING("DllBlockList Failed\n"));
  }
#endif
}
