/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMacUtilsImpl.h"

#include "base/command_line.h"
#include "base/process_util.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Omnijar.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIFile.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "prenv.h"

#if defined(MOZ_SANDBOX)
#  include "mozilla/SandboxSettings.h"
#endif

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#if defined(__aarch64__)
#  include <dlfcn.h>
#endif
#include <sys/sysctl.h>

using mozilla::StaticMutexAutoLock;
using mozilla::Unused;

#if defined(MOZ_SANDBOX) || defined(__aarch64__)
// For thread safe setting/checking of sCachedAppPath
static StaticMutex sCachedAppPathMutex;

// Cache the appDir returned from GetAppPath to avoid doing I/O
static StaticAutoPtr<nsCString> sCachedAppPath
    MOZ_GUARDED_BY(sCachedAppPathMutex);
#endif

// The cached machine architectures of the .app bundle which can
// be multiple architectures for universal binaries.
static std::atomic<uint32_t> sBundleArchMaskAtomic = 0;

#if defined(__aarch64__)
// Limit XUL translation to one attempt
static std::atomic<bool> sIsXULTranslated = false;
#endif

// Info.plist key associated with the developer repo path
#define MAC_DEV_REPO_KEY "MozillaDeveloperRepoPath"
// Info.plist key associated with the developer repo object directory
#define MAC_DEV_OBJ_KEY "MozillaDeveloperObjPath"

// Workaround this constant not being available in the macOS SDK
#define kCFBundleExecutableArchitectureARM64 0x0100000c

enum TCSMStatus { TCSM_Unknown = 0, TCSM_Available, TCSM_Unavailable };

// Initialize with Unknown until we've checked if TCSM is available to set
static Atomic<TCSMStatus> sTCSMStatus(TCSM_Unknown);

#if defined(MOZ_SANDBOX) || defined(__aarch64__)

// Utility method to call ClearOnShutdown() on the main thread
static nsresult ClearCachedAppPathOnShutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  ClearOnShutdown(&sCachedAppPath);
  return NS_OK;
}

// Get the path to the .app directory (aka bundle) for the parent process.
// When executing in the child process, this is the outer .app (such as
// Firefox.app) and not the inner .app containing the child process
// executable. We don't rely on the actual .app extension to allow for the
// bundle being renamed.
bool nsMacUtilsImpl::GetAppPath(nsCString& aAppPath) {
  StaticMutexAutoLock lock(sCachedAppPathMutex);
  if (sCachedAppPath) {
    aAppPath.Assign(*sCachedAppPath);
    return true;
  }

  nsAutoCString appPath;
  nsAutoCString appBinaryPath(
      (CommandLine::ForCurrentProcess()->argv()[0]).c_str());

  // The binary path resides within the .app dir in Contents/MacOS,
  // e.g., Firefox.app/Contents/MacOS/firefox. Search backwards in
  // the binary path for the end of .app path.
  auto pattern = "/Contents/MacOS/"_ns;
  nsAutoCString::const_iterator start, end;
  appBinaryPath.BeginReading(start);
  appBinaryPath.EndReading(end);
  if (RFindInReadable(pattern, start, end)) {
    end = start;
    appBinaryPath.BeginReading(start);

    // If we're executing in a child process, get the parent .app path
    // by searching backwards once more. The child executable resides
    // in Firefox.app/Contents/MacOS/plugin-container/Contents/MacOS.
    if (!XRE_IsParentProcess()) {
      if (RFindInReadable(pattern, start, end)) {
        end = start;
        appBinaryPath.BeginReading(start);
      } else {
        return false;
      }
    }

    appPath.Assign(Substring(start, end));
  } else {
    return false;
  }

  nsCOMPtr<nsIFile> app;
  nsresult rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(appPath), true,
                                getter_AddRefs(app));
  if (NS_FAILED(rv)) {
    return false;
  }

  rv = app->Normalize();
  if (NS_FAILED(rv)) {
    return false;
  }
  app->GetNativePath(aAppPath);

  if (!sCachedAppPath) {
    sCachedAppPath = new nsCString(aAppPath);

    if (NS_IsMainThread()) {
      ClearCachedAppPathOnShutdown();
    } else {
      NS_DispatchToMainThread(
          NS_NewRunnableFunction("ClearCachedAppPathOnShutdown",
                                 [] { ClearCachedAppPathOnShutdown(); }));
    }
  }

  return true;
}

#endif /* MOZ_SANDBOX || __aarch64__ */

