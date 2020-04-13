/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/ErrorNames.h"
#include "nsString.h"
#include "prerror.h"

// Get the GetErrorNameInternal method
#include "ErrorNamesInternal.h"

namespace mozilla {

const char* GetStaticErrorName(nsresult rv) { return GetErrorNameInternal(rv); }

void GetErrorName(nsresult rv, nsACString& name) {
  if (const char* errorName = GetErrorNameInternal(rv)) {
    name.AssignASCII(errorName);
    return;
  }

  bool isSecurityError = NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_SECURITY;

  // NS_ERROR_MODULE_SECURITY is the only module that is "allowed" to
  // synthesize nsresult error codes that are not listed in ErrorList.h. (The
  // NS_ERROR_MODULE_SECURITY error codes are synthesized from NSPR error
  // codes.)
  MOZ_ASSERT(isSecurityError);

  if (NS_SUCCEEDED(rv)) {
    name.AssignLiteral("NS_ERROR_GENERATE_SUCCESS(");
  } else {
    name.AssignLiteral("NS_ERROR_GENERATE_FAILURE(");
  }

  if (isSecurityError) {
    name.AppendLiteral("NS_ERROR_MODULE_SECURITY");
  } else {
    // This should never happen given the assertion above, so we don't bother
    // trying to print a symbolic name for the module here.
    name.AppendInt(NS_ERROR_GET_MODULE(rv));
  }

  name.AppendLiteral(", ");

  const char* nsprName = nullptr;
  if (isSecurityError) {
    // Invert the logic from NSSErrorsService::GetXPCOMFromNSSError
    PRErrorCode nsprCode = -1 * static_cast<PRErrorCode>(NS_ERROR_GET_CODE(rv));
    nsprName = PR_ErrorToName(nsprCode);

    // All NSPR error codes defined by NSPR or NSS should have a name mapping.
    MOZ_ASSERT(nsprName);
  }

  if (nsprName) {
    name.AppendASCII(nsprName);
  } else {
    name.AppendInt(NS_ERROR_GET_CODE(rv));
  }

  name.AppendLiteral(")");
}

}  // namespace mozilla

extern "C" {

// This is an extern "C" binding for the GetErrorName method which is used by
// the nsresult rust bindings in xpcom/rust/nserror.
void Gecko_GetErrorName(nsresult aRv, nsACString& aName) {
  mozilla::GetErrorName(aRv, aName);
}
}
