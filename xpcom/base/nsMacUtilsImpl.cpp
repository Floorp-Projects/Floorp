/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMacUtilsImpl.h"

#include "base/command_line.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsIProperties.h"
#include "nsServiceManagerUtils.h"
#include "nsXULAppAPI.h"

#include <CoreFoundation/CoreFoundation.h>

NS_IMPL_ISUPPORTS(nsMacUtilsImpl, nsIMacUtils)

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

#if defined(MOZ_CONTENT_SANDBOX)
// Get the path to the .app directory (aka bundle) for the parent process.
// When executing in the child process, this is the outer .app (such as
// Firefox.app) and not the inner .app containing the child process
// executable. We don't rely on the actual .app extension to allow for the
// bundle being renamed.
bool nsMacUtilsImpl::GetAppPath(nsCString& aAppPath) {
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

  return true;
}

#  if defined(DEBUG)
// Given a path to a file, return the directory which contains it.
nsAutoCString nsMacUtilsImpl::GetDirectoryPath(const char* aPath) {
  nsCOMPtr<nsIFile> file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  if (!file || NS_FAILED(file->InitWithNativePath(nsDependentCString(aPath)))) {
    MOZ_CRASH("Failed to create or init an nsIFile");
  }
  nsCOMPtr<nsIFile> directoryFile;
  if (NS_FAILED(file->GetParent(getter_AddRefs(directoryFile))) ||
      !directoryFile) {
    MOZ_CRASH("Failed to get parent for an nsIFile");
  }
  directoryFile->Normalize();
  nsAutoCString directoryPath;
  if (NS_FAILED(directoryFile->GetNativePath(directoryPath))) {
    MOZ_CRASH("Failed to get path for an nsIFile");
  }
  return directoryPath;
}
#  endif /* DEBUG */
#endif   /* MOZ_CONTENT_SANDBOX */