#if defined(MOZ_SANDBOX) && defined(DEBUG)
// If XPCOM_MEM_BLOAT_LOG or XPCOM_MEM_LEAK_LOG is set to a log file
// path, return the path to the parent directory (where sibling log
// files will be saved.)
nsresult nsMacUtilsImpl::GetBloatLogDir(nsCString& aDirectoryPath) {
  nsAutoCString bloatLog(PR_GetEnv("XPCOM_MEM_BLOAT_LOG"));
  if (bloatLog.IsEmpty()) {
    bloatLog = PR_GetEnv("XPCOM_MEM_LEAK_LOG");
  }
  if (!bloatLog.IsEmpty() && bloatLog != "1" && bloatLog != "2") {
    return GetDirectoryPath(bloatLog.get(), aDirectoryPath);
  }
  return NS_OK;
}

// Given a path to a file, return the directory which contains it.
nsresult nsMacUtilsImpl::GetDirectoryPath(const char* aPath,
                                          nsCString& aDirectoryPath) {
  nsresult rv = NS_OK;
  nsCOMPtr<nsIFile> file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = file->InitWithNativePath(nsDependentCString(aPath));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> directoryFile;
  rv = file->GetParent(getter_AddRefs(directoryFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = directoryFile->Normalize();
  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_FAILED(directoryFile->GetNativePath(aDirectoryPath))) {
    MOZ_CRASH("Failed to get path for an nsIFile");
  }
  return NS_OK;
}
#endif /* MOZ_SANDBOX  && DEBUG */

/* static */
bool nsMacUtilsImpl::IsTCSMAvailable() {
  if (sTCSMStatus == TCSM_Unknown) {
    uint32_t oldVal = 0;
    size_t oldValSize = sizeof(oldVal);
    int rv = sysctlbyname("kern.tcsm_available", &oldVal, &oldValSize, NULL, 0);
    TCSMStatus newStatus;
    if (rv < 0 || oldVal == 0) {
      newStatus = TCSM_Unavailable;
    } else {
      newStatus = TCSM_Available;
    }
    // The value of sysctl kern.tcsm_available is the same for all
    // threads within the same process. If another thread raced with us
    // and initialized sTCSMStatus first (changing it from
    // TCSM_Unknown), we can continue without needing to update it
    // again. Hence, we ignore compareExchange's return value.
    Unused << sTCSMStatus.compareExchange(TCSM_Unknown, newStatus);
  }
  return (sTCSMStatus == TCSM_Available);
}

static nsresult EnableTCSM() {
  uint32_t newVal = 1;
  int rv = sysctlbyname("kern.tcsm_enable", NULL, 0, &newVal, sizeof(newVal));
  if (rv < 0) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

#if defined(DEBUG)
static bool IsTCSMEnabled() {
  uint32_t oldVal = 0;
  size_t oldValSize = sizeof(oldVal);
  int rv = sysctlbyname("kern.tcsm_enable", &oldVal, &oldValSize, NULL, 0);
  return (rv == 0) && (oldVal != 0);
}
#endif

/*
 * Intentionally return void so that failures will be ignored in non-debug
 * builds. This method uses new sysctls which may not be as thoroughly tested
 * and we don't want to cause crashes handling the failure due to an OS bug.
 */
/* static */
void nsMacUtilsImpl::EnableTCSMIfAvailable() {
  if (IsTCSMAvailable()) {
    if (NS_FAILED(EnableTCSM())) {
      NS_WARNING("Failed to enable TCSM");
    }
    MOZ_ASSERT(IsTCSMEnabled());
  }
}

// Returns 0 on error.
/* static */
uint32_t nsMacUtilsImpl::GetPhysicalCPUCount() {
  uint32_t oldVal = 0;
  size_t oldValSize = sizeof(oldVal);
  int rv = sysctlbyname("hw.physicalcpu_max", &oldVal, &oldValSize, NULL, 0);
  if (rv == -1) {
    return 0;
  }
  return oldVal;
}

/*
 * Helper function to read a string value for a given key from the .app's
 * Info.plist.
 */
static nsresult GetStringValueFromBundlePlist(const nsAString& aKey,
                                              nsAutoCString& aValue) {
  CFBundleRef mainBundle = CFBundleGetMainBundle();
  if (mainBundle == nullptr) {
    return NS_ERROR_FAILURE;
  }

  // Read this app's bundle Info.plist as a dictionary
  CFDictionaryRef bundleInfoDict = CFBundleGetInfoDictionary(mainBundle);
  if (bundleInfoDict == nullptr) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString keyAutoCString = NS_ConvertUTF16toUTF8(aKey);
  CFStringRef key = CFStringCreateWithCString(
      kCFAllocatorDefault, keyAutoCString.get(), kCFStringEncodingUTF8);
  if (key == nullptr) {
    return NS_ERROR_FAILURE;
  }

  CFStringRef value = (CFStringRef)CFDictionaryGetValue(bundleInfoDict, key);
  CFRelease(key);
  if (value == nullptr) {
    return NS_ERROR_FAILURE;
  }

  CFIndex valueLength = CFStringGetLength(value);
  if (valueLength == 0) {
    return NS_ERROR_FAILURE;
  }

  const char* valueCString =
      CFStringGetCStringPtr(value, kCFStringEncodingUTF8);
  if (valueCString) {
    aValue.Assign(valueCString);
    return NS_OK;
  }

  CFIndex maxLength =
      CFStringGetMaximumSizeForEncoding(valueLength, kCFStringEncodingUTF8) + 1;
  char* valueBuffer = static_cast<char*>(moz_xmalloc(maxLength));

  if (!CFStringGetCString(value, valueBuffer, maxLength,
                          kCFStringEncodingUTF8)) {
    free(valueBuffer);
    return NS_ERROR_FAILURE;
  }

  aValue.Assign(valueBuffer);
  free(valueBuffer);
  return NS_OK;
}

/*
 * Helper function for reading a path string from the .app's Info.plist
 * and returning a directory object for that path with symlinks resolved.
 */
static nsresult GetDirFromBundlePlist(const nsAString& aKey, nsIFile** aDir) {
  nsresult rv;

  nsAutoCString dirPath;
  rv = GetStringValueFromBundlePlist(aKey, dirPath);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> dir;
  rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(dirPath), false,
                       getter_AddRefs(dir));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dir->Normalize();
  NS_ENSURE_SUCCESS(rv, rv);

  bool isDirectory = false;
  rv = dir->IsDirectory(&isDirectory);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isDirectory) {
    return NS_ERROR_FILE_NOT_DIRECTORY;
  }

  dir.swap(*aDir);
  return NS_OK;
}

