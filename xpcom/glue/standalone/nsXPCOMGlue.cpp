/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Bootstrap.h"

#include "nspr.h"
#include "nsDebug.h"
#include "nsIServiceManager.h"
#include "nsXPCOMPrivate.h"
#include "nsCOMPtr.h"
#include <stdlib.h>
#include <stdio.h>

#include "mozilla/FileUtils.h"
#include "mozilla/Sprintf.h"
#include "mozilla/UniquePtrExtensions.h"

using namespace mozilla;

#define XPCOM_DEPENDENT_LIBS_LIST "dependentlibs.list"

#if defined(XP_WIN)
#  define READ_TEXTMODE L"rt"
#else
#  define READ_TEXTMODE "r"
#endif

typedef void (*NSFuncPtr)();

#if defined(XP_WIN)
#  include <windows.h>
#  include <mbstring.h>
#  include "mozilla/WindowsVersion.h"

typedef HINSTANCE LibHandleType;

static LibHandleType GetLibHandle(pathstr_t aDependentLib) {
  LibHandleType libHandle =
      LoadLibraryExW(aDependentLib, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);

#  ifdef DEBUG
  if (!libHandle) {
    DWORD err = GetLastError();
    LPWSTR lpMsgBuf;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPWSTR)&lpMsgBuf, 0, nullptr);
    wprintf(L"Error loading %ls: %s\n", aDependentLib, lpMsgBuf);
    LocalFree(lpMsgBuf);
  }
#  endif

  return libHandle;
}

static NSFuncPtr GetSymbol(LibHandleType aLibHandle, const char* aSymbol) {
  return (NSFuncPtr)GetProcAddress(aLibHandle, aSymbol);
}

static void CloseLibHandle(LibHandleType aLibHandle) {
  FreeLibrary(aLibHandle);
}

#else
#  include <dlfcn.h>

#  if defined(MOZ_LINKER)
extern "C" {
NS_HIDDEN __typeof(dlopen) __wrap_dlopen;
NS_HIDDEN __typeof(dlsym) __wrap_dlsym;
NS_HIDDEN __typeof(dlclose) __wrap_dlclose;
}

#    define dlopen __wrap_dlopen
#    define dlsym __wrap_dlsym
#    define dlclose __wrap_dlclose
#  endif

typedef void* LibHandleType;

static LibHandleType GetLibHandle(pathstr_t aDependentLib) {
  LibHandleType libHandle = dlopen(aDependentLib, RTLD_GLOBAL | RTLD_LAZY
#  ifdef XP_MACOSX
                                                      | RTLD_FIRST
#  endif
  );
  if (!libHandle) {
    fprintf(stderr, "XPCOMGlueLoad error for file %s:\n%s\n", aDependentLib,
            dlerror());
  }
  return libHandle;
}

static NSFuncPtr GetSymbol(LibHandleType aLibHandle, const char* aSymbol) {
  return (NSFuncPtr)dlsym(aLibHandle, aSymbol);
}

#  ifndef MOZ_LINKER
static void CloseLibHandle(LibHandleType aLibHandle) { dlclose(aLibHandle); }
#  endif
#endif

struct DependentLib {
  LibHandleType libHandle;
  DependentLib* next;
};

static DependentLib* sTop;

static void AppendDependentLib(LibHandleType aLibHandle) {
  auto* d = new DependentLib;
  if (!d) {
    return;
  }

  d->next = sTop;
  d->libHandle = aLibHandle;

  sTop = d;
}

static bool ReadDependentCB(pathstr_t aDependentLib,
                            LibLoadingStrategy aLibLoadingStrategy) {
#ifndef MOZ_LINKER
  // Don't bother doing a ReadAhead if we're not in the parent process.
  // What we need from the library should already be in the system file
  // cache.
  if (aLibLoadingStrategy == LibLoadingStrategy::ReadAhead) {
    ReadAheadLib(aDependentLib);
  }
#endif
  LibHandleType libHandle = GetLibHandle(aDependentLib);
  if (libHandle) {
    AppendDependentLib(libHandle);
  }

  return libHandle;
}

