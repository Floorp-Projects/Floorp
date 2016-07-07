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
  extern "C" MOZ_MEMORY_API return_type name ## _impl(__VA_ARGS__);
#include "malloc_decls.h"
#endif

#include <windows.h>
#include <winternl.h>
#include <io.h>

#pragma warning( push )
#pragma warning( disable : 4275 4530 ) // See msvc-stl-wrapper.template.h
#include <map>
#pragma warning( pop )

#include "nsAutoPtr.h"

#include "nsWindowsDllInterceptor.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsVersion.h"
#include "nsWindowsHelpers.h"
#include "WindowsDllBlocklist.h"

using namespace mozilla;

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
  //
  // If the USE_TIMESTAMP flag is set, then we use the timestamp from
  // the IMAGE_FILE_HEADER in lieu of a version number.
  unsigned long long maxVersion;

  enum {
    FLAGS_DEFAULT = 0,
    BLOCK_WIN8PLUS_ONLY = 1,
    BLOCK_XP_ONLY = 2,
    USE_TIMESTAMP = 4,
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

  // sprotector.dll crashes, bug 957258
  {"sprotector.dll", ALL_VERSIONS},

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

  // bug 925459, bitguard crashes
  { "bitguard.dll", ALL_VERSIONS },

  // bug 812683 - crashes in Windows library when Asus Gamer OSD is installed
  // Software is discontinued/unsupported
  { "atkdx11disp.dll", ALL_VERSIONS },

  // Topcrash with Conduit SearchProtect, bug 944542
  { "spvc32.dll", ALL_VERSIONS },

  // XP topcrash with F-Secure, bug 970362
  { "fs_ccf_ni_umh32.dll", MAKE_VERSION(1, 42, 101, 0), DllBlockInfo::BLOCK_XP_ONLY },

  // Topcrash with V-bates, bug 1002748 and bug 1023239
  { "libinject.dll", UNVERSIONED },
  { "libinject2.dll", 0x537DDC93, DllBlockInfo::USE_TIMESTAMP },
  { "libredir2.dll", 0x5385B7ED, DllBlockInfo::USE_TIMESTAMP },

  // Crashes with RoboForm2Go written against old SDK, bug 988311/1196859
  { "rf-firefox-22.dll", ALL_VERSIONS },
  { "rf-firefox-40.dll", ALL_VERSIONS },

  // Crashes with DesktopTemperature, bug 1046382
  { "dtwxsvc.dll", 0x53153234, DllBlockInfo::USE_TIMESTAMP },

  // Startup crashes with Lenovo Onekey Theater, bug 1123778
  { "activedetect32.dll", UNVERSIONED },
  { "activedetect64.dll", UNVERSIONED },
  { "windowsapihookdll32.dll", UNVERSIONED },
  { "windowsapihookdll64.dll", UNVERSIONED },

  // Flash crashes with RealNetworks RealDownloader, bug 1132663
  { "rndlnpshimswf.dll", ALL_VERSIONS },
  { "rndlmainbrowserrecordplugin.dll", ALL_VERSIONS },

  // Startup crashes with RealNetworks Browser Record Plugin, bug 1170141
  { "nprpffbrowserrecordext.dll", ALL_VERSIONS },
  { "nprndlffbrowserrecordext.dll", ALL_VERSIONS },

  // Crashes with CyberLink YouCam, bug 1136968
  { "ycwebcamerasource.ax", MAKE_VERSION(2, 0, 0, 1611) },

  // Old version of WebcamMax crashes WebRTC, bug 1130061
  { "vwcsource.ax", MAKE_VERSION(1, 5, 0, 0) },

  // NetOp School, discontinued product, bug 763395
  { "nlsp.dll", MAKE_VERSION(6, 23, 2012, 19) },

  // Orbit Downloader, bug 1222819
  { "grabdll.dll", MAKE_VERSION(2, 6, 1, 0) },
  { "grabkernel.dll", MAKE_VERSION(1, 0, 0, 1) },

  // ESET, bug 1229252
  { "eoppmonitor.dll", ALL_VERSIONS },

  // SS2OSD, bug 1262348
  { "ss2osd.dll", ALL_VERSIONS },
  { "ss2devprops.dll", ALL_VERSIONS },

  // NHASUSSTRIXOSD.DLL, bug 1269244
  { "nhasusstrixosd.dll", ALL_VERSIONS },
  { "nhasusstrixdevprops.dll", ALL_VERSIONS },

  // Crashes with PremierOpinion/RelevantKnowledge, bug 1277846
  { "opls.dll", ALL_VERSIONS },
  { "opls64.dll", ALL_VERSIONS },
  { "pmls.dll", ALL_VERSIONS },
  { "pmls64.dll", ALL_VERSIONS },
  { "prls.dll", ALL_VERSIONS },
  { "prls64.dll", ALL_VERSIONS },
  { "rlls.dll", ALL_VERSIONS },
  { "rlls64.dll", ALL_VERSIONS },

  { nullptr, 0 }
};