nsresult nsMacUtilsImpl::GetRepoDir(nsIFile** aRepoDir) {
#if defined(MOZ_SANDBOX)
  MOZ_ASSERT(!mozilla::IsPackagedBuild());
#endif
  return GetDirFromBundlePlist(NS_LITERAL_STRING_FROM_CSTRING(MAC_DEV_REPO_KEY),
                               aRepoDir);
}

nsresult nsMacUtilsImpl::GetObjDir(nsIFile** aObjDir) {
#if defined(MOZ_SANDBOX)
  MOZ_ASSERT(!mozilla::IsPackagedBuild());
#endif
  return GetDirFromBundlePlist(NS_LITERAL_STRING_FROM_CSTRING(MAC_DEV_OBJ_KEY),
                               aObjDir);
}

/* static */
nsresult nsMacUtilsImpl::GetArchitecturesForBundle(uint32_t* aArchMask) {
  MOZ_ASSERT(aArchMask);

  *aArchMask = sBundleArchMaskAtomic;
  if (*aArchMask != 0) {
    return NS_OK;
  }

  CFBundleRef mainBundle = ::CFBundleGetMainBundle();
  if (!mainBundle) {
    return NS_ERROR_FAILURE;
  }

  CFArrayRef archList = ::CFBundleCopyExecutableArchitectures(mainBundle);
  if (!archList) {
    return NS_ERROR_FAILURE;
  }

  CFIndex archCount = ::CFArrayGetCount(archList);
  for (CFIndex i = 0; i < archCount; i++) {
    CFNumberRef arch =
        static_cast<CFNumberRef>(::CFArrayGetValueAtIndex(archList, i));

    int archInt = 0;
    if (!::CFNumberGetValue(arch, kCFNumberIntType, &archInt)) {
      ::CFRelease(archList);
      return NS_ERROR_FAILURE;
    }

    if (archInt == kCFBundleExecutableArchitecturePPC) {
      *aArchMask |= base::PROCESS_ARCH_PPC;
    } else if (archInt == kCFBundleExecutableArchitectureI386) {
      *aArchMask |= base::PROCESS_ARCH_I386;
    } else if (archInt == kCFBundleExecutableArchitecturePPC64) {
      *aArchMask |= base::PROCESS_ARCH_PPC_64;
    } else if (archInt == kCFBundleExecutableArchitectureX86_64) {
      *aArchMask |= base::PROCESS_ARCH_X86_64;
    } else if (archInt == kCFBundleExecutableArchitectureARM64) {
      *aArchMask |= base::PROCESS_ARCH_ARM_64;
    }
  }

  ::CFRelease(archList);

  sBundleArchMaskAtomic = *aArchMask;

  return NS_OK;
}