#ifdef XP_WIN
static bool ReadDependentCB(const char* aDependentLib,
                            LibLoadingStrategy aLibLoadingStrategy) {
  wchar_t wideDependentLib[MAX_PATH];
  MultiByteToWideChar(CP_UTF8, 0, aDependentLib, -1, wideDependentLib,
                      MAX_PATH);
  return ReadDependentCB(wideDependentLib, aLibLoadingStrategy);
}

inline FILE* TS_tfopen(const char* path, const wchar_t* mode) {
  wchar_t wPath[MAX_PATH];
  MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, MAX_PATH);
  return _wfopen(wPath, mode);
}
#else
inline FILE* TS_tfopen(const char* aPath, const char* aMode) {
  return fopen(aPath, aMode);
}
#endif

/* RAII wrapper for FILE descriptors */
struct ScopedCloseFileTraits {
  typedef FILE* type;
  static type empty() { return nullptr; }
  static void release(type aFile) {
    if (aFile) {
      fclose(aFile);
    }
  }
};
typedef Scoped<ScopedCloseFileTraits> ScopedCloseFile;

#ifndef MOZ_LINKER
static void XPCOMGlueUnload() {
  while (sTop) {
    CloseLibHandle(sTop->libHandle);

    DependentLib* temp = sTop;
    sTop = sTop->next;

    delete temp;
  }
}
#endif

#if defined(XP_WIN)
// like strpbrk but finds the *last* char, not the first
static const char* ns_strrpbrk(const char* string, const char* strCharSet) {
  const char* found = nullptr;
  for (; *string; ++string) {
    for (const char* search = strCharSet; *search; ++search) {
      if (*search == *string) {
        found = string;
        // Since we're looking for the last char, we save "found"
        // until we're at the end of the string.
      }
    }
  }

  return found;
}
#endif

static nsresult XPCOMGlueLoad(const char* aXPCOMFile,
                              LibLoadingStrategy aLibLoadingStrategy) {
#ifdef MOZ_LINKER
  if (!ReadDependentCB(aXPCOMFile, aLibLoadingStrategy)) {
    return NS_ERROR_FAILURE;
  }
#else
  char xpcomDir[MAXPATHLEN];
#  ifdef XP_WIN
  const char* lastSlash = ns_strrpbrk(aXPCOMFile, "/\\");
#  elif XP_MACOSX
  // On OSX, the dependentlibs.list file lives under Contents/Resources.
  // However, the actual libraries listed in dependentlibs.list live under
  // Contents/MacOS. We want to read the list from Contents/Resources, then
  // load the libraries from Contents/MacOS.
  const char* tempSlash = strrchr(aXPCOMFile, '/');
  size_t tempLen = size_t(tempSlash - aXPCOMFile);
  if (tempLen > MAXPATHLEN) {
    return NS_ERROR_FAILURE;
  }
  char tempBuffer[MAXPATHLEN];
  memcpy(tempBuffer, aXPCOMFile, tempLen);
  tempBuffer[tempLen] = '\0';
  const char* slash = strrchr(tempBuffer, '/');
  tempLen = size_t(slash - tempBuffer);
  const char* lastSlash = aXPCOMFile + tempLen;
#  else
  const char* lastSlash = strrchr(aXPCOMFile, '/');
#  endif
  char* cursor;
  if (lastSlash) {
    size_t len = size_t(lastSlash - aXPCOMFile);

    if (len > MAXPATHLEN - sizeof(XPCOM_FILE_PATH_SEPARATOR
#  ifdef XP_MACOSX
                                  "Resources" XPCOM_FILE_PATH_SEPARATOR
#  endif
                                      XPCOM_DEPENDENT_LIBS_LIST)) {
      return NS_ERROR_FAILURE;
    }
    memcpy(xpcomDir, aXPCOMFile, len);
    strcpy(xpcomDir + len, XPCOM_FILE_PATH_SEPARATOR
#  ifdef XP_MACOSX
           "Resources" XPCOM_FILE_PATH_SEPARATOR
#  endif
               XPCOM_DEPENDENT_LIBS_LIST);
    cursor = xpcomDir + len + 1;
  } else {
    strcpy(xpcomDir, XPCOM_DEPENDENT_LIBS_LIST);
    cursor = xpcomDir;
  }

  if (getenv("MOZ_RUN_GTEST")) {
    strcat(xpcomDir, ".gtest");
  }

  ScopedCloseFile flist;
  flist = TS_tfopen(xpcomDir, READ_TEXTMODE);
  if (!flist) {
    return NS_ERROR_FAILURE;
  }

#  ifdef XP_MACOSX
  tempLen = size_t(cursor - xpcomDir);
  if (tempLen > MAXPATHLEN - sizeof("MacOS" XPCOM_FILE_PATH_SEPARATOR) - 1) {
    return NS_ERROR_FAILURE;
  }
  strcpy(cursor, "MacOS" XPCOM_FILE_PATH_SEPARATOR);
  cursor += strlen(cursor);
#  endif
  *cursor = '\0';

  char buffer[MAXPATHLEN];

  while (fgets(buffer, sizeof(buffer), flist)) {
    int l = strlen(buffer);

    // ignore empty lines and comments
    if (l == 0 || *buffer == '#') {
      continue;
    }
#  ifdef XP_WIN
    // There is no point in reading Universal CRT forwarder DLLs ahead on
    // Windows 10 because they will not be touched later.
    if (IsWin10OrLater() && !strncmp(buffer, "api-", 4)) {
      continue;
    }
#  endif

    // cut the trailing newline, if present
    if (buffer[l - 1] == '\n') {
      buffer[l - 1] = '\0';
    }

    if (l + size_t(cursor - xpcomDir) > MAXPATHLEN) {
      return NS_ERROR_FAILURE;
    }

    strcpy(cursor, buffer);
    if (!ReadDependentCB(xpcomDir, aLibLoadingStrategy)) {
      XPCOMGlueUnload();
      return NS_ERROR_FAILURE;
    }
  }
#endif
  return NS_OK;
}