#ifndef STATUS_DLL_NOT_FOUND
#define STATUS_DLL_NOT_FOUND ((DWORD)0xC0000135L)
#endif

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

static DWORD sThreadLoadingXPCOMModule;
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
    vsnprintf(buf, sizeof(buf), fmt, args);
    buf[sizeof(buf) - 1] = '\0';
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

DWORD
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

  const char* mName; // points into the sWindowsDllBlocklist string
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
      if (b->mVersion != -1) {
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

static NTSTATUS NTAPI
patched_LdrLoadDll (PWCHAR filePath, PULONG flags, PUNICODE_STRING moduleFileName, PHANDLE handle)
{
  // We have UCS2 (UTF16?), we want ASCII, but we also just want the filename portion
#define DLLNAME_MAX 128
  char dllName[DLLNAME_MAX+1];
  wchar_t *dll_part;
  char *dot;
  DllBlockInfo *info;

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

  // Block a suspicious binary that uses various 12-digit hex strings
  // e.g. MovieMode.48CA2AEFA22D.dll (bug 973138)
  dot = strchr(dllName, '.');
  if (dot && (strchr(dot+1, '.') == dot+13)) {
    char * end = nullptr;
    _strtoui64(dot+1, &end, 16);
    if (end == dot+13) {
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
      return STATUS_DLL_NOT_FOUND;
    }
  }

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

    if ((info->flags == DllBlockInfo::BLOCK_XP_ONLY) &&
        IsWin2003OrLater()) {
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
      return STATUS_DLL_NOT_FOUND;
    }
  }

continue_loading:
#ifdef DEBUG_very_verbose
  printf_stderr("LdrLoadDll: continuing load... ('%S')\n", moduleFileName->Buffer);
#endif

  if (GetCurrentThreadId() == sThreadLoadingXPCOMModule) {
    // Check to ensure that the DLL has ASLR.
    full_fname = getFullPath(filePath, fname);
    if (!full_fname) {
      // uh, we couldn't find the DLL at all, so...
      printf_stderr("LdrLoadDll: Blocking load of '%s' (SearchPathW didn't find it?)\n", dllName);
      return STATUS_DLL_NOT_FOUND;
    }

    if (IsVistaOrLater() && !CheckASLR(full_fname.get())) {
      printf_stderr("LdrLoadDll: Blocking load of '%s'.  XPCOM components must support ASLR.\n", dllName);
      return STATUS_DLL_NOT_FOUND;
    }
  }

  return stub_LdrLoadDll(filePath, flags, moduleFileName, handle);
}

WindowsDllInterceptor NtDllIntercept;

} // namespace

MFBT_API void
DllBlocklist_Initialize()
{
  if (sBlocklistInitAttempted) {
    return;
  }
  sBlocklistInitAttempted = true;

  if (GetModuleHandleA("user32.dll")) {
    sUser32BeforeBlocklist = true;
  }
  // Catch any missing DELAYLOADS for user32.dll
  MOZ_ASSERT(!sUser32BeforeBlocklist);

  NtDllIntercept.Init("ntdll.dll");

  ReentrancySentinel::InitializeStatics();

  // We specifically use a detour, because there are cases where external
  // code also tries to hook LdrLoadDll, and doesn't know how to relocate our
  // nop space patches. (Bug 951827)
  bool ok = NtDllIntercept.AddDetour("LdrLoadDll", reinterpret_cast<intptr_t>(patched_LdrLoadDll), (void**) &stub_LdrLoadDll);

  if (!ok) {
    sBlocklistInitFailed = true;
#ifdef DEBUG
    printf_stderr ("LdrLoadDll hook failed, no dll blocklisting active\n");
#endif
  }
}

MFBT_API void
DllBlocklist_SetInXPCOMLoadOnMainThread(bool inXPCOMLoadOnMainThread)
{
  if (inXPCOMLoadOnMainThread) {
    MOZ_ASSERT(sThreadLoadingXPCOMModule == 0, "Only one thread should be doing this");
    sThreadLoadingXPCOMModule = GetCurrentThreadId();
  } else {
    sThreadLoadingXPCOMModule = 0;
  }
}

MFBT_API void
DllBlocklist_WriteNotes(HANDLE file)
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