/* static */
nsresult nsMacUtilsImpl::GetArchitecturesForBinary(const char* aPath,
                                                   uint32_t* aArchMask) {
  MOZ_ASSERT(aArchMask);

  *aArchMask = 0;

  CFURLRef url = ::CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, (const UInt8*)aPath, strlen(aPath), false);
  if (!url) {
    return NS_ERROR_FAILURE;
  }

  CFArrayRef archs = ::CFBundleCopyExecutableArchitecturesForURL(url);
  if (!archs) {
    CFRelease(url);
    return NS_ERROR_FAILURE;
  }

  CFIndex archCount = ::CFArrayGetCount(archs);
  for (CFIndex i = 0; i < archCount; i++) {
    CFNumberRef currentArch =
        static_cast<CFNumberRef>(::CFArrayGetValueAtIndex(archs, i));
    int currentArchInt = 0;
    if (!::CFNumberGetValue(currentArch, kCFNumberIntType, &currentArchInt)) {
      continue;
    }
    switch (currentArchInt) {
      case kCFBundleExecutableArchitectureX86_64:
        *aArchMask |= base::PROCESS_ARCH_X86_64;
        break;
      case kCFBundleExecutableArchitectureARM64:
        *aArchMask |= base::PROCESS_ARCH_ARM_64;
        break;
      default:
        break;
    }
  }

  CFRelease(url);
  CFRelease(archs);

  // We expect x86 or ARM64 or both.
  if (*aArchMask == 0) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

#if defined(__aarch64__)
// Pre-translate XUL so that x64 child processes launched after this
// translation will not incur the translation overhead delaying startup.
// Returns 1 if translation is in progress, -1 on an error encountered before
// translation, and otherwise returns the result of rosetta_translate_binaries.
/* static */
int nsMacUtilsImpl::PreTranslateXUL() {
  bool expected = false;
  if (!sIsXULTranslated.compare_exchange_strong(expected, true)) {
    // Translation is already done or in progress.
    return 1;
  }

  // Get the path to XUL by first getting the
  // outer .app path and appending the path to XUL.
  nsCString xulPath;
  if (!GetAppPath(xulPath)) {
    return -1;
  }
  xulPath.Append("/Contents/MacOS/XUL");

  return PreTranslateBinary(xulPath);
}

// Use Chromium's method to pre-translate the provided binary using the
// undocumented function "rosetta_translate_binaries" from libRosetta.dylib.
// Re-translating the same binary does not cause translation to occur again.
// Returns -1 on an error encountered before translation, otherwise returns
// the rosetta_translate_binaries result. This method is partly copied from
// Chromium code.
/* static */
int nsMacUtilsImpl::PreTranslateBinary(nsCString aBinaryPath) {
  // Do not attempt to use this in child processes. Child
  // processes executing should already be translated and
  // sandboxing may interfere with translation.
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!XRE_IsParentProcess()) {
    return -1;
  }

  // Translation can take several seconds and therefore
  // should not be done on the main thread.
  MOZ_ASSERT(!NS_IsMainThread());
  if (NS_IsMainThread()) {
    return -1;
  }

  // @available() is not available for macOS 11 at this time so use
  // -Wunguarded-availability-new to avoid compiler warnings caused
  // by an earlier minimum SDK. ARM64 builds require the 11.0 SDK and
  // can not be run on earlier OS versions so this is not a concern.
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunguarded-availability-new"
  // If Rosetta is not installed, do not proceed.
  if (!CFBundleIsArchitectureLoadable(CPU_TYPE_X86_64)) {
    return -1;
  }
#  pragma clang diagnostic pop

  if (aBinaryPath.IsEmpty()) {
    return -1;
  }

  // int rosetta_translate_binaries(const char*[] paths, int npaths)
  using rosetta_translate_binaries_t = int (*)(const char*[], int);

  static auto rosetta_translate_binaries = []() {
    void* libRosetta =
        dlopen("/usr/lib/libRosetta.dylib", RTLD_LAZY | RTLD_LOCAL);
    if (!libRosetta) {
      return static_cast<rosetta_translate_binaries_t>(nullptr);
    }

    return reinterpret_cast<rosetta_translate_binaries_t>(
        dlsym(libRosetta, "rosetta_translate_binaries"));
  }();

  if (!rosetta_translate_binaries) {
    return -1;
  }

  const char* pathPtr = aBinaryPath.get();
  return rosetta_translate_binaries(&pathPtr, 1);
}

#endif
