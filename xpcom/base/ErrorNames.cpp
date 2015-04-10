/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/ErrorNames.h"
#include "nsString.h"
#include "prerror.h"

namespace {

struct ErrorEntry
{
  nsresult value;
  const char * name;
};

#undef ERROR
#define ERROR(key, val) {key, #key}

const ErrorEntry errors[] = {
  #include "ErrorList.h"
};

#undef ERROR

} // unnamed namespace

namespace mozilla {

void
GetErrorName(nsresult rv, nsACString& name)
{
  for (size_t i = 0; i < ArrayLength(errors); ++i) {
    if (errors[i].value == rv) {
      name.AssignASCII(errors[i].name);
      return;
    }
  }

  bool isSecurityError = NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_SECURITY;

  // NS_ERROR_MODULE_SECURITY is the only module that is "allowed" to
  // synthesize nsresult error codes that are not listed in ErrorList.h. (The
  // NS_ERROR_MODULE_SECURITY error codes are synthesized from NSPR error
  // codes.)
  MOZ_ASSERT(isSecurityError);

  name.AssignASCII(NS_SUCCEEDED(rv) ? "NS_ERROR_GENERATE_SUCCESS("
                                    : "NS_ERROR_GENERATE_FAILURE(");

  if (isSecurityError) {
    name.AppendASCII("NS_ERROR_MODULE_SECURITY");
  } else {
    // This should never happen given the assertion above, so we don't bother
    // trying to print a symbolic name for the module here.
    name.AppendInt(NS_ERROR_GET_MODULE(rv));
  }

  name.AppendASCII(", ");

  const char * nsprName = nullptr;
  if (isSecurityError) {
    // Invert the logic from NSSErrorsService::GetXPCOMFromNSSError
    PRErrorCode nsprCode
      = -1 * static_cast<PRErrorCode>(NS_ERROR_GET_CODE(rv));
    nsprName = PR_ErrorToName(nsprCode);

    // All NSPR error codes defined by NSPR or NSS should have a name mapping.
    MOZ_ASSERT(nsprName);
  }

  if (nsprName) {
    name.AppendASCII(nsprName);
  } else {
    name.AppendInt(NS_ERROR_GET_CODE(rv));
  }

  name.AppendASCII(")");
}

} // namespace mozilla
