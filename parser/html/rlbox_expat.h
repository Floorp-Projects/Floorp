/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PARSER_HTML_RLBOX_EXPAT_H_
#define PARSER_HTML_RLBOX_EXPAT_H_

#include "rlbox_expat_types.h"

// Load general firefox configuration of RLBox
#include "mozilla/rlbox/rlbox_config.h"

#ifdef MOZ_WASM_SANDBOXING_EXPAT
#  include "mozilla/rlbox/rlbox_wasm2c_sandbox.hpp"
#else
// Extra configuration for no-op sandbox
#  define RLBOX_USE_STATIC_CALLS() rlbox_noop_sandbox_lookup_symbol
#  include "mozilla/rlbox/rlbox_noop_sandbox.hpp"
#endif

#include "mozilla/rlbox/rlbox.hpp"

/* status_verifier is a type validator for XML_Status */
inline enum XML_Status status_verifier(enum XML_Status s) {
  MOZ_RELEASE_ASSERT(s >= XML_STATUS_ERROR && s <= XML_STATUS_SUSPENDED,
                     "unexpected status code");
  return s;
}

/* error_verifier is a type validator for XML_Error */
inline enum XML_Error error_verifier(enum XML_Error code) {
  MOZ_RELEASE_ASSERT(
      code >= XML_ERROR_NONE && code <= XML_ERROR_INVALID_ARGUMENT,
      "unexpected XML error code");
  return code;
}

/*
 * transfer_xmlstring_no_validate is used to transfer an XML_Char* from the
 * sandboxed expat. This function doesn't validate the transfered string; it
 * only null-terminates the string.
 *
 * Strings should be validated whenever possible (Bug 1693991 tracks this).
 */
inline XML_Char* transfer_xmlstring_no_validate(
    rlbox_sandbox_expat& aSandbox, tainted_expat<const XML_Char*> t_aStr,
    bool& copied) {
  copied = false;
  if (t_aStr != nullptr) {
    const mozilla::CheckedInt<size_t> len = std::char_traits<char16_t>::length(
        t_aStr.unverified_safe_pointer_because(
            1,
            "Even with bad length, we will not span beyond sandbox boundary."));

    // Compute size to copy/allocate
    const mozilla::CheckedInt<size_t> strSize = (len + 1) * sizeof(char16_t);

    MOZ_RELEASE_ASSERT(len.isValid() && strSize.isValid(), "Bad string");

    // Actually transfer string
    auto* aStr = rlbox::copy_memory_or_deny_access(
        aSandbox, rlbox::sandbox_const_cast<char16_t*>(t_aStr), strSize.value(),
        false, copied);
    // Ensure the string is null padded
    aStr[len.value()] = '\0';
    return aStr;
  }
  return nullptr;
}

#endif
