/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMacUtilsImpl.h"

#include "base/command_line.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsIProperties.h"
#include "nsServiceManagerUtils.h"
#include "nsXULAppAPI.h"
#include "prenv.h"

#include <CoreFoundation/CoreFoundation.h>
#include <sys/sysctl.h>

NS_IMPL_ISUPPORTS(nsMacUtilsImpl, nsIMacUtils)

using mozilla::StaticMutexAutoLock;
using mozilla::Unused;

#if defined(MOZ_SANDBOX)
StaticAutoPtr<nsCString> nsMacUtilsImpl::sCachedAppPath;
StaticMutex nsMacUtilsImpl::sCachedAppPathMutex;
#endif

// Initialize with Unknown until we've checked if TCSM is available to set
Atomic<nsMacUtilsImpl::TCSMStatus> nsMacUtilsImpl::sTCSMStatus(TCSM_Unknown);

nsresult nsMacUtilsImpl::GetArchString(nsAString& aArchString) {
  if (!mBinaryArchs.IsEmpty()) {
    aArchString.Assign(mBinaryArchs);
    return NS_OK;
  }

  aArchString.Truncate();

  bool foundPPC = false, foundX86 = false, foundPPC64 = false,
       foundX86_64 = false;

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
      foundPPC = true;
    } else if (archInt == kCFBundleExecutableArchitectureI386) {
      foundX86 = true;
    } else if (archInt == kCFBundleExecutableArchitecturePPC64) {
      foundPPC64 = true;
    } else if (archInt == kCFBundleExecutableArchitectureX86_64) {
      foundX86_64 = true;
    }
  }

  ::CFRelease(archList);

  // The order in the string must always be the same so
  // don't do this in the loop.
  if (foundPPC) {
    mBinaryArchs.AppendLiteral("ppc");
  }

  if (foundX86) {
    if (!mBinaryArchs.IsEmpty()) {
      mBinaryArchs.Append('-');
    }
    mBinaryArchs.AppendLiteral("i386");
  }

  if (foundPPC64) {
    if (!mBinaryArchs.IsEmpty()) {
      mBinaryArchs.Append('-');
    }
    mBinaryArchs.AppendLiteral("ppc64");
  }

  if (foundX86_64) {
    if (!mBinaryArchs.IsEmpty()) {
      mBinaryArchs.Append('-');
    }
    mBinaryArchs.AppendLiteral("x86_64");
  }

  aArchString.Assign(mBinaryArchs);

  return (aArchString.IsEmpty() ? NS_ERROR_FAILURE : NS_OK);
}

NS_IMETHODIMP
nsMacUtilsImpl::GetArchitecturesInBinary(nsAString& aArchString) {
  return GetArchString(aArchString);
}

// True when running under binary translation (Rosetta).
NS_IMETHODIMP
nsMacUtilsImpl::GetIsTranslated(bool* aIsTranslated) {
#ifdef __ppc__
  static bool sInitialized = false;

  // Initialize sIsNative to 1.  If the sysctl fails because it doesn't
  // exist, then translation is not possible, so the process must not be
  // running translated.
  static int32_t sIsNative = 1;

  if (!sInitialized) {
    size_t sz = sizeof(sIsNative);
    sysctlbyname("sysctl.proc_native", &sIsNative, &sz, nullptr, 0);
    sInitialized = true;
  }

  *aIsTranslated = !sIsNative;
#else
  // Translation only exists for ppc code.  Other architectures aren't
  // translated.
  *aIsTranslated = false;
#endif

  return NS_OK;
}

#if defined(MOZ_SANDBOX)
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
  auto pattern = NS_LITERAL_CSTRING("/Contents/MacOS/");
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
    ClearOnShutdown(&sCachedAppPath);
  }

  return true;
}

#  if defined(DEBUG)
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
#  endif /* DEBUG */
#endif   /* MOZ_SANDBOX */

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

/* static */
nsresult nsMacUtilsImpl::EnableTCSM() {
  uint32_t newVal = 1;
  int rv = sysctlbyname("kern.tcsm_enable", NULL, 0, &newVal, sizeof(newVal));
  if (rv < 0) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

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

#if defined(DEBUG)
/* static */
bool nsMacUtilsImpl::IsTCSMEnabled() {
  uint32_t oldVal = 0;
  size_t oldValSize = sizeof(oldVal);
  int rv = sysctlbyname("kern.tcsm_enable", &oldVal, &oldValSize, NULL, 0);
  return (rv == 0) && (oldVal != 0);
}
#endif

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