#if defined(MOZ_WIDGET_GTK) && \
    (defined(MOZ_MEMORY) || defined(__FreeBSD__) || defined(__NetBSD__))
#  define MOZ_GSLICE_INIT
#endif

#ifdef MOZ_GSLICE_INIT
#  include <glib.h>

class GSliceInit {
 public:
  GSliceInit() {
    mHadGSlice = bool(getenv("G_SLICE"));
    if (!mHadGSlice) {
      // Disable the slice allocator, since jemalloc already uses similar layout
      // algorithms, and using a sub-allocator tends to increase fragmentation.
      // This must be done before g_thread_init() is called.
      // glib >= 2.36 initializes g_slice as a side effect of its various static
      // initializers, so this needs to happen before glib is loaded, which is
      // this is hooked in XPCOMGlueStartup before libxul is loaded. This
      // relies on the main executable not depending on glib.
      setenv("G_SLICE", "always-malloc", 1);
    }
  }

  ~GSliceInit() {
    if (!mHadGSlice) {
      unsetenv("G_SLICE");
    }
  }

 private:
  bool mHadGSlice;
};
#endif

namespace mozilla {

Bootstrap::UniquePtr GetBootstrap(const char* aXPCOMFile,
                                  LibLoadingStrategy aLibLoadingStrategy) {
#ifdef MOZ_GSLICE_INIT
  GSliceInit gSliceInit;
#endif

  if (!aXPCOMFile) {
    return nullptr;
  }

  char* lastSlash =
      strrchr(const_cast<char*>(aXPCOMFile), XPCOM_FILE_PATH_SEPARATOR[0]);
  if (!lastSlash) {
    return nullptr;
  }
  size_t base_len = size_t(lastSlash - aXPCOMFile) + 1;

  UniqueFreePtr<char> file(
      reinterpret_cast<char*>(malloc(base_len + sizeof(XPCOM_DLL))));
  memcpy(file.get(), aXPCOMFile, base_len);
  memcpy(file.get() + base_len, XPCOM_DLL, sizeof(XPCOM_DLL));

  if (NS_FAILED(XPCOMGlueLoad(file.get(), aLibLoadingStrategy))) {
    return nullptr;
  }

  GetBootstrapType func =
      (GetBootstrapType)GetSymbol(sTop->libHandle, "XRE_GetBootstrap");
  if (!func) {
    return nullptr;
  }

  Bootstrap::UniquePtr b;
  (*func)(b);

  return b;
}

}  // namespace mozilla
