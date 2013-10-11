/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMacUtilsImpl.h"

#include <CoreFoundation/CoreFoundation.h>

NS_IMPL_ISUPPORTS1(nsMacUtilsImpl, nsIMacUtils)

nsresult nsMacUtilsImpl::GetArchString(nsAString& archString)
{
  if (!mBinaryArchs.IsEmpty()) {
    archString.Assign(mBinaryArchs);
    return NS_OK;
  }

  archString.Truncate();

  bool foundPPC = false,
         foundX86 = false,
         foundPPC64 = false,
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
    CFNumberRef arch = static_cast<CFNumberRef>(::CFArrayGetValueAtIndex(archList, i));

    int archInt = 0;
    if (!::CFNumberGetValue(arch, kCFNumberIntType, &archInt)) {
      ::CFRelease(archList);
      return NS_ERROR_FAILURE;
    }

    if (archInt == kCFBundleExecutableArchitecturePPC)
      foundPPC = true;
    else if (archInt == kCFBundleExecutableArchitectureI386)
      foundX86 = true;
    else if (archInt == kCFBundleExecutableArchitecturePPC64)
      foundPPC64 = true;
    else if (archInt == kCFBundleExecutableArchitectureX86_64)
      foundX86_64 = true;
  }

  ::CFRelease(archList);

  // The order in the string must always be the same so
  // don't do this in the loop.
  if (foundPPC) {
    mBinaryArchs.Append(NS_LITERAL_STRING("ppc"));
  }

  if (foundX86) {
    if (!mBinaryArchs.IsEmpty()) {
      mBinaryArchs.Append(NS_LITERAL_STRING("-"));
    }
    mBinaryArchs.Append(NS_LITERAL_STRING("i386"));
  }

  if (foundPPC64) {
    if (!mBinaryArchs.IsEmpty()) {
      mBinaryArchs.Append(NS_LITERAL_STRING("-"));
    }
    mBinaryArchs.Append(NS_LITERAL_STRING("ppc64"));
  }

  if (foundX86_64) {
    if (!mBinaryArchs.IsEmpty()) {
      mBinaryArchs.Append(NS_LITERAL_STRING("-"));
    }
    mBinaryArchs.Append(NS_LITERAL_STRING("x86_64"));
  }

  archString.Assign(mBinaryArchs);

  return (archString.IsEmpty() ? NS_ERROR_FAILURE : NS_OK);
}

NS_IMETHODIMP nsMacUtilsImpl::GetIsUniversalBinary(bool *aIsUniversalBinary)
{
  NS_ENSURE_ARG_POINTER(aIsUniversalBinary);
  *aIsUniversalBinary = false;

  nsAutoString archString;
  nsresult rv = GetArchString(archString);
  if (NS_FAILED(rv))
    return rv;

  // The delimiter char in the arch string is '-', so if that character
  // is in the string we know we have multiple architectures.
  *aIsUniversalBinary = (archString.Find("-") > -1);

  return NS_OK;
}

NS_IMETHODIMP nsMacUtilsImpl::GetArchitecturesInBinary(nsAString& archString)
{
  return GetArchString(archString);
}

/* readonly attribute boolean isTranslated; */
// True when running under binary translation (Rosetta).
NS_IMETHODIMP nsMacUtilsImpl::GetIsTranslated(bool *aIsTranslated)
{
#ifdef __ppc__
  static bool    sInitialized = false;

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
