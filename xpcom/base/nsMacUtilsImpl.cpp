/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is XPCOM utility functions for Mac OS X
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Mentovai <mark@moxienet.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMacUtilsImpl.h"

#include <CoreFoundation/CoreFoundation.h>
#include <sys/sysctl.h>

NS_IMPL_ISUPPORTS1(nsMacUtilsImpl, nsIMacUtils)

nsresult nsMacUtilsImpl::GetArchString(nsAString& archString)
{
  if (!mBinaryArchs.IsEmpty()) {
    archString.Assign(mBinaryArchs);
    return NS_OK;
  }

  archString.Truncate();

  bool foundPPC = false,
         foundX86 = PR_FALSE,
         foundPPC64 = PR_FALSE,
         foundX86_64 = PR_FALSE;

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
      foundPPC = PR_TRUE;
    else if (archInt == kCFBundleExecutableArchitectureI386)
      foundX86 = PR_TRUE;
    else if (archInt == kCFBundleExecutableArchitecturePPC64)
      foundPPC64 = PR_TRUE;
    else if (archInt == kCFBundleExecutableArchitectureX86_64)
      foundX86_64 = PR_TRUE;
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
  *aIsUniversalBinary = PR_FALSE;

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
  static PRInt32 sIsNative = 1;

  if (!sInitialized) {
    size_t sz = sizeof(sIsNative);
    sysctlbyname("sysctl.proc_native", &sIsNative, &sz, NULL, 0);
    sInitialized = PR_TRUE;
  }

  *aIsTranslated = !sIsNative;
#else
  // Translation only exists for ppc code.  Other architectures aren't
  // translated.
  *aIsTranslated = PR_FALSE;
#endif

  return NS_OK;
}
