/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCertHelper.h"

#include <algorithm>

#include "ScopedNSSTypes.h"
#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/NotNull.h"
#include "mozilla/Sprintf.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Utf8.h"
#include "mozilla/net/DNS.h"
#include "nsCOMPtr.h"
#include "nsIStringBundle.h"
#include "nsNSSCertificate.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "prerror.h"
#include "prnetdb.h"
#include "secder.h"

using namespace mozilla;

// To avoid relying on localized strings in PSM, we hard-code the root module
// name internally. When we display it to the user in the list of modules in the
// front-end, we look up the localized value and display that instead of this.
const char* kRootModuleName = "Builtin Roots Module";
const size_t kRootModuleNameLen = strlen(kRootModuleName);

static nsresult GetPIPNSSBundle(nsIStringBundle** pipnssBundle) {
  nsCOMPtr<nsIStringBundleService> bundleService(
      do_GetService(NS_STRINGBUNDLE_CONTRACTID));
  if (!bundleService) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return bundleService->CreateBundle("chrome://pipnss/locale/pipnss.properties",
                                     pipnssBundle);
}

nsresult GetPIPNSSBundleString(const char* stringName, nsAString& result) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }
  MOZ_ASSERT(stringName);
  if (!stringName) {
    return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<nsIStringBundle> pipnssBundle;
  nsresult rv = GetPIPNSSBundle(getter_AddRefs(pipnssBundle));
  if (NS_FAILED(rv)) {
    return rv;
  }
  result.Truncate();
  return pipnssBundle->GetStringFromName(stringName, result);
}

nsresult GetPIPNSSBundleString(const char* stringName, nsACString& result) {
  nsAutoString tmp;
  nsresult rv = GetPIPNSSBundleString(stringName, tmp);
  if (NS_FAILED(rv)) {
    return rv;
  }
  result.Assign(NS_ConvertUTF16toUTF8(tmp));
  return NS_OK;
}

nsresult PIPBundleFormatStringFromName(const char* stringName,
                                       const nsTArray<nsString>& params,
                                       nsAString& result) {
  MOZ_ASSERT(stringName);
  MOZ_ASSERT(!params.IsEmpty());
  if (!stringName || params.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<nsIStringBundle> pipnssBundle;
  nsresult rv = GetPIPNSSBundle(getter_AddRefs(pipnssBundle));
  if (NS_FAILED(rv)) {
    return rv;
  }
  result.Truncate();
  return pipnssBundle->FormatStringFromName(stringName, params, result);
}

void LossyUTF8ToUTF16(const char* str, uint32_t len,
                      /*out*/ nsAString& result) {
  auto span = Span(str, len);
  if (IsUtf8(span)) {
    CopyUTF8toUTF16(span, result);
  } else {
    // Actually Latin1 despite ASCII in the legacy name
    CopyASCIItoUTF16(span, result);
  }
}
